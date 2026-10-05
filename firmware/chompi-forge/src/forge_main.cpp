#include <new>
#include "hardware.h"
#include "command_queue.h"
#include "engine.h"
#include "fatfs_storage.h"
#include "midi_framer.h"
#include "panel_controller.h"
#include "preset_menu.h"
#include "restart.h"
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
#ifdef FORGE_TEST_HOOKS
    std::atomic<uint32_t> received{0};
#endif
    static void Receive(uint8_t* data, size_t size, void* context) {
        auto& self = *static_cast<MidiPort*>(context);
        forge::MidiFrame frame;
        for(size_t i = 0; i < size; ++i)
            if(self.framer.Feed(data[i], frame)) {
#ifdef FORGE_TEST_HOOKS
                self.received.fetch_add(1, std::memory_order_relaxed);
#endif
                if(!self.frames.Push(frame)) self.dropped.fetch_add(1, std::memory_order_relaxed);
            }
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
// Largest reply (v5 status, 102 bytes with F0/F7) packs into 34 USB-MIDI events.
constexpr size_t kMaxEnvelope = forge::kMaxReply + 2;
uint8_t usb_tx_packets[((kMaxEnvelope + 2) / 3) * 4];
struct Outgoing { uint8_t source = 0; uint8_t bytes[kMaxEnvelope]{}; size_t size = 0; };
forge::SpscQueue<Outgoing, 32> outgoing; // producer and consumer both main loop
Outgoing pending;
bool has_pending = false;
uint32_t pending_since = 0;
// Objects whose default members are not all zero would carry a .data image in SRAM_EXEC
// (code space); they are built at start-up in zeroed storage instead (RESOURCE_LEDGER).
template<typename T> T& Construct() { alignas(T) static unsigned char storage[sizeof(T)]; return *new(storage) T(); }
forge::Engine& engine = Construct<forge::Engine>();
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
// Why CHOMPI last started (core/restart.h). Backup SRAM keeps these across a reset
// (not a power loss); NOLOAD, so they are validated by a magic number.
struct BootCount { uint32_t magic, boots; };
__attribute__((section(".backup_sram.forge"))) forge::restart::FaultRecord fault_record;   // after boot_info (0x38800000)
__attribute__((section(".backup_sram.forge"))) BootCount boot_count;
uint8_t reset_flags = 0;                       // this start's RCC_RSR, compacted
forge::restart::FaultRecord last_fault{};      // the crash that caused this start, if any
// Forge's fault handler replaces libDaisy's (a breakpoint that freezes CHOMPI without a
// debugger) through a copy of the vector table: record where it crashed, then restart.
// The copy lives in DTCM: uncached, so the core's vector fetches see what was written
// without depending on D-cache maintenance (AXI SRAM is write-back cached).
alignas(1024) __attribute__((section(".dtcmram_bss"))) uint32_t vector_table[166];
extern "C" __attribute__((used)) void ForgeFaultRecord(const uint32_t* frame) {
    fault_record.pc = frame[6]; fault_record.lr = frame[5];
    fault_record.cfsr = SCB->CFSR; fault_record.hfsr = SCB->HFSR;
    fault_record.count = fault_record.Valid() ? fault_record.count + 1 : 1;
    fault_record.magic = forge::restart::FaultRecord::kMagic;
    __DSB();
    NVIC_SystemReset();
}
extern "C" __attribute__((naked)) void ForgeFaultHandler() {
    __asm volatile("tst lr, #4\n ite eq\n mrseq r0, msp\n mrsne r0, psp\n b ForgeFaultRecord\n");
}
void InstallFaultHandler() {
    PWR->CR1 |= PWR_CR1_DBP;                               // backup SRAM writable
    RCC->AHB4ENR |= RCC_AHB4ENR_BKPRAMEN;
    const uint32_t* current = reinterpret_cast<const uint32_t*>(SCB->VTOR);
    for(unsigned i = 0; i < 166; ++i) vector_table[i] = current[i];
    vector_table[3] = reinterpret_cast<uint32_t>(&ForgeFaultHandler);   // HardFault (other faults escalate to it)
    __disable_irq();
    SCB->VTOR = reinterpret_cast<uint32_t>(vector_table);
    __DSB(); __ISB();
    __enable_irq();
}
// TAPE's warble line (8 KB): ordinary .bss, zeroed at start-up, so no image space.
float warble_memory[2 * forge::tape::Warble::kLength];
uint32_t dropped_commands = 0, rejected_messages = 0; // main-loop owned

// Device presets on the SD card (main loop only) and the TAPE-style panel menu
// (audio owner). Menu actions cross to the main loop with a parameter snapshot.
SdmmcHandler sdmmc;
FatFSInterface fsi;
FatFsStorage card;
forge::PresetStore store(card);
// One line per start in FORGE/RESTARTS.TXT on the card (kept under 16 KB).
FORGE_COLD void LogRestart() {
    if(!card.Ready()) return;
    char line[160];
    const unsigned n = forge::restart::Describe(boot_count.boots, reset_flags, &last_fault, line, sizeof(line));
    f_mkdir("FORGE");
    FIL file;
    FILINFO info;
    const bool big = f_stat("FORGE/RESTARTS.TXT", &info) == FR_OK && info.fsize > 16384;
    if(f_open(&file, "FORGE/RESTARTS.TXT", big ? (FA_WRITE | FA_CREATE_ALWAYS) : (FA_WRITE | FA_OPEN_APPEND)) != FR_OK) return;
    UINT written = 0;
    f_write(&file, line, n, &written);
    f_close(&file);
}
// The charger readings behind the stock battery protection and the install check (core/power.h).
forge::power::Readings ChargerReadings() {
    forge::power::Readings r;
    r.battery_low = hw.batt_low_bounce; r.usb_good = hw.vin_gd_bounce;
    r.legacy = hw.legacy_cable_bounce; r.input_limit = hw.iindpm_stat_bounce;
    return r;
}
bool InstallPowerOk() {
    return forge::power::InstallPowerOk(static_cast<forge::power::Battery>(hw.GetBatteryLevel()), ChargerReadings());
}
// Just before the stock protection switches CHOMPI off or stops it, leave a line in
// FORGE/RESTARTS.TXT (once per kind per start): those shut-offs leave no reset flag.
FORGE_COLD void LogLockout(forge::power::Lockout lockout) {
    static uint8_t logged = 0;
    const uint8_t bit = static_cast<uint8_t>(1u << static_cast<unsigned>(lockout));
    if((logged & bit) || !card.Ready()) return;
    logged |= bit;
    char line[160];
    const unsigned n = forge::restart::DescribeEvent(boot_count.boots, forge::power::LockoutText(lockout), line, sizeof(line));
    FIL file;
    if(f_open(&file, "FORGE/RESTARTS.TXT", FA_WRITE | FA_OPEN_APPEND) != FR_OK) return;
    UINT written = 0;
    f_write(&file, line, n, &written);
    f_close(&file);
}
struct PanelAction { forge::MenuAction action; forge::Parameters patch; };
forge::SpscQueue<PanelAction, 8> panel_actions;   // audio -> main
std::atomic<uint32_t> menu_state{0};               // PresetMenu::Packed(), for LEDs
uint8_t last_bank = 0, last_slot = forge::panel::kNoSlot; // main-loop owned
uint32_t flash_until = 0; bool flash_ok = true;   // panel LED feedback after SD actions
std::atomic<uint32_t> audio_flash{0};              // audio -> main: 1 = ok, 2 = failed (record refused)

// Sampler (docs/forge/SAMPLING.md). SDRAM is not zeroed at boot; every read is
// bounded by a slot's published `loaded` count, so stale memory never plays.
constexpr uint32_t kPoolSamples = 32u * 1024 * 1024 / 2;      // 32 MiB: chromatic sample or kit bank (~174 s stereo)
constexpr uint32_t kRecordFrames = 16u * 1024 * 1024 / 4;     // 16 MB: ~87 s stereo at 48 kHz
int16_t DSY_SDRAM_BSS sample_pool[kPoolSamples];
int16_t DSY_SDRAM_BSS record_memory[2 * kRecordFrames];
// Looper (docs/forge/LOOPING.md): 4,000,000 stereo frames = ~83 s; reads stay below what was recorded.
constexpr uint32_t kLoopFrames = 4000000u;
int16_t DSY_SDRAM_BSS loop_memory[2 * kLoopFrames];
forge::Looper looper;                                         // audio owner
std::atomic<uint32_t> looper_state{0};                        // audio -> main: PackLooper, for the LEDs
uint8_t __attribute__((aligned(32))) sample_scratch[16384];   // D1 SRAM: reachable by SD DMA
forge::SampleTable& sample_table = Construct<forge::SampleTable>();
forge::SampleHandoff sample_handoff;
forge::SampleLoader& sample_loader = Construct<forge::SampleLoader>();                            // main loop
forge::Recorder recorder;                                     // audio owner (Unlock: main)
FatFsSampleFiles sample_files(card);
// USB file transfer and firmware install (core/file_transfer.h; main loop, gate shared with audio).
FatFsUploadFiles upload_files(card);
forge::FileTransfer file_transfer;
forge::InstallGate install_gate;
std::atomic<uint32_t> sample_wanted{0};                      // audio -> main: PackSelection of the live patch
std::atomic<bool> recording_now{false};                       // audio -> main, for the CHOMPI LED
std::atomic<float> input_peak{0.f};                           // audio -> main: input meter (record position)
std::atomic<float> loop_speed{1.f};                           // audio -> main: SW5's lights
forge::SpscQueue<forge::SampleJob, 4> sample_jobs;            // audio -> main: panel save/erase/copy
uint8_t led_shadow[26][3];                                    // main loop: what the LEDs show now (probe page 1)

#ifdef FORGE_TEST_HOOKS
forge::InspectorMailbox& inspector_mailbox = Construct<forge::InspectorMailbox>();
forge::InspectorAudio& inspector_latest = Construct<forge::InspectorAudio>();
forge::InspectorSnapshot& inspector_snapshot = Construct<forge::InspectorSnapshot>();
forge::InspectorLog& inspector_log = Construct<forge::InspectorLog>();
forge::SpscQueue<forge::InspectorEvent,64> inspector_edges;
std::atomic<uint32_t> inspector_event_drops{0}, inspector_panel_drops{0}, inspector_sample_drops{0};
uint32_t inspector_tx[2]{}, inspector_tx_errors[2]{};
uint32_t inspector_storage_errors=0;
uint8_t inspector_last_storage_error=0;
void InspectorEvent(forge::InspectorEventKind kind, uint8_t id, uint32_t value) {
    inspector_log.Add({0,System::GetNow(),value,kind,id});
}
void InspectorStorageError(forge::Error error) {
    if(error==forge::Error::None) return;
    ++inspector_storage_errors; inspector_last_storage_error=static_cast<uint8_t>(error);
    InspectorEvent(forge::InspectorEventKind::StorageError,0,static_cast<uint8_t>(error));
}
#endif

// Panel work for the main loop (audio callback is the producer).
struct FirmwarePanelSink : forge::PanelSink {
    bool PresetAction(const forge::MenuAction& action, const forge::Parameters& snapshot) override {
        const bool ok=panel_actions.Push({action, snapshot});
#ifdef FORGE_TEST_HOOKS
        if(!ok) inspector_panel_drops.fetch_add(1,std::memory_order_relaxed);
#endif
        return ok;
    }
    bool SampleJob(const forge::SampleJob& job) override {
        const bool ok=sample_jobs.Push(job);
#ifdef FORGE_TEST_HOOKS
        if(!ok) inspector_sample_drops.fetch_add(1,std::memory_order_relaxed);
#endif
        return ok;
    }
    void Flash(bool ok) override { audio_flash.store(ok ? 1 : 2, std::memory_order_relaxed); }
};
forge::PanelController& panel_controller = Construct<forge::PanelController>();                      // audio owner

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
        if(request.kind == forge::RequestKind::Panel) {         // development: injected panel event
            forge::PanelEvent event;
            event.kind = static_cast<forge::PanelEvent::Kind>(request.panel_kind);
            event.id = request.panel_id; event.value = request.panel_value;
            panel_controller.Inject(event);
            continue;
        }
        if(forge::ExecuteRequest(request, engine, response)) {
            response.cpu_average = cpu.GetAvgCpuLoad();
            response.cpu_max = cpu.GetMaxCpuLoad();
            responses.Push(response);
            if(request.kind == forge::RequestKind::Status && request.reset_cpu) cpu.Reset();   // audio owns the meter
        }
    }

    hw.ProcessAllControls();
    // The whole panel (menus, record gesture, keys, knobs) lives in
    // core/panel_controller.h; development builds merge injected events.
    static FirmwarePanelSink sink;
    forge::PanelInput input;
    input.frames = static_cast<uint16_t>(size);
    for(unsigned key = 0; key < forge::panel::kButtons; ++key)
        if(hw.button_sr.State(key)) input.keys |= uint64_t(1) << key;
    input.toggle_up = hw.GetToggleState();
    input.jack = hw.jack_detect.Read();
    input.tone_down = hw.enc[forge::panel::kToneEncoder].Pressed();
    for(unsigned i = 0; i < forge::panel::kEncoders; ++i) input.turns[i] = static_cast<int16_t>(hw.enc[i].Increment());
#ifdef FORGE_TEST_HOOKS
    const uint32_t inspector_now=System::GetNow();
    panel_controller.SetInspectorEvents(&inspector_edges,&inspector_event_drops,inspector_now);
#endif
    panel_controller.Block(input, engine, recorder, sink);
    const forge::RecordSource source = panel_controller.Source();
    sample_wanted.store(forge::PackSelection(engine.GetParameters()), std::memory_order_relaxed);
    recording_now.store(recorder.Recording(), std::memory_order_relaxed);
    menu_state.store(panel_controller.MenuPacked(), std::memory_order_relaxed);
    looper_state.store(forge::PackLooper(looper, engine.FxBeforeLoop()), std::memory_order_relaxed);
    loop_speed.store(looper.Speed(), std::memory_order_relaxed);

    const bool recording = recorder.Recording();
    // TAPE's monitor positions (menu SW6 press, options.json): Headphones = in the record
    // position the dry input goes to the headphones; Both = the input always goes through
    // the effects and looper to both outputs; SendReturn = the mic through the effects in
    // the record position, line in always back to the headphones. The meter follows the input.
    const bool record_position = panel_controller.RecordPosition();
    const forge::panel::MonitorMode mode = panel_controller.Monitor();
    const bool monitor = record_position || recording || mode != forge::panel::MonitorMode::Headphones;
    float peak = 0.f;
    for(size_t i = 0; i < size; ++i) {
        float left, right, rec_l = 0.f, rec_r = 0.f, pre_l = 0.f, pre_r = 0.f;
        bool post = false;                               // dry input into the headphones after the effects
        if(monitor) {
            recorder.Input(source, in[0][i], in[2][i], in[3][i], 0.f, 0.f, rec_l, rec_r);
            if(source != forge::RecordSource::Resample) {
                const bool mic = source == forge::RecordSource::Mic;
                if(mode == forge::panel::MonitorMode::Both) { pre_l = rec_l; pre_r = rec_r; }
                else if(mode == forge::panel::MonitorMode::SendReturn) {
                    if(mic && record_position) { pre_l = rec_l; pre_r = rec_r; } else if(!mic) post = true;
                } else post = record_position;
                peak = std::fmax(peak, std::fmax(std::fabs(rec_l), std::fabs(rec_r)));
            }
        }
        // Upstream channel map: mic 0, aux L/R = 2/3 (aux feeds the delay route).
        engine.Process(in[2][i], in[3][i], left, right, 0.5f * pre_l, 0.5f * pre_r);
        if(recording) {
            if(source == forge::RecordSource::Resample) recorder.Input(source, 0.f, 0.f, 0.f, left, right, rec_l, rec_r);
            recorder.Write(rec_l, rec_r);
        }
        float hp_l = left, hp_r = right;
        if(post) { hp_l = forge::Clamp(left + 0.5f * rec_l, -1.f, 1.f); hp_r = forge::Clamp(right + 0.5f * rec_r, -1.f, 1.f); }
        out[0][i] = hp_l; out[1][i] = hp_r;              // headphones
        out[2][i] = left; out[3][i] = right;             // main (line) out
    }
    input_peak.store(peak, std::memory_order_relaxed);
#ifdef FORGE_TEST_HOOKS
    static uint32_t block=0, revision=0, selection=0; static bool was_recording=false;
    ++block;
    auto edge=[&](forge::InspectorEventKind kind,uint32_t value) {
        if(!inspector_edges.Push({0,inspector_now,value,kind,0})) inspector_event_drops.fetch_add(1,std::memory_order_relaxed);
    };
    engine.ObserveVoiceEdges(inspector_edges,inspector_event_drops,inspector_now);
    if(engine.PatchRevision()!=revision) { revision=engine.PatchRevision(); edge(forge::InspectorEventKind::PatchApply,revision); }
    const uint32_t live=forge::PackSelection(engine.GetParameters());
    if(live!=selection) { selection=live; edge(forge::InspectorEventKind::SampleSelection,live); }
    if(recorder.Recording()!=was_recording) {
        was_recording=recorder.Recording();
        edge(was_recording?forge::InspectorEventKind::RecordingStart:forge::InspectorEventKind::RecordingStop,recorder.Length());
    }
    if(auto* a=inspector_mailbox.AudioBegin()) {
        engine.Inspect(*a); panel_controller.Inspect(*a);
        a->block=block; a->time_ms=inspector_now; a->record_frames=recorder.Length();
        a->recording=recorder.Recording(); a->locked=recorder.Locked();
        // Completed earlier callbacks only; mirror work is included in this callback's meter.
        a->cpu_average=cpu.GetAvgCpuLoad(); a->cpu_peak=cpu.GetMaxCpuLoad();
        inspector_mailbox.AudioEnd();
    }
#endif
    cpu.OnBlockEnd();
}

void Send(uint8_t source, uint8_t* envelope, size_t payload_size) {
    Outgoing message;
    message.source = source; message.size = payload_size + 2;
    envelope[0] = 0xf0; envelope[payload_size + 1] = 0xf7;
    std::copy(envelope, envelope + message.size, message.bytes);
    if(!outgoing.Push(message)) ++dropped_commands;
}

// TAPE's per-slot sample settings in presets.json (core/slot_settings.h), shared with TAPE.
// Read at start-up and when a card goes in; written by the main loop 2 s after the last
// change (TAPE's own way: presets_temp.json, then a rename), once FORGE/presets_backup.json
// holds the card's original file.
forge::SlotSettings slot_settings;
forge::harmony::Player harmony_player;   // harmony mode (core/harmony.h): zero-initialised, audio owner
alignas(32) char presets_text[forge::SlotSettings::kFileMax];   // whole cache lines: SD DMA reads
uint32_t slot_settings_failures = 0;
FORGE_COLD void LoadSlotSettings() {
    slot_settings.Clear();
    if(!card.Ready()) return;
    FIL file; UINT read = 0;
    if(f_open(&file, "presets.json", FA_READ) != FR_OK) return;
    f_read(&file, presets_text, sizeof(presets_text) - 1, &read);
    f_close(&file);
    presets_text[read] = 0;
    slot_settings.Parse(presets_text, read);
}
FORGE_COLD bool CopyFile(const char* from, const char* to) {
    FIL in, out;
    if(f_open(&in, from, FA_READ) != FR_OK) return false;
    bool ok = f_open(&out, to, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK;
    if(ok) {
        for(UINT read = 1; ok && read;) {
            UINT written = 0;
            ok = f_read(&in, presets_text, sizeof(presets_text), &read) == FR_OK
                 && (!read || (f_write(&out, presets_text, read, &written) == FR_OK && written == read));
        }
        ok = f_close(&out) == FR_OK && ok;
    }
    f_close(&in);
    return ok;
}
FORGE_COLD bool WritePresetsFile() {
    FILINFO info;
    if(f_stat("FORGE/presets_backup.json", &info) != FR_OK && f_stat("presets.json", &info) == FR_OK) {
        f_mkdir("FORGE");
        if(!CopyFile("presets.json", "FORGE/presets_backup.json")) return false;   // never write without the backup
    }
    const size_t n = slot_settings.Write(presets_text, sizeof(presets_text));
    if(!n) return false;
    FIL file; UINT written = 0;
    if(f_open(&file, "presets_temp.json", FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    const bool ok = f_write(&file, presets_text, n, &written) == FR_OK && written == n;
    if(f_close(&file) != FR_OK || !ok) { f_unlink("presets_temp.json"); return false; }
    f_unlink("presets.json");
    return f_rename("presets_temp.json", "presets.json") == FR_OK;
}
void SaveSlotSettings(uint32_t now) {
    static uint32_t seen = 0, changed_at = 0;
    const uint32_t changes = slot_settings.Changes();
    if(changes != seen) { seen = changes; changed_at = now; return; }
    if(!slot_settings.Dirty() || now - changed_at < 2000 || sample_loader.Busy() || !card.Ready()) return;
    slot_settings.TakeDirty();
    if(!WritePresetsFile()) ++slot_settings_failures;
}
// TAPE's options.json (core/options.h), read once before audio starts; Forge never writes it.
FORGE_COLD void LoadOptions() {
    forge::Options o;
    if(card.Ready()) {
        alignas(32) static char text[1024];     // whole cache lines: the SD driver invalidates around DMA reads
        FIL file; UINT read = 0;
        if(f_open(&file, "options.json", FA_READ) == FR_OK) {
            f_read(&file, text, sizeof(text), &read);
            f_close(&file);
            o = forge::options::Parse(text, read);
        }
    }
    panel_controller.SetOptions(o);
    looper.SetTapeSlew(o.tape_slew);
    engine.SetSplitDelay(o.split_delay);
}
// The panel's MIDI out (TAPE): UART and USB, separate from SysEx replies. A USB message
// waits up to 20 ms for the endpoint (a lost note-off would leave a note stuck elsewhere).
void SendPanelMidi() {
    static forge::MidiOut usb_pending; static bool usb_waiting = false; static uint32_t usb_since = 0;
    for(unsigned n = 0; n < 8; ++n) {
        if(usb_waiting) {
            bool sent = false;
            {
                ScopedIrqBlocker guard;
                if(hUsbDeviceHS.dev_state == USBD_STATE_CONFIGURED && hUsbDeviceHS.pClassData) {
                    auto* cdc = static_cast<USBD_CDC_HandleTypeDef*>(hUsbDeviceHS.pClassData);
                    if(cdc->TxState == 0) {
                        static uint8_t packet[4];
                        forge::PackUsbChannel(usb_pending.status, usb_pending.data1, usb_pending.data2, packet);
                        sent = usb_sender.TransmitExternal(packet, 4) == UsbHandle::Result::OK;
                    }
                } else sent = true;                      // no USB host: nothing to wait for
            }
            if(!sent && System::GetNow() - usb_since < 20) return;
            usb_waiting = false;
        }
        forge::MidiOut m;
        if(!panel_controller.PopMidi(m)) return;
        const uint8_t bytes[3] = {m.status, m.data1, m.data2};
        uart_midi.transport.GetUartHandle().BlockingTransmit(const_cast<uint8_t*>(bytes), 3, 3);
        usb_pending = m; usb_waiting = true; usb_since = System::GetNow();
    }
}
void DrawLeds(); void SendResponses(); void TransmitPending(); void RunSampler();
// TAPE's USB/charger hand-over after a power event (plug/unplug, charge state).
forge::power::ChargerUsb charger_usb;
bool usb_lines_to_daisy = true;
void ServiceChargerUsb(uint32_t now) {
    const auto a = charger_usb.Poll(now, hw.mpc_int.Read(), hw.read_ready, hw.mp_buff_[0]);
    if(a.read) hw.MpReadAll();
    if(a.usb_to_daisy >= 0) { usb_lines_to_daisy = a.usb_to_daisy != 0; hw.usb_sw.Write(usb_lines_to_daisy); }
    if(a.force_detection) hw.MpWrite(0x0a, 0B00110100);   // FORCEDPDM (as TAPE)
}
// Firmware install confirmed on the panel: finish sending the replies, then
// restart; the bootloader finds the new FORGE.bin on the card and flashes it.
[[noreturn]] void Restart() {
    file_transfer.Abort(upload_files);
    DrawLeds();
    uint32_t start = System::GetNow();
    while(sample_loader.Busy() && System::GetNow() - start < 5000) RunSampler();   // never cut a sample save short
    if(slot_settings.TakeDirty() && card.Ready()) WritePresetsFile();              // nor the last knob turns
    start = System::GetNow();
    while(System::GetNow() - start < 300) { TransmitPending(); SendResponses(); }
    NVIC_SystemReset();
    while(true) {}
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
        if(!sent) {
#ifdef FORGE_TEST_HOOKS
            ++inspector_tx_errors[0];
#endif
            ++dropped_commands; has_pending = false;
        }
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
    if(sent) {
#ifdef FORGE_TEST_HOOKS
        ++inspector_tx[pending.source];
#endif
        has_pending = false;
    }
    else if(System::GetNow() - pending_since > 100) {
#ifdef FORGE_TEST_HOOKS
        ++inspector_tx_errors[pending.source];
#endif
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
FORGE_COLD forge::Error RecallPreset(forge::Request& request) {
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
FORGE_COLD void RunPanelActions() {
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
#ifdef FORGE_TEST_HOOKS
        InspectorStorageError(error);
#endif
        Flash(error == forge::Error::None);
    }
}
// Sampler main-loop work: panel jobs, one loader step, job replies.
FORGE_COLD void RunSampler() {
    for(forge::SampleJob job; sample_jobs.Pop(job);) {
        job.source = 0xff;
        if(!sample_loader.Queue(job)) {
            if(job.kind == forge::SampleJob::Kind::Save) { if(job.from_loop) looper.Unlock(); else recorder.Unlock(); }
            Flash(false);
        }
    }
    forge::SampleEvent event;
    if(sample_loader.Poll(sample_files, sample_wanted.load(std::memory_order_relaxed), record_memory, event, loop_memory)) {
#ifdef FORGE_TEST_HOOKS
        InspectorEvent(forge::InspectorEventKind::SampleJobDone,static_cast<uint8_t>(event.job.kind),event.ok?1:0);
#endif
        if(event.job.kind == forge::SampleJob::Kind::Save) { if(event.job.from_loop) looper.Unlock(); else recorder.Unlock(); }
        forge::FollowSampleJob(slot_settings, event);
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
// Key LEDs (~30 Hz), the knob page LEDs (1-4), PLAY/LOOP (7/8) and panel LED 0 (the CHOMPI key, as in TAPE): red while
// recording, a flash after an action, pink blink while saving/copying. The
// composed colours are kept for the development probe.
// White balance: CHOMPI's LEDs show full white as light blue (DC, 2026-10-05; TAPE drives
// them the same way), so green and blue are trimmed. Applied only at the LED driver;
// the composed colours (tests, development probe) stay TAPE's.
constexpr float kBalanceG = .85f, kBalanceB = .6f;
forge::Rgb Balance(forge::Rgb c) { return forge::Rgb{c.r, c.g * kBalanceG, c.b * kBalanceB}; }
void Pth(unsigned led, forge::Rgb c) { c = Balance(c); SetPthLedFloat(led, c.r, c.g, c.b); }
FORGE_COLD void DrawLeds() {
    static uint32_t last_draw = 0;
    const uint32_t now = System::GetNow();
    if(now - last_draw < 33) return;
    last_draw = now;
    forge::LedView view;
    view.menu = menu_state.load(std::memory_order_relaxed);
    view.preset_occupancy = store.Occupancy((view.menu >> 4) & 7u); view.preset_card = store.Ready();
    view.last_bank = last_bank; view.last_slot = last_slot;
    view.sample_occupancy = sample_loader.Occupancy((view.menu >> 22) & 1u, (view.menu >> 4) & 7u);
    view.sample_card = sample_files.Ready();
    view.recording_present = sample_table.slots[forge::kRamSlot].loaded.load(std::memory_order_acquire) > 0;
    view.recording_now = recording_now.load(std::memory_order_relaxed);
    view.live = sample_wanted.load(std::memory_order_relaxed);
    view.keys_down = panel_controller.KeysDown();
    view.kit_occupancy = sample_loader.Occupancy(1, (view.live >> 2) & 7u);
    view.harmony = panel_controller.HarmonyLights();
    view.blink = (now / 250) % 2 == 0; view.slow_blink = (now / 300) % 2 != 0;
    view.flash = now < flash_until ? (flash_ok ? 1 : 0) : -1;
    view.saving = sample_loader.Busy() && !sample_loader.Loading();
    view.looper = looper_state.load(std::memory_order_relaxed);
    view.install = install_gate.Confirmed() ? 2 : install_gate.Armed() ? 1 : 0;
    view.record_position = panel_controller.RecordPosition();
    view.count_in = panel_controller.CountIn();
    static float meter = 0.f;                                    // TAPE-like VU: fast up, slow down
    meter = std::fmax(input_peak.load(std::memory_order_relaxed), meter * 0.85f);
    view.input_level = meter;
    forge::Rgb keys[25], chompi, play, loop;
    forge::ComposeLeds(view, keys, chompi);
    forge::ComposeLooperLeds(view.looper, view.blink, play, loop);
    if(view.record_position) { play = forge::knobs::Scale(play, .7f); loop = forge::knobs::Scale(loop, .7f); }  // TAPE kRecDim
    Pth(forge::panel::kPlayLed, play); Pth(forge::panel::kLoopLed, loop);
    forge::Rgb knobs[4], reverse, forward;
    forge::ComposeKnobLeds(panel_controller.KnobValues(), panel_controller.KnobState(), view.record_position, knobs);
    const uint32_t knob_state = panel_controller.KnobState();
    forge::Rgb volume = forge::knobs::VolumeColour((knob_state >> 20) & 1u, (knob_state >> 24) / 255.f);
    forge::ComposeMenuKnobLeds(panel_controller.MenuLights(), knobs, volume);   // TAPE's menu page
    if((view.menu & 1u) && ((view.menu >> 1) & 7u) == 7u) forge::ComposeHarmonyKnobLeds(view.harmony, knobs);   // harmony page
    for(unsigned k = 0; k < 4; ++k) Pth(forge::panel::kKnobLed[k], knobs[k]);
    forge::ComposeTransportLeds((view.looper & 7u) == static_cast<uint32_t>(forge::Looper::State::Playing),
                                loop_speed.load(std::memory_order_relaxed), view.record_position, reverse, forward);
    Pth(5, reverse); Pth(6, forward);
    for(unsigned i = 0; i < 25; ++i) { const forge::Rgb c = Balance(keys[i]); SetSmtLedFloat(i, c.r, c.g, c.b); }
    Pth(0, chompi);
    // SW6 (TAPE): held 2 s = battery; otherwise its page (volume / input gain) and value.
    if(panel_controller.BatteryView()) {
        const auto b = forge::power::BatteryColour(static_cast<forge::power::Battery>(hw.GetBatteryLevel()));
        volume = forge::Rgb{b.r, b.g, b.b};
    } else {                                                       // 0.12: low battery warning (core/power.h)
        const auto warning = forge::power::BatteryWarning(static_cast<forge::power::Battery>(hw.GetBatteryLevel()), ChargerReadings());
        if(forge::power::WarningLit(warning, now)) volume = forge::Rgb{forge::power::kWarningColour.r, forge::power::kWarningColour.g, forge::power::kWarningColour.b};
    }
    Pth(forge::panel::kVolumeLed, volume);
    fill_led_data();
    auto seven = [](float x) { return static_cast<uint8_t>(forge::Clamp(x, 0.f, 1.f) * 127.f + 0.5f); };
    for(unsigned i = 0; i < 26; ++i) {
        const forge::Rgb& c = i < 25 ? keys[i] : chompi;
        led_shadow[i][0] = seven(c.r); led_shadow[i][1] = seven(c.g); led_shadow[i][2] = seven(c.b);
    }
}
// Card insert/remove: remount and rescan when the card comes back.
FORGE_COLD void WatchCard() {
    static uint32_t last_check = 0; static bool was_ready = false;
    const uint32_t now = System::GetNow();
    if(now - last_check < 1000) return;
    last_check = now;
    const bool present = disk_status(0) == RES_OK;
    if(present && !was_ready) {
        card.SetMounted(f_mount(&fsi.GetSDFileSystem(), fsi.GetSDPath(), 1) == FR_OK);
#ifdef FORGE_TEST_HOOKS
        if(!card.Ready()) InspectorStorageError(forge::Error::Storage);
#endif
        store.Rescan();
        LoadSlotSettings();
    }
    was_ready = present && card.Ready();
}

#ifdef FORGE_TEST_HOOKS
void CollectInspector() {
    for(unsigned i=0;i<64;++i) { forge::InspectorEvent e; if(!inspector_edges.Pop(e)) break; inspector_log.Add(e); }
    inspector_mailbox.Read(inspector_latest);
    static uint32_t refreshed=0;
    const uint32_t now=System::GetNow();
    if(now-refreshed>=50) { refreshed=now; inspector_mailbox.RequestRefresh(); }
    static uint32_t drops=0, loaded=0, errors=0; static uint8_t card_flags=127;
    const uint32_t d=dropped_commands+uart_midi.dropped.load()+usb_midi.dropped.load()
        +inspector_panel_drops.load()+inspector_sample_drops.load()+inspector_event_drops.load();
    if(d!=drops) { InspectorEvent(forge::InspectorEventKind::QueueError,0,d-drops); drops=d; }
    const uint8_t flags=(disk_status(0)==RES_OK?1:0)|(card.Ready()?2:0);
    if(flags!=card_flags) { card_flags=flags; InspectorEvent(forge::InspectorEventKind::Card,0,flags); }
    forge::InspectorStorage st; sample_loader.Inspect(st);
    if(st.loaded_selection!=loaded && st.loaded_selection!=0xffffffffu && !st.loading) {
        loaded=st.loaded_selection; InspectorEvent(forge::InspectorEventKind::SampleLoaded,0,loaded);
    }
    if(st.errors!=errors) {
        inspector_last_storage_error=static_cast<uint8_t>(forge::Error::Storage);
        InspectorEvent(forge::InspectorEventKind::StorageError,1,st.errors-errors); errors=st.errors;
    }
}
FORGE_COLD void CaptureInspector() {
    CollectInspector();
    auto& s=inspector_snapshot; ++s.generation; s.audio=inspector_latest;
    auto& sys=s.system; sys.uptime_ms=System::GetNow();
    sys.rx[0]=uart_midi.received.load(); sys.rx[1]=usb_midi.received.load();
    sys.ingress_drops[0]=uart_midi.dropped.load(); sys.ingress_drops[1]=usb_midi.dropped.load();
    for(unsigned i=0;i<2;++i) { sys.tx[i]=inspector_tx[i]; sys.tx_errors[i]=inspector_tx_errors[i]; }
    sys.dropped=dropped_commands+sys.ingress_drops[0]+sys.ingress_drops[1]; sys.rejected=rejected_messages;
    sys.panel_drops=inspector_panel_drops.load(); sys.sample_drops=inspector_sample_drops.load();
    sys.event_drops=inspector_event_drops.load(); sys.emergencies=emergency_epoch.load();
    const auto power=forge::power::DecodeStatus(hw.mp_buff_, static_cast<forge::power::Battery>(hw.GetBatteryLevel()),
                                                charger_usb.Handover() || !usb_lines_to_daisy);
    sys.battery=static_cast<uint8_t>(power.level);
    sys.power_flags=static_cast<uint8_t>((power.usb_power?1:0)|(power.fault?2:0)|(power.usb_to_charger?4:0)
                                         |forge::power::SupplyFlags(power.level,ChargerReadings()));
    sys.charge_state=power.charge_state;
    sys.reset_flags=reset_flags; sys.crashed=last_fault.Valid(); sys.crash_pc=last_fault.pc;
    auto& st=s.storage; sample_loader.Inspect(st);
    st.present=disk_status(0)==RES_OK; st.mounted=card.Mounted(); st.record_capacity_frames=kRecordFrames;
    st.errors+=inspector_storage_errors;
    if(inspector_last_storage_error) st.last_error=inspector_last_storage_error;
    st.pending+=static_cast<uint8_t>(sample_jobs.Size());
}
#endif

// One received frame (shared by both transports, so the code exists once).
FORGE_COLD void HandleFrame(const forge::MidiFrame& frame, uint8_t source) {
    forge::Request request;
    if(frame.kind == forge::MidiFrame::Kind::SysEx && forge::IsRequest(frame.data, frame.size)
       && frame.data[4] == forge::kFileOpcode) {
        uint8_t envelope[kMaxEnvelope];
        const size_t size = forge::ServeFileRequest(frame.data, frame.size, file_transfer, upload_files, install_gate,
                                                    System::GetNow(), envelope + 1, sample_loader.Busy(), InstallPowerOk());
        if(file_transfer.SampleWritten()) sample_loader.Rescan();
        Send(source, envelope, size);
        return;
    }
    if(frame.kind != forge::MidiFrame::Kind::SysEx) {
        switch(forge::TranslateChannel(frame, source, request, panel_controller.GetOptions().midi_in)) {
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
#ifdef FORGE_TEST_HOOKS
    // Development probe: answered here from the latest snapshots; panel events
    // go to the audio owner and are acknowledged once queued.
    if(error == forge::Error::None && request.kind == forge::RequestKind::Probe) {
        uint8_t envelope[kMaxEnvelope];
        size_t size;
        if(request.page>=2 && !inspector_latest.block) {
            inspector_mailbox.RequestRefresh();
            ++rejected_messages;
            Send(source,envelope,forge::EncodeError(request.sequence,forge::Error::Busy,envelope+1));
            return;
        }
        if(request.page==1) size = forge::EncodeProbeLeds(request.sequence, led_shadow, envelope + 1);
        else {
            if(request.page==2 || !inspector_snapshot.generation) CaptureInspector();
            size=forge::EncodeInspector(request.sequence,request.page,inspector_snapshot,inspector_log,request.inspector_cursor,envelope+1);
        }
        Send(source, envelope, size);
        return;
    }
    if(error == forge::Error::None && request.kind == forge::RequestKind::Panel) {
        if(Queue(request)) { uint8_t envelope[kMaxEnvelope]; Send(source, envelope, forge::EncodePanelAck(request.sequence, envelope + 1)); return; }
        error = forge::Error::Busy;
        handled = true;
    }
#endif
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
#ifdef FORGE_TEST_HOOKS
        if(error==forge::Error::Storage || error==forge::Error::StorageBusy) InspectorStorageError(error);
#endif
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

FORGE_COLD void SendResponses() {
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
#ifdef FORGE_TEST_HOOKS
            InspectorStorageError(response.error);
#endif
        }
        SendResponse(response);
    }
}
} // namespace

FORGE_COLD int main() {
    const uint32_t rsr = RCC->RSR;                 // why this start, before anything clears it
    RCC->RSR |= RCC_RSR_RMVF;
    reset_flags = forge::restart::Flags(rsr);
    hw.Init();
    InstallFaultHandler();
    if(fault_record.Valid()) { last_fault = fault_record; fault_record.magic = 0; }
    if(boot_count.magic != forge::restart::FaultRecord::kMagic) { boot_count.magic = forge::restart::FaultRecord::kMagic; boot_count.boots = 0; }
    ++boot_count.boots;
    LedSetup();
    hw.MpWrite(0x0c, 0B01010001); // retain upstream 3 V battery threshold
    hw.MpReadAll();
    for(unsigned i = 0; i < 10; ++i) {
        hw.LowBatteryLockoutCheck();
        System::Delay(10);
    }
    // Stock start-up scan (0.5 s, also clears shift-register junk): CHOMPI + PLAY +
    // LOOP held while CHOMPI starts switches it off (charger IC shipping mode).
    forge::power::BootGesture off_gesture;
    for(unsigned i = 0; i < forge::power::kBootScans; ++i) {
        hw.ProcessAllControls();
        off_gesture.Scan(hw.button_sr.State(int(Hardware::SwId::KEY_26)), hw.button_sr.State(int(Hardware::SwId::KEY_27)),
                         hw.button_sr.State(int(Hardware::SwId::KEY_28)));
        System::DelayUs(forge::power::kBootScanUs);
    }
    if(off_gesture.PowerOff()) {
        hw.LedsOff();
        hw.MpWrite(0x08, 0B10111111);   // SHIPPING MODE (as stock)
        while(true) System::Delay(10);
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
    looper.Init(loop_memory, kLoopFrames, hw.seed.AudioSampleRate());
    engine.SetLooper(&looper);
    engine.SetWarbleMemory(warble_memory);
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
#ifdef FORGE_TEST_HOOKS
    if(disk_status(0)==RES_OK && !card.Ready()) InspectorStorageError(forge::Error::Storage);
#endif
    store.Rescan();
    LogRestart();
    LoadOptions();
    LoadSlotSettings();
    engine.SetSlotSettings(&slot_settings);
    engine.SetHarmony(&harmony_player);
    panel_controller.SetInstallGate(&install_gate);
    cpu.Init(hw.seed.AudioSampleRate(), hw.seed.AudioBlockSize());
    hw.StartAudio(AudioCallback);
    uint32_t battery_check = System::GetNow();
    while(true) {
        TransmitPending();
        SendPanelMidi();
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
#ifdef FORGE_TEST_HOOKS
        CollectInspector();
#endif
        const uint32_t now = System::GetNow();
        if(now - battery_check >= 20) {
            const auto lockout = forge::power::StockLockout(ChargerReadings());
            if(lockout != forge::power::Lockout::None) LogLockout(lockout);
            hw.LowBatteryLockoutCheck();
            battery_check = now;
        }
        ServiceChargerUsb(now);
        SaveSlotSettings(now);
        if(install_gate.Poll(now)) {
            if(InstallPowerOk()) Restart();
            else install_gate.Cancel();       // power changed since the request: the host sees the power flag
        }
        System::DelayUs(100);
    }
}
