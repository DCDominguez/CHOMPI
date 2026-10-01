#include "hardware.h"
#include "command_queue.h"
#include "engine.h"

using namespace daisy;
using namespace chompi;

namespace {
Hardware hw;
MidiUartHandler uart_midi;
MidiUsbHandler usb_midi;
forge::Engine engine;
forge::CommandQueue<64> commands;
// The bootloader does not zero SDRAM; Engine::Init clears these before audio.
constexpr size_t kDelayCapacity = 48002;
float DSY_SDRAM_BSS delay_left[kDelayCapacity];
float DSY_SDRAM_BSS delay_right[kDelayCapacity];
uint32_t dropped_commands = 0; // main-loop-owned debugger diagnostic

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    forge::Command command;
    // Bounded control work even under a MIDI flood.
    for(unsigned i = 0; i < 16 && commands.Pop(command); ++i) engine.Apply(command);

    hw.ProcessAllControls();
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
}

template<typename Midi> void PollMidi(Midi& midi) {
    midi.Listen();
    for(unsigned i = 0; i < 16 && midi.HasEvents(); ++i) {
        const auto event = midi.PopEvent();
        forge::Command command;
        if(event.type == ControlChange
           && forge::DecodeCC(event.channel, event.data[0], event.data[1], command)
           && !commands.Push(command)) ++dropped_commands;
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

    MidiUartHandler::Config uart_config;
    uart_midi.Init(uart_config);
    uart_midi.StartReceive();
    MidiUsbHandler::Config usb_config;
    usb_config.transport_config.periph = MidiUsbTransport::Config::EXTERNAL;
    usb_midi.Init(usb_config);
    usb_midi.Listen();

    if(!engine.Init(hw.seed.AudioSampleRate(), delay_left, delay_right, kDelayCapacity)) {
        SetPthLedFloat(0, 0.1f, 0.f, 0.f);
        fill_led_data();
        while(true) { hw.LowBatteryLockoutCheck(); System::Delay(20); }
    }
    SetPthLedFloat(0, 0.f, 0.05f, 0.1f);
    fill_led_data();
    hw.StartAudio(AudioCallback);
    uint32_t battery_check = System::GetNow();
    while(true) {
        PollMidi(uart_midi);
        PollMidi(usb_midi);
        const uint32_t now = System::GetNow();
        if(now - battery_check >= 20) {
            hw.LowBatteryLockoutCheck();
            battery_check = now;
        }
        System::DelayUs(100);
    }
}
