#pragma once
#include <cstdint>
#include "parameters.h"

// Why CHOMPI last started (DC's "random shut-off", 2026-10-05): the STM32H7 reset
// flags (RCC_RSR) read once at start-up, and a crash record that Forge's fault handler
// leaves in backup SRAM before it restarts the chip. Backup SRAM survives a reset but
// not a power loss, so a crash shows up only if power stayed on. Pure decoding here;
// forge_main.cpp reads the registers.
namespace forge {
namespace restart {

// RCC_RSR bit positions (RM0433 8.7.43).
constexpr uint32_t kLowPower = 1u << 30, kWindowWatchdog = 1u << 28, kWatchdog = 1u << 26, kSoftware = 1u << 24,
                   kPowerOn = 1u << 23, kPin = 1u << 22, kBrownOut = 1u << 21;
// Compact flags for the Inspector and the log: bit 0 power-on, 1 brown-out (power dip),
// 2 reset pin, 3 software (Forge's own restart: firmware install or after a crash),
// 4 watchdog, 5 window watchdog, 6 low-power.
inline uint8_t Flags(uint32_t rsr) {
    return static_cast<uint8_t>((rsr & kPowerOn ? 1u : 0u) | (rsr & kBrownOut ? 2u : 0u) | (rsr & kPin ? 4u : 0u)
                              | (rsr & kSoftware ? 8u : 0u) | (rsr & kWatchdog ? 16u : 0u)
                              | (rsr & kWindowWatchdog ? 32u : 0u) | (rsr & kLowPower ? 64u : 0u));
}

// Left in backup SRAM by the fault handler (magic marks it valid).
struct FaultRecord {
    static constexpr uint32_t kMagic = 0x464f5247u;   // "FORG"
    uint32_t magic, pc, lr, cfsr, hfsr, count;
    bool Valid() const { return magic == kMagic; }
};

// One line for FORGE/RESTARTS.TXT, e.g. "boot 3: power-on brown-out; crash pc=0x24012345 cfsr=0x00008200".
// Returns the length written (always NUL-terminated, at most `size` - 1 characters).
FORGE_COLD inline unsigned Describe(uint32_t boot, uint8_t flags, const FaultRecord* fault, char* out, unsigned size) {
    unsigned n = 0;
    auto put = [&](const char* s) { while(*s && n + 1 < size) out[n++] = *s++; };
    auto hex = [&](uint32_t v) { char b[11] = "0x00000000"; for(int i = 0; i < 8; ++i) b[9 - i] = "0123456789abcdef"[(v >> (4 * i)) & 15u]; put(b); };
    auto dec = [&](uint32_t v) { char b[11]; int i = 10; b[i] = 0; do { b[--i] = static_cast<char>('0' + v % 10); v /= 10; } while(v && i > 0); put(b + i); };
    static const char* const names[7] = {" power-on", " brown-out", " reset-pin", " software", " watchdog", " window-watchdog", " low-power"};
    put("boot "); dec(boot); put(":");
    if(!flags) put(" (no reset flags)");
    for(unsigned i = 0; i < 7; ++i) if((flags >> i) & 1u) put(names[i]);
    if(fault && fault->Valid()) { put("; crash pc="); hex(fault->pc); put(" lr="); hex(fault->lr); put(" cfsr="); hex(fault->cfsr); put(" hfsr="); hex(fault->hfsr); }
    put("\n");
    if(size) out[n] = 0;
    return n;
}
// A line for an event during a start, e.g. "boot 3: battery low on a weak USB supply: ...".
inline unsigned DescribeEvent(uint32_t boot, const char* text, char* out, unsigned size) {
    unsigned n = 0;
    auto put = [&](const char* s) { while(*s && n + 1 < size) out[n++] = *s++; };
    char b[11]; int i = 10; b[i] = 0; do { b[--i] = static_cast<char>('0' + boot % 10); boot /= 10; } while(boot && i > 0);
    put("boot "); put(b + i); put(": "); put(text); put("\n");
    if(size) out[n] = 0;
    return n;
}
} // namespace restart
} // namespace forge
