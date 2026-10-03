#include "hardware.h"
#include "command_queue.h"
#include "engine.h"
#include "fatfs_storage.h"
#include "midi_framer.h"
#include "preset_menu.h"
#include "runtime.h"
#include "sampler_runtime.h"
#include "usb_packets.h"
#include "usbd_cdc.h"
#include "usbd_cdc_if.h"
#include "util/CpuLoadMeter.h"

extern "C" { extern USBD_HandleTypeDef hUsbDeviceHS; }

using namespace daisy;
using namespace chompi;

namespace {
Hardware hw;
template<typename Transport> struct MidiPort {
    Transport transport;
    forge::MidiFramer framer;
    forge::SpscQueue<forge::MidiFrame, 16> frames;
    std::atomic<uint32_t> dropped{0};
    static void Receive(uint8_t* data, size_t size, void* context) {
        auto& self = *static_cast<MidiPort*>(context);
        forge::MidiFrame frame;
        for(size_t i = 0; i < size; ++i)
            if(self.framer.Feed(data[i], frame) && !self.frames.Push(frame))
                self.dropped.fetch_add(1, std::memory_order_relaxed);
    }
    void Listen() {
        if(!transport.RxActive()) {
            framer.Reset(); transport.FlushRx();
            transport.StartRx(Receive, this);
        }
    }
};
MidiPort<MidiUartTransport> uart_midi;
MidiPort<MidiUsbTransport> usb_midi;
CpuLoadMeter cpu;
UsbHandle usb_sender;
// libDaisy exposes the configured device and CDC state used by its MIDI mode.
// Keep TX memory alive until USB completion, and never rewrite it while busy.
// Largest reply (v4 status, 98 bytes with F0/F7) packs into 33 USB-MIDI events.
constexpr size_t kMaxEnvelope = forge::kMaxReply + 2;
uint8_t usb_tx_packets[((kMaxEnvelope + 2) / 3) * 4];
struct Outgoing { uint8_t source = 0; uint8_t bytes[kMaxEnvelope]{}; size_t size = 0; };
forge::SpscQueue<Outgoing, 32> outgoing; // producer and consumer both main loop
Outgoing pending;
bool has_pending = false;
uint32_t pending_since = 0;
forge::Engine engine;
// Emergency count: main loop is the only writer; audio reads it each block.
std::atomic<uint32_t> emergency_epoch{0};
bool discard_ingress = false; // main-loop owned
forge::SpscQueue<forge::Request, 64> requests;
forge::SpscQueue<forge::Response, 64> responses;
// The bootloader does not zero SDRAM; Engine::Init clears these before audio.
constexpr size_t kDelayCapacity = 48002;
float DSY_SDRAM_BSS delay_left[kDelayCapacity];
float DSY_SDRAM_BSS delay_right[kDelayCapacity];
constexpr size_t kReverbCapacity = 8704; // >= Reverb::Required(48000) = 8606
// 34 KB in DTCM (zero-wait, uncached), as TAPE/TEMPO/WAVE place their reverbs.
// The FDN lines exceed the 16 KB D-cache, so SDRAM would mean cache misses.
// Not zeroed at boot; Reverb's unread counter hides stale cells until rewritten.
float __attribute__((section(".dtcmram_bss"))) reverb_memory[kReverbCapacity];
uint32_t dropped_commands = 0, rejected_messages = 0; // main-loop owned

// Device presets on the SD card (main loop only) and the TAPE-style panel menu
// (audio owner). Menu actions cross to the main loop with a parameter snapshot.
SdmmcHandler sdmmc;
FatFSInterface fsi;
FatFsStorage card;
forge::PresetStore store(card);
struct PanelAction { forge::MenuAction action; forge::Parameters patch; };
forge::SpscQueue<PanelAction, 8> panel_actions;   // audio -> main
std::atomic<uint32_t> menu_state{0};               // PresetMenu::Packed(), for LEDs
uint8_t last_bank = 0, last_slot = forge::panel::kNoSlot; // main-loop owned
uint32_t flash_until = 0; bool flash_ok = true;   // panel LED feedback after SD actions
std::atomic<uint32_t> audio_flash{0};              // audio -> main: 1 = ok, 2 = failed (record refused)

// Sampler (docs/forge/SAMPLING.md). SDRAM is not zeroed at boot; every read is
// bounded by a slot's published `loaded` count, so stale memory never plays.
constexpr uint32_t kPoolSamples = 40u * 1024 * 1024 / 2;      // 40 MB: chromatic sample or kit bank
constexpr uint32_t kRecordFrames = 16u * 1024 * 1024 / 4;     // 16 MB: ~87 s stereo at 48 kHz
int16_t DSY_SDRAM_BSS sample_pool[kPoolSamples];
int16_t DSY_SDRAM_BSS record_memory[2 * kRecordFrames];
uint8_t __attribute__((aligned(32))) sample_scratch[16384];   // D1 SRAM: reachable by SD DMA
forge::SampleTable sample_table;
forge::SampleHandoff sample_handoff;
forge::SampleLoader sample_loader;                            // main loop
forge::Recorder recorder;                                     // audio owner (Unlock: main)
FatFsSampleFiles sample_files(card);
std::atomic<uint32_t> sample_wanted{0};                      // audio -> main: PackSelection of the live patch
std::atomic<bool> recording_now{false};                       // audio -> main, for the CHOMPI LED
forge::SpscQueue<forge::SampleJob, 4> sample_jobs;            // audio -> main: panel save/erase/copy

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    cpu.OnBlockStart();
    forge::Request request;
    static forge::RecoveryGate recovery; // audio-owner only
    recovery.Observe(static_cast<uint8_t>(emergency_epoch.load(std::memory_order_acquire)), engine);
    // Backpressure instead of applying a patch whose acknowledgement cannot
    // be queued. Only this audio callback produces responses.
    // Sampler memory handoff: refuse file-slot notes while the loader rewrites them.
    engine.SetSampleFilesAvailable(sample_handoff.AudioBlock(engine));
    for(unsigned i = 0; i < 16 && responses.HasSpace() && requests.Pop(request); ++i) {
        if(!recovery.Admit(request, engine)) continue;
        forge::Response response;
        if(request.kind == forge::RequestKind::SampleJob) {     // host save: lock the take first
            responses.Push(forge::LockForSave(recorder, request));
            continue;
        }
        if(forge::ExecuteRequest(request, engine, response)) {
            response.cpu_average = cpu.GetAvgCpuLoad();
            response.cpu_max = cpu.GetMaxCpuLoad();
            responses.Push(response);
        }
    }

    hw.ProcessAllControls();
    // Toggle + CHOMPI key: TAPE-style preset menu. While it is open, keys select
    // presets instead of playing (releases still end held notes).
    static forge::PresetMenu menu;
    static forge::RecordGesture gesture;
    static forge::RecordSource source = forge::RecordSource::Line;
    const bool toggle_up = hw.GetToggleState(), chompi = hw.button_sr.State(forge::panel::kChompiKey);
    menu.Update(toggle_up, chompi);
    // TAPE: jack insertion selects line in, removal the mic.
    static bool jack = !hw.jack_detect.Read();
    if(hw.jack_detect.Read() != jack) {
        jack = hw.jack_detect.Read();
        source = jack ? forge::RecordSource::Line : forge::RecordSource::Mic;
        menu.SetRecordSource(static_cast<uint8_t>(source));
    }
    // TAPE: toggle down + hold CHOMPI records; on release the take becomes the
    // chromatic recording slot and plays at once.
    switch(gesture.Update(toggle_up, chompi)) {
        case forge::RecordGesture::Event::Start:
            if(!recorder.Start()) { gesture.Cancel(); audio_flash.store(2, std::memory_order_relaxed); }
            break;
        case forge::RecordGesture::Event::Stop: {
            recorder.Stop();
            const auto p = engine.GetParameters();
            engine.ApplyPatch(forge::SelectSample(p, 0, p.sample_bank, forge::kRamSlot));
            break;
        }
        default: break;
    }
    // Matches upstream NormalPage::key_map: 25 chromatic keys, MIDI 48..72.
    static constexpr uint8_t notes[40] = {0,0,0,0,0,0,0,49,50,52,53,55,51,54,56,48,
        57,59,60,62,64,58,61,63,65,67,69,71,72,66,68,70,0,0,0,0,0,0,0,0};
    for(unsigned key = 0; key < 40; ++key) if(notes[key]) {
        if(hw.button_sr.RisingEdge(key) && !menu.Key(key, true)) engine.Note(notes[key], 100, 2);
        if(hw.button_sr.FallingEdge(key)) { menu.Key(key, false); engine.Note(notes[key], 0, 2); }
    }
    for(forge::MenuAction action; menu.PopAction(action);) {
        using Kind = forge::MenuAction::Kind;
        if(action.kind == Kind::SampleSelect) {                 // applied here, as TAPE does
            engine.ApplyPatch(forge::SelectSample(engine.GetParameters(), action.mode, action.bank, action.slot));
        } else if(action.kind == Kind::RecordSource) {
            source = static_cast<forge::RecordSource>(action.slot);
        } else if(action.kind == Kind::SampleSave || action.kind == Kind::SampleErase || action.kind == Kind::SampleCopy) {
            forge::SampleJob job;
            job.mode = action.mode; job.bank = action.bank; job.slot = action.slot;
            job.to_mode = action.to_mode; job.to_bank = action.to_bank; job.to_slot = action.to_slot;
            job.kind = action.kind == Kind::SampleSave ? forge::SampleJob::Kind::Save
                     : action.kind == Kind::SampleErase ? forge::SampleJob::Kind::Erase : forge::SampleJob::Kind::Copy;
            if(job.kind == forge::SampleJob::Kind::Save) {
                if(!recorder.Lock()) { audio_flash.store(2, std::memory_order_relaxed); continue; }
                job.frames = recorder.Length(); job.gain = recorder.Gain();
            }
            if(!sample_jobs.Push(job)) {
                if(job.kind == forge::SampleJob::Kind::Save) recorder.Unlock();
                audio_flash.store(2, std::memory_order_relaxed);
            }
        } else {
            panel_actions.Push({action, engine.GetParameters()});    // full queue: dropped, LEDs show no change
        }
    }
    {
        const auto& live = engine.GetParameters();
        if(live.Sampler()) menu.FollowSampler(live.sample_mode, live.sample_bank, live.sample_slot);
        sample_wanted.store(forge::PackSelection(live), std::memory_order_relaxed);
    }
    recording_now.store(recorder.Recording(), std::memory_order_relaxed);
    menu_state.store(menu.Packed(), std::memory_order_relaxed);
    // Dedicated local recovery; SW5 turn controls synth tone, press silences.
    if(hw.enc[4].RisingEdge()) engine.Panic();
    const int tone = hw.enc[4].Increment();
    if(tone) engine.Apply({forge::Parameter::Cutoff, engine.GetParameters().cutoff + tone / 127.f});
    // Logical knobs in stock order (knob n = CC20+n), mapped to hardware encoders.
    // Their functions follow the patch: mix/time/feedback/level, or TAPE's
    // sampler page (pitch, start, end, mix).
    for(unsigned knob = 0; knob < 4; ++knob) {
        const int increment = hw.enc[forge::panel::kKnobEncoder[knob]].Increment();
        if(increment && !menu.Encoder(knob, increment)) {   // knob 0 picks the bank while the menu is open
            const auto& p = engine.GetParameters();
            const float value = p.Value(p.KnobParameter(knob));
            engine.Apply({static_cast<forge::Parameter>(static_cast<unsigned>(forge::Parameter::Knob1) + knob),
                          value + increment / 127.f});
        }
    }
    // Physical volume encoder SW6 also controls output level.
    const int volume = hw.enc[5].Increment();
    if(volume) engine.Apply({forge::Parameter::Level, engine.GetParameters().level + volume / 127.f});

    const bool recording = recorder.Recording();
    for(size_t i = 0; i < size; ++i) {
        float left, right;
        // Upstream channel map: mic 0, aux L/R = 2/3 (aux feeds the delay route).
        engine.Process(in[2][i], in[3][i], left, right);
        if(recording) {
            float rec_l, rec_r;
            recorder.Input(source, in[0][i], in[2][i], in[3][i], left, right, rec_l, rec_r);
            recorder.Write(rec_l, rec_r);
            if(source != forge::RecordSource::Resample) {      // monitor what is recorded (as TAPE)
                left = forge::Clamp(left + 0.5f * rec_l, -1.f, 1.f);
                right = forge::Clamp(right + 0.5f * rec_r, -1.f, 1.f);
            }
        }
        out[0][i] = out[2][i] = left;
        out[1][i] = out[3][i] = right;
    }
    cpu.OnBlockEnd();
}

void Send(uint8_t source, uint8_t* envelope, size_t payload_size) {
    Outgoing message;
    message.source = source; message.size = payload_size + 2;
    envelope[0] = 0xf0; envelope[payload_size + 1] = 0xf7;
    std::copy(envelope, envelope + message.size, message.bytes);
    if(!outgoing.Push(message)) ++dropped_commands;
}

void TransmitPending() {
    if(!has_pending) {
        if(!outgoing.Pop(pending)) return;
        has_pending = true; pending_since = System::GetNow();
    }
    bool sent = false;
    if(pending.source == 0) {
        // 0.32 ms per byte at 31250 baud (83-byte v3 reply: 26.6 ms) plus 5 ms
        // margin; the upstream PollTx wrapper's 10 ms timeout is too short.
        // Audio remains interrupt-driven while this blocks the main loop.
        const uint32_t timeout_ms = static_cast<uint32_t>((pending.size * 320 + 999) / 1000 + 5);
        sent = uart_midi.transport.GetUartHandle().BlockingTransmit(pending.bytes, pending.size, timeout_ms)
            == UartHandler::Result::OK;
        if(!sent) { ++dropped_commands; has_pending = false; }
    } else {
        ScopedIrqBlocker guard;
        if(hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceHS.pClassData) {
            auto* cdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceHS.pClassData);
            if(cdc->TxState == 0) {
                const size_t size = forge::PackUsbSysEx(pending.bytes, pending.size,
                                                       usb_tx_packets, sizeof(usb_tx_packets));
                sent = size && usb_sender.TransmitExternal(usb_tx_packets, size) == UsbHandle::Result::OK;
            }
        }
    }
    if(sent) has_pending = false;
    else if(System::GetNow() - pending_since > 100) {
        ++dropped_commands; has_pending = false;
    }
}

void RaiseEmergency() {
    emergency_epoch.store(emergency_epoch.load(std::memory_order_relaxed) + 1, std::memory_order_release);
}
bool Queue(forge::Request& request) {
    request.epoch = static_cast<uint8_t>(emergency_epoch.load(std::memory_order_relaxed));
    return requests.Push(request);
}

void SendResponse(const forge::Response& response) {
    const uint32_t dropped = dropped_commands + uart_midi.dropped.load(std::memory_order_relaxed)
        + usb_midi.dropped.load(std::memory_order_relaxed);
    uint8_t envelope[kMaxEnvelope];
    const size_t size = forge::EncodeResponse(response, dropped, rejected_messages, envelope + 1);
    Send(response.source, envelope, size);
}
void Flash(bool ok) { flash_ok = ok; flash_until = System::GetNow() + 400; }
// Loads a slot and queues it for the audio owner. Silent requests (panel,
// program change) get no reply; a host recall is acknowledged with status.
forge::Error RecallPreset(forge::Request& request) {
    forge::Request apply;
    const forge::Error error = forge::RecallRequest(store, request, apply);
    if(error != forge::Error::None) { if(request.silent) Flash(false); return error; }
    if(!Queue(apply)) { ++dropped_commands; return forge::Error::Busy; }
    last_bank = request.bank; last_slot = request.slot;
    return forge::Error::None;
}
// Host device-preset requests. Store goes on to the audio owner for a
// snapshot; recall is replaced by the loaded patch; erase/list reply here.
forge::Error HandleStorage(forge::Request& request) {
    using forge::RequestKind;
    if(request.kind == RequestKind::Recall) {
        forge::Request apply;
        const forge::Error error = forge::RecallRequest(store, request, apply);
        if(error == forge::Error::None) { last_bank = request.bank; last_slot = request.slot; request = apply; }
        return error;
    }
    if(request.kind == RequestKind::Erase) {
        const forge::Response reply = forge::EraseReply(store, request);
        if(reply.error == forge::Error::None) SendResponse(reply);
        return reply.error;
    }
    if(request.kind == RequestKind::List) {
        const forge::Response reply = forge::ListReply(store, request);
        if(reply.error == forge::Error::None) SendResponse(reply);
        return reply.error;
    }
    return forge::Error::None;
}
// Panel menu actions (queued by the audio callback).
void RunPanelActions() {
    PanelAction item;
    while(panel_actions.Pop(item)) {
        const forge::MenuAction& a = item.action;
        forge::Error error = forge::Error::None;
        switch(a.kind) {
            case forge::MenuAction::Kind::Recall: {
                forge::Request request; request.kind = forge::RequestKind::Recall;
                request.bank = a.bank; request.slot = a.slot; request.silent = true;
                RecallPreset(request);
                continue;   // RecallPreset flashes on failure; success shows as the white key
            }
            case forge::MenuAction::Kind::Save: error = store.Save(a.bank, a.slot, item.patch); break;
            case forge::MenuAction::Kind::Erase: error = store.Erase(a.bank, a.slot); break;
            case forge::MenuAction::Kind::Copy: error = store.Copy(a.bank, a.slot, a.to_bank, a.to_slot); break;
            default: continue;   // sample actions never reach this queue (handled in the audio callback)
        }
        Flash(error == forge::Error::None);
    }
}
// Sampler main-loop work: panel jobs, one loader step, job replies.
void RunSampler() {
    for(forge::SampleJob job; sample_jobs.Pop(job);) {
        job.source = 0xff;
        if(!sample_loader.Queue(job)) { if(job.kind == forge::SampleJob::Kind::Save) recorder.Unlock(); Flash(false); }
    }
    forge::SampleEvent event;
    if(sample_loader.Poll(sample_files, sample_wanted.load(std::memory_order_relaxed), record_memory, event)) {
        if(event.job.kind == forge::SampleJob::Kind::Save) recorder.Unlock();
        if(event.job.source == 0xff) Flash(event.ok);
        else {
            const forge::Response reply = forge::SampleDoneReply(event);
            if(reply.error != forge::Error::None) ++rejected_messages;
            SendResponse(reply);
        }
    }
    const uint32_t flash = audio_flash.exchange(0, std::memory_order_relaxed);
    if(flash) Flash(flash == 1);
}
// Key LEDs while the menu is open (~30 Hz); panel LED 0 (the CHOMPI key, as in
// TAPE) shows status: red while recording, pink blink while saving/copying.
void DrawLeds() {
    static uint32_t last_draw = 0; static bool was_open = false;
    const uint32_t now = System::GetNow();
    if(now - last_draw < 33) return;
    last_draw = now;
    const uint32_t packed = menu_state.load(std::memory_order_relaxed);
    const bool open = packed & 1u;
    if(open || was_open) {
        forge::Rgb leds[25];
        if((packed >> 21) & 1u)
            forge::RenderSampleLeds(packed, sample_loader.Occupancy((packed >> 22) & 1u, (packed >> 4) & 7u), sample_files.Ready(),
                                    sample_table.slots[forge::kRamSlot].loaded.load(std::memory_order_acquire) > 0,
                                    sample_wanted.load(std::memory_order_relaxed), (now / 250) % 2 == 0, leds);
        else
            forge::RenderMenuLeds(packed, store.Occupancy((packed >> 4) & 7u), store.Ready(), last_bank, last_slot,
                                  (now / 250) % 2 == 0, leds);
        for(unsigned i = 0; i < 25; ++i) SetSmtLedFloat(i, leds[i].r, leds[i].g, leds[i].b);
    }
    was_open = open;
    if(recording_now.load(std::memory_order_relaxed)) SetPthLedFloat(0, 1.f, 0.f, 0.f);
    else if(now < flash_until) SetPthLedFloat(0, flash_ok ? 0.f : 0.3f, flash_ok ? 0.3f : 0.f, 0.f);
    else if(sample_loader.Busy() && !sample_loader.Loading()) SetPthLedFloat(0, (now / 300) % 2 ? 1.f : 0.f, 0.f, (now / 300) % 2 ? .6f : 0.f);
    else SetPthLedFloat(0, 0.f, 0.05f, 0.1f);
    fill_led_data();
}
// Card insert/remove: remount and rescan when the card comes back.
void WatchCard() {
    static uint32_t last_check = 0; static bool was_ready = false;
    const uint32_t now = System::GetNow();
    if(now - last_check < 1000) return;
    last_check = now;
    const bool present = disk_status(0) == RES_OK;
    if(present && !was_ready) {
        card.SetMounted(f_mount(&fsi.GetSDFileSystem(), fsi.GetSDPath(), 1) == FR_OK);
        store.Rescan();
    }
    was_ready = present && card.Ready();
}

// One received frame (shared by both transports, so the code exists once).
FORGE_NOINLINE void HandleFrame(const forge::MidiFrame& frame, uint8_t source) {
    forge::Request request;
    if(frame.kind != forge::MidiFrame::Kind::SysEx) {
        switch(forge::TranslateChannel(frame, source, request)) {
            case forge::Ingress::Emergency: RaiseEmergency(); break;
            case forge::Ingress::Critical:
                if(!Queue(request)) { ++dropped_commands; discard_ingress = true; RaiseEmergency(); }
                break;
            case forge::Ingress::Control: if(!Queue(request)) ++dropped_commands; break;
            case forge::Ingress::Storage: RecallPreset(request); break;   // program change
            case forge::Ingress::Ignore: break;
        }
        return;
    }
    if(!forge::IsRequest(frame.data, frame.size)) return;
    auto error = forge::DecodeRequest(frame.data, frame.size, request);
    request.source = source;
    if(error == forge::Error::None) error = HandleStorage(request);
    bool handled = request.kind == forge::RequestKind::Erase || request.kind == forge::RequestKind::List;
    if(error == forge::Error::None && request.kind == forge::RequestKind::SampleList) {
        SendResponse(forge::SampleListReply(sample_loader, sample_files.Ready(), sample_table.slots[forge::kRamSlot],
                                            kRecordFrames, request));
        handled = true;
    } else if(error == forge::Error::None && request.kind == forge::RequestKind::SampleJob
              && request.action != forge::SampleAction::Save) {   // save goes to the audio owner first
        forge::SampleJob job;
        error = forge::FileJob(sample_loader, request, job);
        if(error == forge::Error::None && !sample_loader.Queue(job)) error = forge::Error::StorageBusy;
        handled = true;
    }
    if(error == forge::Error::None && !handled && !Queue(request)) {
        ++dropped_commands; error = forge::Error::Busy;
    }
    if(error != forge::Error::None) {
        ++rejected_messages;
        uint8_t envelope[kMaxEnvelope];
        const size_t size = forge::EncodeError(forge::Read14(frame.data + 5), error, envelope + 1);
        Send(source, envelope, size);
    }
}
template<typename Midi> void PollMidi(Midi& midi, uint8_t source) {
    midi.Listen();
    forge::MidiFrame frame;
    for(unsigned i = 0; i < 8 && midi.frames.Pop(frame); ++i) HandleFrame(frame, source);
}

void SendResponses() {
    forge::Response response;
    for(unsigned i = 0; i < 4 && outgoing.HasSpace() && responses.Pop(response); ++i) {
        if(response.kind == forge::ResponseKind::SampleSnapshot) {   // host save: the take is locked
            if(!sample_loader.Queue(forge::SaveJob(response))) {
                recorder.Unlock();
                response.kind = forge::ResponseKind::SampleDone; response.error = forge::Error::StorageBusy;
                ++rejected_messages; SendResponse(response);
            }
            continue;
        }
        if(response.kind == forge::ResponseKind::Snapshot) {   // host store: write, then acknowledge
            response = forge::StoreReply(store, response.sequence, response.source, response.bank, response.slot,
                                         response.patch);
            if(response.error != forge::Error::None) ++rejected_messages;
        }
        SendResponse(response);
    }
}
} // namespace

int main() {
    hw.Init();
    LedSetup();
    hw.MpWrite(0x0c, 0B01010001); // retain upstream 3 V battery threshold
    hw.MpReadAll();
    for(unsigned i = 0; i < 10; ++i) {
        hw.LowBatteryLockoutCheck();
        System::Delay(10);
    }
    // Match the upstream charge-detection/USB switch sequence.
    hw.usb_sw.Write(false);
    System::Delay(1);
    hw.MpWrite(0x0a, 0B00100100);
    System::Delay(1);
    hw.usb_sw.Write(true);

    MidiUartTransport::Config uart_config;
    uart_midi.transport.Init(uart_config);
    uart_midi.Listen();
    MidiUsbTransport::Config usb_config;
    usb_config.periph = MidiUsbTransport::Config::EXTERNAL;
    usb_midi.transport.Init(usb_config);
    usb_midi.Listen();

    engine.SetSamples(&sample_table);
    recorder.Init(record_memory, kRecordFrames, &sample_table.slots[forge::kRamSlot], hw.seed.AudioSampleRate());
    sample_loader.Init(&sample_table, &sample_handoff, sample_pool, kPoolSamples, sample_scratch, sizeof(sample_scratch));
    if(!engine.Init(hw.seed.AudioSampleRate(), delay_left, delay_right, kDelayCapacity,
                    reverb_memory, kReverbCapacity)) {
        SetPthLedFloat(0, 0.1f, 0.f, 0.f);
        fill_led_data();
        while(true) { hw.LowBatteryLockoutCheck(); System::Delay(20); }
    }
    SetPthLedFloat(0, 0.f, 0.05f, 0.1f);
    fill_led_data();
    // SD card for device presets (as TAPE mounts it). Missing card: presets are
    // unavailable (menu keys light red), everything else works.
    SdmmcHandler::Config sd_config;
    sd_config.speed = SdmmcHandler::Speed::FAST;
    sd_config.width = SdmmcHandler::BusWidth::BITS_4;
    sdmmc.Init(sd_config);
    fsi.Init(FatFSInterface::Config::MEDIA_SD);
    card.SetMounted(f_mount(&fsi.GetSDFileSystem(), fsi.GetSDPath(), 1) == FR_OK);
    store.Rescan();
    cpu.Init(hw.seed.AudioSampleRate(), hw.seed.AudioBlockSize());
    hw.StartAudio(AudioCallback);
    uint32_t battery_check = System::GetNow();
    while(true) {
        TransmitPending();
        SendResponses();
        static uint32_t seen_drops = 0;
        const uint32_t ingress_drops = uart_midi.dropped.load(std::memory_order_relaxed) + usb_midi.dropped.load(std::memory_order_relaxed);
        if(ingress_drops != seen_drops || discard_ingress) {
            ScopedIrqBlocker guard;
            forge::MidiFrame ignored;
            for(unsigned i = 0; i < 16; ++i) { uart_midi.frames.Pop(ignored); usb_midi.frames.Pop(ignored); }
            uart_midi.framer.Reset(); usb_midi.framer.Reset();
            RaiseEmergency();
            seen_drops = ingress_drops; discard_ingress = false;
        }
        PollMidi(uart_midi, 0);
        PollMidi(usb_midi, 1);
        RunPanelActions();
        RunSampler();
        WatchCard();
        DrawLeds();
        const uint32_t now = System::GetNow();
        if(now - battery_check >= 20) {
            hw.LowBatteryLockoutCheck();
            battery_check = now;
        }
        System::DelayUs(100);
    }
}
