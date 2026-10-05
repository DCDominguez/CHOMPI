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
        case Battery::Low: return {1.f, 0.f, 0.f};           // red (upstream never reports it yet)
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
    Actions Poll(uint32_t now, bool interrupt_line, bool read_ready, uint8_t status0) {
        Actions a;
        const bool edge = !interrupt_line && line_;
        line_ = interrupt_line;
        if(waiting_) {
            if(read_ready) {
                waiting_ = false;
                if((status0 & 0xF0u) == 0) { a.usb_to_daisy = 0; handoff_ = true; handoff_at_ = now; }
                else a.usb_to_daisy = 1;
            } else if(now - asked_ > kReadTimeoutMs) waiting_ = false;           // read failed: next edge retries
        }
        if(edge) { a.read = true; waiting_ = true; asked_ = now; }
        if(handoff_ && now - handoff_at_ > kHandoffMs) { handoff_ = false; a.force_detection = true; }
        return a;
    }
    bool Handover() const { return handoff_; }
private:
    bool line_ = true, waiting_ = false, handoff_ = false;
    uint32_t asked_ = 0, handoff_at_ = 0;
};

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
