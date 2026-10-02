#include "hardware.h"
#include "command_queue.h"
#include "engine.h"
#include "midi_framer.h"
#include "runtime.h"
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
uint8_t usb_tx_packets[64];
struct Outgoing { uint8_t source = 0; uint8_t bytes[44]{}; size_t size = 0; };
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
uint32_t dropped_commands = 0, rejected_messages = 0; // main-loop owned

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    cpu.OnBlockStart();
    forge::Request request;
    static forge::RecoveryGate recovery; // audio-owner only
    recovery.Observe(static_cast<uint8_t>(emergency_epoch.load(std::memory_order_acquire)), engine);
    // Backpressure instead of applying a patch whose acknowledgement cannot
    // be queued. Only this audio callback produces responses.
    for(unsigned i = 0; i < 16 && responses.HasSpace() && requests.Pop(request); ++i) {
        if(!recovery.Admit(request, engine)) continue;
        forge::Response response;
        if(forge::ExecuteRequest(request, engine, response)) {
            response.cpu_average = cpu.GetAvgCpuLoad();
            response.cpu_max = cpu.GetMaxCpuLoad();
            responses.Push(response);
        }
    }

    hw.ProcessAllControls();
    // Matches upstream NormalPage::key_map: 25 chromatic keys, MIDI 48..72.
    static constexpr uint8_t notes[40] = {0,0,0,0,0,0,0,49,50,52,53,55,51,54,56,48,
        57,59,60,62,64,58,61,63,65,67,69,71,72,66,68,70,0,0,0,0,0,0,0,0};
    for(unsigned key = 0; key < 40; ++key) if(notes[key]) {
        if(hw.button_sr.RisingEdge(key)) engine.Note(notes[key], 100, 2);
        if(hw.button_sr.FallingEdge(key)) engine.Note(notes[key], 0, 2);
    }
    // Dedicated local recovery; SW5 turn controls synth tone, press silences.
    if(hw.enc[4].RisingEdge()) engine.Panic();
    const int tone = hw.enc[4].Increment();
    if(tone) engine.Apply({forge::Parameter::Cutoff, engine.GetParameters().cutoff + tone / 127.f});
    // Hardware encoder IDs, not assumptions about printed panel labels.
    const auto p = engine.GetParameters();
    const float values[] = {p.mix, p.time, p.feedback, p.level};
    for(unsigned i = 0; i < 4; ++i) {
        const int increment = hw.enc[i].Increment();
        if(increment) engine.Apply({static_cast<forge::Parameter>(i), values[i] + increment / 127.f});
    }
    // Physical volume encoder SW6 also controls output level.
    const int volume = hw.enc[5].Increment();
    if(volume) engine.Apply({forge::Parameter::Level, engine.GetParameters().level + volume / 127.f});

    for(size_t i = 0; i < size; ++i) {
        float left, right;
        // Upstream channel map: aux L/R = 2/3. Mic 0 intentionally unused.
        engine.Process(in[2][i], in[3][i], left, right);
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
        // A 44-byte v2 status reply takes 14.08 ms at 31250 baud; the upstream
        // PollTx wrapper's 10 ms timeout is too short. Audio remains interrupt-driven.
        sent = uart_midi.transport.GetUartHandle().BlockingTransmit(pending.bytes, pending.size, 25)
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

template<typename Midi> void PollMidi(Midi& midi, uint8_t source) {
    midi.Listen();
    forge::MidiFrame frame;
    for(unsigned i = 0; i < 8 && midi.frames.Pop(frame); ++i) {
        forge::Request request;
        if(frame.kind != forge::MidiFrame::Kind::SysEx) {
            switch(forge::TranslateChannel(frame, source, request)) {
                case forge::Ingress::Emergency: RaiseEmergency(); break;
                case forge::Ingress::Critical:
                    if(!Queue(request)) { ++dropped_commands; discard_ingress = true; RaiseEmergency(); }
                    break;
                case forge::Ingress::Control: if(!Queue(request)) ++dropped_commands; break;
                case forge::Ingress::Ignore: break;
            }
            continue;
        }
        if(!forge::IsRequest(frame.data, frame.size)) continue;
        auto error = forge::DecodeRequest(frame.data, frame.size, request);
        request.source = source;
        if(error == forge::Error::None && !Queue(request)) {
            ++dropped_commands; error = forge::Error::Busy;
        }
        if(error != forge::Error::None) {
            ++rejected_messages;
            uint8_t envelope[44];
            const size_t size = forge::EncodeError(forge::Read14(frame.data + 5), error, envelope + 1);
            Send(source, envelope, size);
        }
    }
}

void SendResponses() {
    forge::Response response;
    for(unsigned i = 0; i < 4 && outgoing.HasSpace() && responses.Pop(response); ++i) {
        const uint32_t dropped = dropped_commands + uart_midi.dropped.load(std::memory_order_relaxed)
            + usb_midi.dropped.load(std::memory_order_relaxed);
        uint8_t envelope[44];
        const size_t size = forge::EncodeResponse(response, dropped, rejected_messages, envelope + 1);
        Send(response.source, envelope, size);
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

    if(!engine.Init(hw.seed.AudioSampleRate(), delay_left, delay_right, kDelayCapacity)) {
        SetPthLedFloat(0, 0.1f, 0.f, 0.f);
        fill_led_data();
        while(true) { hw.LowBatteryLockoutCheck(); System::Delay(20); }
    }
    SetPthLedFloat(0, 0.f, 0.05f, 0.1f);
    fill_led_data();
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
        const uint32_t now = System::GetNow();
        if(now - battery_check >= 20) {
            hw.LowBatteryLockoutCheck();
            battery_check = now;
        }
        System::DelayUs(100);
    }
}
