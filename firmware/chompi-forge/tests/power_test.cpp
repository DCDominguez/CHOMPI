#include <cassert>
#include <cstring>
#include <iostream>
#include "../core/power.h"
#include "../core/restart.h"
#include "../core/options.h"

using namespace forge::power;

// Stock start-up gesture: CHOMPI + PLAY + LOOP for more than 4,000 of the 5,000 scans.
void BootGestureOff() {
    BootGesture held, two, brief;
    for(unsigned i = 0; i < kBootScans; ++i) {
        held.Scan(true, true, true);
        two.Scan(true, true, false);                       // PLAY + LOOP alone (a looper clear) is not it
        brief.Scan(i < 4000, i < 4000, i < 4000);          // exactly 4,000: not more than the threshold
    }
    assert(held.PowerOff() && !two.PowerOff() && !brief.PowerOff());
    BootGesture nothing; for(unsigned i = 0; i < kBootScans; ++i) nothing.Scan(false, false, false);
    assert(!nothing.PowerOff());
}

void Colours() {
    const Colour w = BatteryColour(Battery::Full), g = BatteryColour(Battery::High), y = BatteryColour(Battery::Medium),
                 r = BatteryColour(Battery::Low), off = BatteryColour(Battery::Unknown);
    assert(w.r == 1.f && w.g == 1.f && w.b == 1.f && g.g == 1.f && g.r == 0.f && y.r == 1.f && y.g > .9f && r.r == 1.f && r.g == 0.f);
    assert(off.r == 0.f && off.g == 0.f && off.b == 0.f);
}

// TAPE's hand-over, step by step: an interrupt edge reads the status; an unidentified
// source hands D+/D- to the charger and forces detection 1 ms later; an identified one
// takes them back. A read that never completes is abandoned after the timeout.
void ChargerHandover() {
    ChargerUsb c;
    auto a = c.Poll(0, true, true, 0x00);  assert(!a.read && a.usb_to_daisy == -1 && !a.force_detection);   // idle line
    a = c.Poll(1, false, true, 0x00);      assert(a.read && a.usb_to_daisy == -1);                          // falling edge
    a = c.Poll(2, false, false, 0x00);     assert(!a.read && a.usb_to_daisy == -1);                         // read running, no new edge
    a = c.Poll(3, false, true, 0x00);      assert(a.usb_to_daisy == 0 && !a.force_detection && c.Handover()); // unidentified
    a = c.Poll(4, false, true, 0x00);      assert(!a.force_detection);                                       // 1 ms not yet passed
    a = c.Poll(5, true, true, 0x00);       assert(a.force_detection && !c.Handover());                      // FORCEDPDM once
    a = c.Poll(6, true, true, 0x00);       assert(!a.force_detection && !a.read);
    a = c.Poll(7, false, false, 0x00);     assert(a.read);                                                    // detection done: new edge
    a = c.Poll(8, false, true, 0x30);      assert(a.usb_to_daisy == 1 && !c.Handover());                     // identified: lines back
    // A read that never completes: abandoned, the next edge starts again.
    ChargerUsb lost;
    lost.Poll(0, true, false, 0); a = lost.Poll(1, false, false, 0); assert(a.read);
    for(uint32_t t = 2; t <= 1 + ChargerUsb::kReadTimeoutMs; ++t) { a = lost.Poll(t, false, false, 0); assert(a.usb_to_daisy == -1); }
    a = lost.Poll(2 + ChargerUsb::kReadTimeoutMs, false, false, 0); assert(a.usb_to_daisy == -1);
    a = lost.Poll(3 + ChargerUsb::kReadTimeoutMs, false, true, 0);  assert(a.usb_to_daisy == -1);         // stale completion ignored
    lost.Poll(4 + ChargerUsb::kReadTimeoutMs, true, true, 0);
    a = lost.Poll(5 + ChargerUsb::kReadTimeoutMs, false, false, 0); assert(a.read);
    // Wrap-around of the millisecond clock.
    ChargerUsb wrap; wrap.Poll(0xfffffffeu, true, true, 0); a = wrap.Poll(0xffffffffu, false, false, 0); assert(a.read);
    a = wrap.Poll(0u, false, true, 0); assert(a.usb_to_daisy == 0);
    a = wrap.Poll(2u, false, true, 0); assert(a.force_detection);
}

void StatusDecode() {
    const uint8_t regs[6] = {0x30, 0x40, 0xa0, 0x00, 0, 0};
    auto s = DecodeStatus(regs, Battery::High, false);
    assert(s.usb_power && s.charge_state == 5 && !s.fault && !s.usb_to_charger && s.level == Battery::High);
    const uint8_t bad[6] = {0, 0, 0x60, 0x04, 0, 0};
    s = DecodeStatus(bad, Battery::Medium, true);
    assert(!s.usb_power && s.charge_state == 3 && s.fault && s.usb_to_charger);
}

// Restart reason (core/restart.h): RCC_RSR flags and the crash record, as logged.
void RestartReason() {
    using namespace forge::restart;
    assert(Flags(0) == 0 && Flags(kPowerOn | kBrownOut | kPin) == 7 && Flags(kSoftware) == 8 && Flags(kWatchdog | kLowPower) == 80);
    char line[200];
    FaultRecord none{}; assert(!none.Valid());
    Describe(3, Flags(kPowerOn | kBrownOut), &none, line, sizeof(line));
    assert(std::strcmp(line, "boot 3: power-on brown-out\n") == 0);
    FaultRecord crash{FaultRecord::kMagic, 0x24012345u, 0x24000101u, 0x8200u, 0x40000000u, 1};
    Describe(12, Flags(kSoftware), &crash, line, sizeof(line));
    assert(std::strcmp(line, "boot 12: software; crash pc=0x24012345 lr=0x24000101 cfsr=0x00008200 hfsr=0x40000000\n") == 0);
    Describe(1, 0, nullptr, line, sizeof(line)); assert(std::strcmp(line, "boot 1: (no reset flags)\n") == 0);
    char small[8]; assert(Describe(1, 0, nullptr, small, sizeof(small)) == 7 && small[7] == 0);   // truncated, terminated
}

// TAPE's options.json, exactly as TAPE writes it (OptionsManager::WriteFile), and edge cases.
void OptionsFile() {
    const char tape[] = "{\n\t\"chompi\": [\n\t\t{\n\t\t\t\"name\": \"Record Latch\",\n\t\t\t\"value\": true\n\t\t},"
        "\n\t\t{\n\t\t\t\"name\": \"Midi In Channel\",\n\t\t\t\"value\": 3\n\t\t},\n\t\t{\n\t\t\t\"name\": \"Midi Out Channel\",\n\t\t\t\"value\": 16\n\t\t},"
        "\n\t\t{\n\t\t\t\"name\": \"Tape Slew On\",\n\t\t\t\"value\": false\n\t\t},\n\t\t{\n\t\t\t\"name\": \"Monitor Position\",\n\t\t\t\"value\": 3\n\t\t},"
        "\n\t\t{\n\t\t\t\"name\": \"Pitch Quantize In Shift Menu\",\n\t\t\t\"value\": false\n\t\t},\n\t\t{\n\t\t\t\"name\": \"Split Delay\",\n\t\t\t\"value\": true\n\t\t}\n\t]\n}";
    const forge::Options o = forge::options::Parse(tape, sizeof(tape) - 1);
    assert(o.record_latch && o.midi_in == 2 && o.midi_out == 15 && !o.tape_slew && o.monitor == 2 && !o.quantise_menu && o.split_delay);
    const forge::Options d = forge::options::Parse("", 0);                     // no file: TAPE's defaults
    assert(!d.record_latch && d.midi_in == 0 && d.midi_out == 0 && d.tape_slew && d.monitor == 0 && d.quantise_menu && !d.split_delay);
    const char bad[] = "{\"chompi\":[{\"name\":\"Midi In Channel\",\"value\":17},{\"name\":\"Monitor Position\"},{\"name\":\"Record Latch\",\"value\":";
    const forge::Options b = forge::options::Parse(bad, sizeof(bad) - 1);     // out of range, missing, cut off: defaults
    assert(b.midi_in == 0 && b.monitor == 0 && !b.record_latch);
    assert(forge::options::Parse(tape, 40).midi_in == 0);                     // truncated read
}

int main() {
    BootGestureOff(); Colours(); ChargerHandover(); StatusDecode(); RestartReason(); OptionsFile();
    std::cout << "PASS: power off gesture, battery colours, charger/USB hand-over (edges, timeout, wrap), status decode, restart reason, options.json\n";
}
