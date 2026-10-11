#pragma once
#include <cstdint>

namespace forge {
// Battery reading instrumentation (development builds only: forge_main.cpp uses it under
// FORGE_TEST_HOOKS; Inspector page 11). It observes the stock MP2722 reads and level check
// (chompi-wave hardware.h, untouched) and changes nothing they do. docs/forge/BATTERY_READINGS.md
// "Instrumented build" explains the fields. Pure logic, so it can be tested on the host.
namespace power {

// The values the stock code writes to register 0x0C (its comments: BATT_LOW at 3.0 V / 3.3 V).
constexpr uint8_t kThresholdRegister = 0x0c, kThreshold30 = 0b01010001, kThreshold33 = 0b01011101;
enum class ReadBack : uint8_t { None, After33, After30, Periodic };

struct BatteryDiag {
    uint32_t uptime_ms = 0;
    uint8_t reg[6]{};                 // registers 0x11..0x16 as last read (mp_buff_)
    uint8_t battery_low = 0, usb_good = 0, legacy = 0, input_limit = 0;   // the stock 8-reading histories
    uint8_t level = 4, check_state = 0;                                   // batt_level, batt_check_state
    bool read_ready = false;          // the last started read completed without an I2C error
    bool expect_33 = false;           // Forge's view: the stock level check has set the 3.3 V threshold
    uint8_t after_33 = 0, after_30 = 0, periodic = 0;   // 0x0C read back after each write / once a second
    uint8_t readback_valid = 0;       // bit 0 after_33, 1 after_30, 2 periodic: read at least once
    uint32_t mismatch_33 = 0, mismatch_30 = 0, mismatch_periodic = 0, readback_errors = 0;
    uint32_t checks = 0, not_ready = 0;                // 20 ms checks; of them, no completed read to count
    uint32_t samples_30 = 0, low_30 = 0, samples_33 = 0, low_33 = 0;   // BATT_LOW per expected threshold
    uint32_t low_run_30 = 0, low_run_30_max = 0;       // consecutive low readings at 3.0 V (now, longest)
    uint32_t level_checks = 0;                         // completed level decisions (state 2 -> 0)
};

// Main loop only. Order per 20 ms check: Sample(), ReadStarted() just before the stock read,
// LevelCheck() around BMCMediumBattCheck; ReadStarted() also for any other stock read (the
// charger hand-over); ReadBackDone() after each register 0x0C read-back.
class BatteryMonitor {
public:
    static constexpr uint32_t kPeriodicMs = 1000;
    // The latest completed read, counted under the threshold that applied when it started.
    void Sample(bool read_ready, uint8_t reg16) {
        ++d_.checks;
        if(!read_ready) { ++d_.not_ready; return; }
        const bool low = (reg16 >> 4) & 1u;
        if(read_33_) { ++d_.samples_33; if(low) ++d_.low_33; return; }
        ++d_.samples_30;
        if(low) { if(++d_.low_run_30 > d_.low_run_30_max) d_.low_run_30_max = d_.low_run_30; ++d_.low_30; }
        else d_.low_run_30 = 0;
    }
    void ReadStarted() { read_33_ = d_.expect_33; }
    // The stock check's state before and after BMCMediumBattCheck: which read-back is due now.
    ReadBack LevelCheck(uint8_t before, uint8_t after) {
        if(before == 0 && after == 1) { d_.expect_33 = true; return ReadBack::After33; }
        if(before == 1 && after == 2) { d_.expect_33 = false; return ReadBack::After30; }
        if(before == 2 && after == 0) ++d_.level_checks;
        return ReadBack::None;
    }
    // Once a second, when no stock read is in flight.
    bool PeriodicDue(uint32_t now, bool read_ready) {
        if(!read_ready || now - periodic_at_ < kPeriodicMs) return false;
        periodic_at_ = now; return true;
    }
    uint8_t Expected(ReadBack kind) const {
        return kind == ReadBack::After33 ? kThreshold33 : kind == ReadBack::After30 ? kThreshold30
             : d_.expect_33 ? kThreshold33 : kThreshold30;
    }
    void ReadBackDone(ReadBack kind, bool ok, uint8_t value) {
        if(kind == ReadBack::None) return;
        if(!ok) { ++d_.readback_errors; return; }
        const bool mismatch = value != Expected(kind);
        if(kind == ReadBack::After33) { d_.after_33 = value; d_.readback_valid |= 1; d_.mismatch_33 += mismatch; }
        else if(kind == ReadBack::After30) { d_.after_30 = value; d_.readback_valid |= 2; d_.mismatch_30 += mismatch; }
        else { d_.periodic = value; d_.readback_valid |= 4; d_.mismatch_periodic += mismatch; }
    }
    const BatteryDiag& Diag() const { return d_; }
private:
    BatteryDiag d_;
    bool read_33_ = false;           // threshold of the read started last
    uint32_t periodic_at_ = 0;
};
} // namespace power
} // namespace forge
