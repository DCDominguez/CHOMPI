#include <cassert>
#include <cstring>
#include <iostream>
#include "../core/power.h"
#include "../core/battery_diag.h"
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
    // 0.19.2: USB power plugged in while on, the interrupt pulse missed: the reads start the
    // hand-over; the lines go back once a read shows the source identified (its pulse missed too).
    ChargerUsb plug;
    a = plug.Poll(0, true, true, 0x00, 0x00);  assert(!a.read && a.usb_to_daisy == -1);               // on battery
    a = plug.Poll(20, true, true, 0x00, 0x40); assert(a.read);                                        // power appears
    a = plug.Poll(21, true, true, 0x00, 0x40); assert(a.usb_to_daisy == 0 && plug.Handover());
    a = plug.Poll(23, true, true, 0x00, 0x40); assert(a.force_detection && !a.read);                  // no second edge
    a = plug.Poll(40, true, true, 0x00, 0x40); assert(a.usb_to_daisy == -1 && !a.read);               // still detecting
    a = plug.Poll(60, true, true, 0x30, 0x40); assert(a.usb_to_daisy == 1);                           // identified: back
    a = plug.Poll(80, true, true, 0x30, 0x40); assert(a.usb_to_daisy == -1 && !a.read);               // once
    ChargerUsb boot;                                                                                  // powered at start-up: no hand-over
    a = boot.Poll(0, true, true, 0x30, 0x40);  assert(!a.read && a.usb_to_daisy == -1);
    a = boot.Poll(20, true, true, 0x00, 0x40); assert(!a.read && a.usb_to_daisy == -1);
    a = boot.Poll(40, true, true, 0x00, 0x00); a = boot.Poll(60, true, true, 0x00, 0x40); assert(a.read); // unplug, replug
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

// The stock protection as the histories show it, and the install check (the owner, 2026-10-05: a flat
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

// 0.19.2: a dip while playing (the owner's shut-offs, 2026-10-11) is not a flat battery; 3 s of low readings is.
void LockoutDip() {
    Readings pc; pc.input_limit = 0xff; Readings low_pc = pc; low_pc.battery_low = 0xff;
    LockoutGuard dip;
    for(int i = 0; i < 40; ++i) assert(dip.Update(low_pc) == Lockout::None);            // 0.8 s below 3.0 V
    assert(dip.Update(pc) == Lockout::None);                                            // recovers: the count restarts
    for(uint32_t i = 1; i < LockoutGuard::kSustainChecks; ++i) assert(dip.Update(low_pc) == Lockout::None);
    assert(dip.Update(low_pc) == Lockout::WeakSupply && dip.Update(low_pc) == Lockout::WeakSupply);
    assert(LockoutGuard::kSustainChecks * LockoutGuard::kCheckMs == 3000);
    Readings flat_unplugged; flat_unplugged.battery_low = 0xff; flat_unplugged.usb_good = 0;
    LockoutGuard flat;
    Lockout l = Lockout::None;
    for(uint32_t i = 0; i < LockoutGuard::kSustainChecks; ++i) l = flat.Update(flat_unplugged);
    assert(l == Lockout::Unplugged);                                                    // a flat battery still gets the stock protection
    Readings strong; strong.battery_low = 0xff;
    LockoutGuard charger;
    for(int i = 0; i < 500; ++i) assert(charger.Update(strong) == Lockout::None);       // strong supply: never
}

// Battery instrumentation (development builds): readings are counted under the threshold that
// applied when their read started; read-backs follow the stock level check's writes.
void BatteryInstrumentation() {
    BatteryMonitor m;
    assert(m.Expected(ReadBack::Periodic) == kThreshold30 && kThreshold30 == 0x51 && kThreshold33 == 0x5d);
    m.Sample(false, 0); assert(m.Diag().checks == 1 && m.Diag().not_ready == 1 && m.Diag().samples_30 == 0);
    m.ReadStarted(); assert(m.LevelCheck(0, 0) == ReadBack::None);
    m.Sample(true, 0x10); m.ReadStarted(); m.Sample(true, 0x10);            // two low readings at 3.0 V
    // The check that writes 3.3 V: its read started before the write, so it still counts at 3.0 V.
    m.ReadStarted(); assert(m.LevelCheck(0, 1) == ReadBack::After33 && m.Diag().expect_33);
    m.ReadBackDone(ReadBack::After33, true, kThreshold33);
    m.Sample(true, 0x00); assert(m.Diag().samples_30 == 3 && m.Diag().low_30 == 2 && m.Diag().low_run_30 == 0 && m.Diag().low_run_30_max == 2);
    m.ReadStarted(); assert(m.LevelCheck(1, 1) == ReadBack::None);
    m.Sample(true, 0x10);                                                     // low against 3.3 V
    // The check that writes 3.0 V: both its reads started at 3.3 V; the write reads back wrong.
    m.ReadStarted(); assert(m.LevelCheck(1, 2) == ReadBack::After30 && !m.Diag().expect_33);
    m.ReadBackDone(ReadBack::After30, true, kThreshold33);
    m.Sample(true, 0x10);
    assert(m.Diag().samples_33 == 2 && m.Diag().low_33 == 2 && m.Diag().samples_30 == 3);
    assert(m.Diag().mismatch_30 == 1 && m.Diag().mismatch_33 == 0 && m.Diag().after_30 == kThreshold33 && m.Diag().readback_valid == 3);
    m.ReadStarted(); assert(m.LevelCheck(2, 0) == ReadBack::None && m.Diag().level_checks == 1);
    m.Sample(true, 0x10); assert(m.Diag().samples_30 == 4 && m.Diag().low_run_30 == 1);
    // A charger hand-over read after the 3.3 V write counts at 3.3 V.
    m.LevelCheck(0, 1); m.ReadStarted(); m.Sample(true, 0x00); assert(m.Diag().samples_33 == 3 && m.Diag().low_run_30 == 1);
    // Periodic read-backs: once a second, never while a read is in flight; expected value follows the check.
    assert(!m.PeriodicDue(999, true) && !m.PeriodicDue(1000, false) && m.PeriodicDue(1000, true) && !m.PeriodicDue(1500, true));
    assert(m.Expected(ReadBack::Periodic) == kThreshold33);
    m.ReadBackDone(ReadBack::Periodic, true, kThreshold30); assert(m.Diag().mismatch_periodic == 1 && m.Diag().periodic == kThreshold30);
    m.ReadBackDone(ReadBack::Periodic, false, 0); assert(m.Diag().readback_errors == 1 && m.Diag().readback_valid == 7);
    m.ReadBackDone(ReadBack::None, true, 0); assert(m.Diag().readback_errors == 1 && m.Diag().mismatch_periodic == 1);
    assert(m.PeriodicDue(0xffffffffu, true) && m.PeriodicDue(1500, true));            // the millisecond clock wraps
}

// 0.19.2 fix: Forge's battery level check (core/power.h LevelCheck) on a simulated MP2722:
// register 0x0C picks the BATT_LOW threshold, register 0x16 bit 4 reports battery < threshold.
struct FakeCharger {
    float volts = 3.8f;
    uint8_t reg0c = LevelCheck::k30;
    int fail_writes = 0, fail_reads = 0;        // the next N writes / reads fail
    unsigned writes = 0;
    float Threshold() const {
        return reg0c == LevelCheck::k33 ? 3.3f : reg0c == LevelCheck::k31 ? 3.1f : 3.0f;
    }
    bool Write(uint8_t value) { ++writes; if(fail_writes > 0) { --fail_writes; return false; } reg0c = value; return true; }
    bool Low(bool& low) { if(fail_reads > 0) { --fail_reads; return false; } low = volts < Threshold(); return true; }
};
// Run 20 ms ticks for `ms`; returns the level and how many ticks a 3.3 / 3.1 V threshold was set
// while the check said the stock reads could run (must stay 0).
static Battery Run(LevelCheck& c, FakeCharger& f, uint32_t& now, uint32_t ms, Battery level, bool charging = false,
                   unsigned* leaks = nullptr) {
    for(uint32_t end = now + ms; now < end; now += 20) {
        if(!c.Paused() && f.reg0c != LevelCheck::k30 && leaks) ++*leaks;
        level = c.Tick(now, true, level, charging, f);
    }
    return level;
}
void LevelCheckBands() {
    {   // At power-on the first check runs at once (stock: yellow for 30 s), then every 30 s.
        LevelCheck c; FakeCharger f; uint32_t now = 0; unsigned leaks = 0;
        Battery level = Run(c, f, now, 200, Battery::Medium, false, &leaks);
        assert(level == Battery::High && !c.Paused() && f.reg0c == LevelCheck::k30 && c.Checks() == 1 && leaks == 0);
        f.volts = 3.2f;
        level = Run(c, f, now, 29000, level, false, &leaks);
        assert(level == Battery::High && c.Checks() == 1);                         // not yet
        level = Run(c, f, now, 2000, level, false, &leaks);
        assert(level == Battery::Medium && c.Checks() == 2 && leaks == 0);         // 3.1-3.3 V: yellow
        f.volts = 3.05f;
        level = Run(c, f, now, 31000, level, false, &leaks);
        assert(level == Battery::Low && f.reg0c == LevelCheck::k30 && leaks == 0); // below 3.1 V: red
    }
    {   // Charging lifts the voltage: the level does not rise while charging, it can drop.
        LevelCheck c; FakeCharger f; uint32_t now = 0; f.volts = 3.2f;
        Battery level = Run(c, f, now, 200, Battery::Medium);
        assert(level == Battery::Medium);
        f.volts = 4.1f;                                                            // plugged in, charging
        level = Run(c, f, now, 61000, level, true);
        assert(level == Battery::Medium && c.Checks() == 3);
        f.volts = 3.0f;
        level = Run(c, f, now, 31000, level, true);
        assert(level == Battery::Low);                                             // a drop shows
        f.volts = 3.9f;
        level = Run(c, f, now, 31000, level, false);
        assert(level == Battery::High);                                            // unplugged: it rises
        LevelCheck first; FakeCharger g; g.volts = 4.1f; uint32_t t = 0;           // first check on USB: taken
        assert(Run(first, g, t, 200, Battery::Medium, true) == Battery::High);
    }
    {   // Charge done (white) is the stock read's: no checks while Full.
        LevelCheck c; FakeCharger f; uint32_t now = 0;
        assert(Run(c, f, now, 61000, Battery::Full) == Battery::Full && f.writes == 0);
    }
    {   // The main loop's order (forge_main.cpp): level check, then the stock read when not paused.
        // The read completes within the 20 ms tick, so the check finds the bus idle the next time.
        LevelCheck c; FakeCharger f; f.volts = 3.5f; bool in_flight = false; Battery level = Battery::Medium;
        for(uint32_t now = 0; now < 300; now += 20) {
            in_flight = false;                                    // the read started last tick has completed
            level = c.Tick(now, !in_flight, level, false, f);
            if(!c.Paused()) in_flight = true;                     // hw.BMCPerformCheck()
        }
        assert(level == Battery::High && c.Checks() == 1);
    }
    {   // A busy bus (a stock DMA read in flight) delays the check; nothing is written meanwhile.
        LevelCheck c; FakeCharger f;
        for(uint32_t now = 0; now < 500; now += 20) assert(c.Tick(now, false, Battery::Medium, false, f) == Battery::Medium);
        assert(f.writes == 0 && !c.Paused());
    }
}
void LevelCheckFailures() {
    {   // The restore to 3.0 V is retried every tick until it sticks.
        LevelCheck c; FakeCharger f; uint32_t now = 0; f.volts = 3.5f;
        Battery level = Run(c, f, now, 60, Battery::Medium);                       // 3.3 V set, settling
        assert(c.Paused() && f.reg0c == LevelCheck::k33);
        f.fail_writes = 3;
        level = Run(c, f, now, 200, level);
        assert(!c.Paused() && f.reg0c == LevelCheck::k30 && level == Battery::High && c.Failures() == 3);
    }
    {   // If it never sticks, the check gives up after 1 s and the stock reads resume.
        LevelCheck c; FakeCharger f; uint32_t now = 0;
        Run(c, f, now, 60, Battery::Medium);
        f.fail_writes = 1000;
        Battery level = Run(c, f, now, 1100, Battery::Medium);
        assert(!c.Paused() && c.GivenUp() == 1 && level == Battery::Medium);
        level = Run(c, f, now, 800, level);                                         // reads run, no new check yet
        assert(!c.Paused() && c.GivenUp() == 1);
        f.fail_writes = 0;                                                          // the bus recovers
        level = Run(c, f, now, 400, level);
        assert(f.reg0c == LevelCheck::k30 && level == Battery::High);              // 3.0 V back, then a level
    }
    {   // A failed first write: the threshold is set back, no level from it.
        LevelCheck c; FakeCharger f; uint32_t now = 0; f.fail_writes = 1;
        Battery level = Run(c, f, now, 100, Battery::Medium);
        assert(!c.Paused() && f.reg0c == LevelCheck::k30 && level == Battery::Medium && c.Checks() == 0);
    }
    {   // A failed comparator read: no level, threshold restored.
        LevelCheck c; FakeCharger f; uint32_t now = 0; f.fail_reads = 1;
        Battery level = Run(c, f, now, 200, Battery::Medium);
        assert(!c.Paused() && f.reg0c == LevelCheck::k30 && level == Battery::Medium);
    }
    {   // The charger hand-over's read aborts a check: 3.0 V back at once, retried a second later.
        LevelCheck c; FakeCharger f; uint32_t now = 0; f.volts = 3.5f;
        Battery level = Run(c, f, now, 200, Battery::Medium);
        while(!c.Paused() && now < 40000) { level = c.Tick(now, true, level, false, f); now += 20; }   // next check starts
        assert(c.Paused());
        c.Abort(now, f);
        assert(!c.Paused() && f.reg0c == LevelCheck::k30);
        const uint32_t checks = c.Checks();
        level = Run(c, f, now, 1500, level);
        assert(c.Checks() == checks + 1 && level == Battery::High);
    }
    // Red is the near-empty warning on SW6 too.
    Readings pc; pc.input_limit = 0xff;
    assert(BatteryWarning(Battery::Low, pc) == Warning::Critical);
}

int main() {
    BatteryInstrumentation(); LevelCheckBands(); LevelCheckFailures();
    BootGestureOff(); Colours(); ChargerHandover(); StatusDecode(); RestartReason(); SafeMode(); OptionsFile(); InstallPower(); LockoutDip();
    std::cout << "PASS: power off gesture, battery colours, charger/USB hand-over (edges, timeout, wrap), status decode, restart reason, safe mode, options.json,"
                 " stock battery lockout, install power check, low-battery warning, supply flags, battery instrumentation, level check (bands, charging, failures)\n";
}
