#pragma once
#include <cstdint>

namespace forge {
// Power behaviour taken from the stock firmwares (TAPE/WAVE/TEMPO chompi_main.cpp,
// TAPE NormalPage.h). The battery/charger IC (MP2722) itself is driven by the
// upstream hardware class; this is only the decision logic, so it can be tested.
namespace power {

// Start-up: all three stock firmwares scan the keys 5,000 times at 100 us (0.5 s)
// and, if CHOMPI + PLAY + LOOP (KEY_26, KEY_27, KEY_28) were held for more than
// 4,000 of them, put the charger IC into shipping mode (battery disconnected for
// storage; USB power wakes it). The hardware switch S1 is the normal on/off.
constexpr unsigned kBootScans = 5000, kBootScanUs = 100, kBootHoldThreshold = 4000;
class BootGesture {
public:
    void Scan(bool chompi, bool play, bool loop) { if(chompi && play && loop) ++held_; }
    bool PowerOff() const { return held_ > kBootHoldThreshold; }
private:
    unsigned held_ = 0;
};

// TAPE: holding the SW6 (volume) knob down for 2 s shows the battery on its light
// for as long as it stays down.
constexpr uint32_t kBatteryHoldMs = 2000;
enum class Battery : uint8_t { Full, High, Medium, Low, Unknown };   // upstream Hardware::BatteryLevel order
struct Colour { float r, g, b; };
inline Colour BatteryColour(Battery level) {
    switch(level) {                                 // TAPE NormalPage.h colours
        case Battery::Full: return {1.f, 1.f, 1.f};          // white: charged, on the charger
        case Battery::High: return {0.f, 1.f, 0.f};          // green: above the 3.3 V check
        case Battery::Medium: return {1.f, .95f, .05f};      // yellow: below 3.3 V
        case Battery::Low: return {1.f, 0.f, 0.f};           // red: below 3.1 V (0.19.2 LevelCheck)
        default: return {0.f, 0.f, 0.f};
    }
}

// TAPE's USB/charger hand-over (main loop). The charger IC pulls its interrupt line
// low on a power event (plug, unplug, charge state). Its status register 0x11 bits
// 7..4 (DPDM_STAT) are 0 while it has not identified the USB source: the D+/D- lines
// are then handed to the charger IC (usb_sw low) and, 1 ms later, detection is forced
// (register 0x0A = 0b00110100); once identified, the lines go back to the Daisy for
// USB MIDI (usb_sw high). TAPE waits for the I2C read without a limit; Forge gives up
// after kReadTimeoutMs.
class ChargerUsb {
public:
    static constexpr uint32_t kReadTimeoutMs = 50, kHandoffMs = 1;
    struct Actions {
        bool read = false;          // start a status read (MpReadAll)
        int8_t usb_to_daisy = -1;   // -1 leave, 0 lines to the charger IC, 1 lines to the Daisy
        bool force_detection = false;
    };
    // `status0` is register 0x11 from the last completed read; `read_ready` is true
    // once the read started for this edge has completed.
    // 0.19.2: `status1` is register 0x12 (bit 6 VIN_GD) from the latest completed read, the
    // 20 ms battery reads included. The interrupt is a short pulse the main loop can miss (the
    // owner, 2026-10-11: a charger plugged in while CHOMPI was on drew nothing until a restart),
    // so USB power appearing in those reads counts as an edge too, and lines left with the
    // charger IC go back to the Daisy once a read shows the source identified.
    Actions Poll(uint32_t now, bool interrupt_line, bool read_ready, uint8_t status0, uint8_t status1 = 0x40) {
        Actions a;
        bool edge = !interrupt_line && line_;
        line_ = interrupt_line;
        if(read_ready && !waiting_) {
            const int8_t vin = (status1 >> 6) & 1u;
            if(vin && vin_ == 0) edge = true;
            vin_ = vin;
            if(!lines_to_daisy_ && !handoff_ && (status0 & 0xF0u)) a.usb_to_daisy = 1;
        }
        if(waiting_) {
            if(read_ready) {
                waiting_ = false;
                if((status0 & 0xF0u) == 0) { a.usb_to_daisy = 0; handoff_ = true; handoff_at_ = now; }
                else a.usb_to_daisy = 1;
            } else if(now - asked_ > kReadTimeoutMs) waiting_ = false;           // read failed: next edge retries
        }
        if(edge) { a.read = true; waiting_ = true; asked_ = now; }
        if(handoff_ && now - handoff_at_ > kHandoffMs) { handoff_ = false; a.force_detection = true; }
        if(a.usb_to_daisy >= 0) lines_to_daisy_ = a.usb_to_daisy != 0;
        return a;
    }
    bool Handover() const { return handoff_; }
private:
    int8_t vin_ = -1;                       // USB power in the last read; -1 before the first
    bool lines_to_daisy_ = true;            // start-up leaves the lines with the Daisy
    bool line_ = true, waiting_ = false, handoff_ = false;
    uint32_t asked_ = 0, handoff_at_ = 0;
};

// The charger readings the upstream hardware class keeps as 8-reading histories (bit
// set = that reading had the flag; one reading every 20 ms in Forge's main loop).
struct Readings {
    uint8_t battery_low = 0;   // BATT_LOW: below the charger's low mark (3.0 V as Forge sets it)
    uint8_t usb_good = 0xff;   // VIN_GD: USB power present
    uint8_t legacy = 0;        // legacy (USB-A / no USB-C advertisement) source
    uint8_t input_limit = 0;   // IINDPM: the supply is at its current limit
};
// The stock battery protection (bootloader and upstream hardware class), as it reads
// those histories: low battery and no USB -> 15 s amber flashes, then shipping mode
// (the app; the bootloader switches off at once); low battery on a weak supply ->
// every light off and the processor stopped (app) or waiting (bootloader) until the
// power switch is cycled; the readings are not refreshed while it waits.
enum class Lockout : uint8_t { None, Unplugged, WeakSupply };
inline Lockout StockLockout(const Readings& r) {
    if(r.battery_low != 0xff) return Lockout::None;
    if(r.usb_good == 0x00) return Lockout::Unplugged;
    if(r.legacy == 0xff || r.input_limit == 0xff) return Lockout::WeakSupply;
    return Lockout::None;
}
// 0.19.2: the stock protection acts on 8 low readings in a row (160 ms at Forge's 20 ms
// checks), so a dip under load stopped CHOMPI: the owner's shut-offs while playing fast on
// a sampler sound on a computer's USB port (caught 2026-10-11: battery "high", then a reading
// below 3.0 V, then off within 0.6 s). In the main loop Forge takes the readings itself and
// hands over to the stock protection only once its condition has held for kSustainChecks
// checks in a row; start-up and the fault loops keep the stock check.
class LockoutGuard {
public:
    static constexpr uint32_t kCheckMs = 20, kSustainChecks = 150;   // 3 s
    Lockout Update(const Readings& r) {
        const Lockout l = StockLockout(r);
        if(l == Lockout::None) { held_ = 0; return l; }
        if(held_ < kSustainChecks) ++held_;
        return held_ >= kSustainChecks ? l : Lockout::None;
    }
private:
    uint32_t held_ = 0;
};
// 0.19.2 fix: Forge's own green / yellow / red check, in place of the stock BMCMediumBattCheck
// (docs/forge/BATTERY_READINGS.md, problems 1-4 and the colours). The MP2722 has no voltage
// reading: it only compares the battery with one threshold, register 0x0C, 3.0 / 3.1 / 3.2 / 3.3 V.
// So the level is a band: green above 3.3 V, yellow 3.1-3.3 V, red below 3.1 V; most of a cell's
// charge sits above 3.3 V, so green means "not getting low", not "full".
// - The first check runs at once after power-on (stock: yellow for the first 30 s), then every 30 s.
// - While a check's threshold (3.3 / 3.1 V) is set, the stock 20 ms reads pause (Paused()), so no
//   reading against it reaches the 3.0 V history the protection uses (stock: one to three every 30 s).
// - The comparator is read directly (register 0x16) once the threshold has settled, not whichever
//   buffered read happens to be there.
// - Every threshold write is checked (written, then read back); the 3.0 V restore is retried each
//   tick. A check that cannot finish within kGiveUpMs ends anyway and the stock reads resume (the
//   protection needs them).
// - Charging lifts the battery's voltage: while charging the level can drop but not rise, until the
//   charger reports charge done (white, stock) or USB is unplugged.
class LevelCheck {
public:
    static constexpr uint32_t kPeriodMs = 30000, kSettleMs = 40, kGiveUpMs = 1000, kRetryMs = 1000;
    static constexpr uint8_t k30 = 0b01010001, k31 = 0b01010101, k33 = 0b01011101;   // register 0x0C
    enum class State : uint8_t { Idle, At33, At31, Restore };
    // Io: bool Write(uint8_t value) (0x0C written and read back equal); bool Low(bool& low)
    // (register 0x16 bit 4 read). `idle`: no stock DMA read in flight. Returns the level to show.
    template<class Io> Battery Tick(uint32_t now, bool idle, Battery current, bool charging, Io& io) {
        if(state_ != State::Idle && now - began_ >= kGiveUpMs) {
            restore_pending_ = !io.Write(k30);
            if(restore_pending_) ++failures_;
            ++given_up_; Finish(now); result_ = Battery::Unknown;
            return current;
        }
        bool low = false;
        switch(state_) {
            case State::Idle:
                if(!idle) return current;
                if(restore_pending_) {                  // a check gave up: keep setting 3.0 V back, reads running
                    if(io.Write(k30)) restore_pending_ = false; else ++failures_;
                    return current;
                }
                // Next check: 30 s after a level, 1 s after a check that gave none (reads run meanwhile).
                if(current == Battery::Full || (tried_ && now - last_ < (measured_ ? kPeriodMs : kRetryMs))) return current;
                began_ = at_ = now;
                if(io.Write(k33)) state_ = State::At33; else Fail();
                return current;
            case State::At33:
                if(now - at_ < kSettleMs || !idle) return current;
                if(!io.Low(low)) { Fail(); return current; }
                if(!low) { result_ = Battery::High; state_ = State::Restore; }
                else if(io.Write(k31)) { at_ = now; state_ = State::At31; }
                else { result_ = Battery::Medium; Fail(); }
                return current;
            case State::At31:
                if(now - at_ < kSettleMs || !idle) return current;
                if(!io.Low(low)) { result_ = Battery::Medium; Fail(); return current; }
                result_ = low ? Battery::Low : Battery::Medium; state_ = State::Restore;
                return current;
            case State::Restore:
                if(!idle) return current;
                if(!io.Write(k30)) { ++failures_; return current; }
                Finish(now);
                return Apply(current, charging);
        }
        return current;
    }
    // The charger hand-over needs a status read now: set 3.0 V back and try again in a second.
    template<class Io> void Abort(uint32_t now, Io& io) {
        if(state_ == State::Idle) return;
        restore_pending_ = !io.Write(k30);
        if(restore_pending_) ++failures_;
        Finish(now); result_ = Battery::Unknown;
        if(measured_) last_ = now - kPeriodMs + kRetryMs;
    }
    bool Paused() const { return state_ != State::Idle; }      // the stock 20 ms reads wait
    State GetState() const { return state_; }
    uint32_t Checks() const { return checks_; }
    uint32_t Failures() const { return failures_; }
    uint32_t GivenUp() const { return given_up_; }
private:
    void Fail() { ++failures_; state_ = State::Restore; }
    void Finish(uint32_t now) { state_ = State::Idle; last_ = now; tried_ = true; }
    Battery Apply(Battery current, bool charging) {
        const Battery r = result_; result_ = Battery::Unknown;
        if(r == Battery::Unknown || current == Battery::Full) return current;
        ++checks_;
        // Battery order: Full, High, Medium, Low (a smaller value is fuller).
        if(charging && measured_ && static_cast<uint8_t>(r) < static_cast<uint8_t>(current)) return current;
        measured_ = true;
        return r;
    }
    State state_ = State::Idle;
    Battery result_ = Battery::Unknown;
    bool measured_ = false, tried_ = false, restore_pending_ = false;
    uint32_t began_ = 0, at_ = 0, last_ = 0, checks_ = 0, failures_ = 0, given_up_ = 0;
};

// A warning on SW6 before the stock protection acts (on a weak supply it gives none):
// Low = battery yellow (below 3.3 V) and not on a strong supply; Critical = red (below 3.1 V,
// 0.19.2) or a reading below the 3.0 V mark, the protection is near. Nothing on a strong supply (it charges).
enum class Warning : uint8_t { None, Low, Critical };
inline Warning BatteryWarning(Battery level, const Readings& r) {
    if(r.usb_good == 0xff && r.legacy == 0 && r.input_limit == 0) return Warning::None;
    if(r.battery_low != 0 || level == Battery::Low) return Warning::Critical;
    return level == Battery::Medium || level == Battery::Low ? Warning::Low : Warning::None;
}
// Low: two short blinks every 4 s; Critical: 4 Hz. In the stock lockout's yellow.
inline bool WarningLit(Warning w, uint32_t now_ms) {
    if(w == Warning::Critical) return (now_ms / 125) % 2 == 0;
    const uint32_t t = now_ms % 4000;
    return w == Warning::Low && (t < 120 || (t >= 240 && t < 360));
}
constexpr Colour kWarningColour{1.f, .95f, .05f};
// The line Forge appends to FORGE/RESTARTS.TXT just before the stock protection acts.
inline const char* LockoutText(Lockout l) {
    return l == Lockout::Unplugged ? "battery low, no USB power: 15 s amber flashes, then off (stock protection)"
         : l == Lockout::WeakSupply ? "battery low on a weak USB supply: lights off, stopped until switched off and on (stock protection)"
         : "";
}
// A firmware install restarts CHOMPI into the bootloader. Allow it only when that
// lockout cannot follow (the owner, 2026-10-05: a flat battery on a computer's USB port left
// CHOMPI dark after an install): the battery reads green or white with no recent low
// reading, or USB power is present on a supply that never hit its limit and is not a
// legacy source. Stricter than the lockout itself (any recent flag counts).
inline bool InstallPowerOk(Battery level, const Readings& r) {
    const bool charged = (level == Battery::Full || level == Battery::High) && r.battery_low == 0;
    const bool strong_supply = r.usb_good == 0xff && r.legacy == 0 && r.input_limit == 0;
    return charged || strong_supply;
}
// Inspector page 2 power flags from 0.12 (bits 0-2 are DecodeStatus's): 8 weak supply
// (legacy source or at its current limit in the last 8 readings), 16 a reading below
// the 3.0 V mark, 32 an install would be refused now.
inline uint8_t SupplyFlags(Battery level, const Readings& r) {
    const bool weak = r.usb_good == 0xff && (r.legacy != 0 || r.input_limit != 0);
    return static_cast<uint8_t>((weak ? 8u : 0u) | (r.battery_low != 0 ? 16u : 0u) | (InstallPowerOk(level, r) ? 0u : 32u));
}

// Charger status for the Inspector (development): register bytes from the upstream
// read (mp_buff_[0..5] = registers 0x11..0x16).
struct Status {
    Battery level = Battery::Unknown;
    bool usb_power = false;        // VIN_GD (reg 0x12 bit 6)
    uint8_t charge_state = 0;      // CHG_STAT (reg 0x13 bits 7..5); 5 = charge done (as upstream)
    bool fault = false;            // reg 0x14 non-zero (NTC/battery missing or faults: the factory test)
    bool usb_to_charger = false;   // D+/D- currently handed to the charger IC
};
inline Status DecodeStatus(const uint8_t (&reg)[6], Battery level, bool usb_to_charger) {
    Status s; s.level = level; s.usb_to_charger = usb_to_charger;
    s.usb_power = (reg[1] >> 6) & 1u;
    s.charge_state = static_cast<uint8_t>((reg[2] >> 5) & 7u);
    s.fault = reg[3] != 0;
    return s;
}
} // namespace power
} // namespace forge
