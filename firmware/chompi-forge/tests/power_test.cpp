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
// Start-up safe mode (0.15.2): three crashes before a stable run; cleared by a clean start,
// a stable run or a power loss (backup SRAM garbage = an invalid magic).
void SafeMode() {
    using namespace forge::restart;
    StartupGuard g{0xdeadbeefu, 77};                       // power-on: backup SRAM holds junk
    assert(!SafeModeStart(g, false) && g.crashes == 0);
    for(unsigned crash = 1; crash < kSafeModeCrashes; ++crash) { CountCrash(g); assert(!SafeModeStart(g, true)); }
    CountCrash(g); assert(SafeModeStart(g, true));        // the third early crash
    CountCrash(g); assert(SafeModeStart(g, true));        // safe mode crashing early stays safe
    RanStably(g); assert(!SafeModeStart(g, false) && g.crashes == 0);
    CountCrash(g); CountCrash(g); assert(!SafeModeStart(g, false));   // a reset without a crash record starts over
    CountCrash(g); assert(g.crashes == 1 && !SafeModeStart(g, true));
    StartupGuard junk{0x12345678u, 99}; CountCrash(junk); assert(junk.magic == StartupGuard::kMagic && junk.crashes == 1);
    char line[200];
    DescribeEvent(4, "SAFE MODE after repeated start-up crashes", line, sizeof line);
    assert(std::strcmp(line, "boot 4: SAFE MODE after repeated start-up crashes\n") == 0);
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

// The stock protection as the histories show it, and the install check (DC, 2026-10-05: a flat
// battery on a computer port left CHOMPI dark after an install).
void InstallPower() {
    Readings flat_pc; flat_pc.battery_low = 0xff; flat_pc.input_limit = 0xff;               // flat, at a 0.5 A port's limit
    Readings flat_unplugged; flat_unplugged.battery_low = 0xff; flat_unplugged.usb_good = 0;
    Readings flat_legacy; flat_legacy.battery_low = 0xff; flat_legacy.legacy = 0xff;
    Readings flat_strong; flat_strong.battery_low = 0xff;                                     // flat on a USB-C charger
    Readings flicker; flicker.battery_low = 0x7f; flicker.input_limit = 0xff;                 // not 8 readings in a row
    assert(StockLockout(flat_pc) == Lockout::WeakSupply && StockLockout(flat_legacy) == Lockout::WeakSupply);
    assert(StockLockout(flat_unplugged) == Lockout::Unplugged);
    assert(StockLockout(flat_strong) == Lockout::None && StockLockout(flicker) == Lockout::None && StockLockout(Readings{}) == Lockout::None);
    assert(*LockoutText(Lockout::WeakSupply) && *LockoutText(Lockout::Unplugged) && !*LockoutText(Lockout::None));
    // Yellow (or not yet measured) battery: only a strong supply allows the install.
    for(Battery low : {Battery::Medium, Battery::Low, Battery::Unknown}) {
        assert(!InstallPowerOk(low, flat_pc) && !InstallPowerOk(low, flat_unplugged) && !InstallPowerOk(low, flat_legacy));
        assert(InstallPowerOk(low, flat_strong));
        Readings one_limit; one_limit.input_limit = 0x01;                                    // one recent limit reading is enough to refuse
        assert(!InstallPowerOk(low, one_limit));
    }
    // Green or white with no recent low reading: fine on any supply, even unplugged.
    Readings unplugged; unplugged.usb_good = 0; Readings pc; pc.input_limit = 0xff;
    for(Battery ok : {Battery::Full, Battery::High}) {
        assert(InstallPowerOk(ok, unplugged) && InstallPowerOk(ok, pc));
        Readings dipped = pc; dipped.battery_low = 0x01;                                      // a low reading contradicts the colour
        assert(!InstallPowerOk(ok, dipped));
    }
    // 0.12 warning on SW6 and the Inspector flags.
    assert(BatteryWarning(Battery::Medium, flat_strong) == Warning::None);                  // charging: no warning
    assert(BatteryWarning(Battery::Medium, Readings{}) == Warning::None);                  // strong supply, yellow
    assert(BatteryWarning(Battery::Medium, pc) == Warning::Low && BatteryWarning(Battery::Medium, unplugged) == Warning::Low);
    assert(BatteryWarning(Battery::High, pc) == Warning::None && BatteryWarning(Battery::Unknown, pc) == Warning::None);
    assert(BatteryWarning(Battery::High, flat_pc) == Warning::Critical && BatteryWarning(Battery::Medium, flat_unplugged) == Warning::Critical);
    unsigned low_on = 0, critical_on = 0;
    for(uint32_t t = 0; t < 4000; t += 10) { low_on += WarningLit(Warning::Low, t); critical_on += WarningLit(Warning::Critical, t); }
    assert(low_on == 24 && critical_on == 208 && !WarningLit(Warning::None, 0));          // two 120 ms blinks; 4 Hz
    assert(SupplyFlags(Battery::High, Readings{}) == 0 && SupplyFlags(Battery::Medium, pc) == (8 | 32));
    assert(SupplyFlags(Battery::Medium, flat_unplugged) == (16 | 32) && SupplyFlags(Battery::High, pc) == 8);
    assert(SupplyFlags(Battery::Medium, flat_pc) == (8 | 16 | 32) && SupplyFlags(Battery::Medium, flat_pc) < 64);
    char line[160];
    const unsigned n = forge::restart::DescribeEvent(12, LockoutText(Lockout::WeakSupply), line, sizeof line);
    assert(n == std::strlen(line) && std::strncmp(line, "boot 12: battery low on a weak USB supply", 41) == 0 && line[n - 1] == '\n');
    assert(forge::restart::DescribeEvent(1, "x", line, 8) == 7 && std::strlen(line) == 7);    // truncated, still terminated
}

int main() {
    BootGestureOff(); Colours(); ChargerHandover(); StatusDecode(); RestartReason(); SafeMode(); OptionsFile(); InstallPower();
    std::cout << "PASS: power off gesture, battery colours, charger/USB hand-over (edges, timeout, wrap), status decode, restart reason, safe mode, options.json,"
                 " stock battery lockout, install power check, low-battery warning, supply flags\n";
}
