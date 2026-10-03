// CPU benchmark entry points for the Forge engine (emulator only, not firmware).
// The runner writes a real SysEx apply request (encoded by forge_host.py from a
// preset file) into `request`, then calls bench_init / bench_block.
#include <cmath>
#include <cstdint>
#include <new>
#include "../core/runtime.h"

#define SDRAM __attribute__((section(".sdram_bss")))
namespace {
constexpr size_t kDelay = 48002;
float SDRAM delay_l[kDelay];
float SDRAM delay_r[kDelay];
float SDRAM reverb_mem[8704];
alignas(forge::Engine) unsigned char engine_storage[sizeof(forge::Engine)];
forge::Engine* engine;
uint32_t seed = 1;
float Noise() { seed = seed * 1664525u + 1013904223u; return static_cast<int32_t>(seed) * (0.25f / 2147483648.f); }
}
extern "C" {
uint8_t request[forge::kMaxRequest];
uint32_t request_size;
float out_l[24], out_r[24];
// Returns 0 on success. notes: how many voices to start (chord from MIDI 48).
int bench_init(int notes) {
    engine = new(engine_storage) forge::Engine();
    if(!engine->Init(48000.f, delay_l, delay_r, kDelay, reverb_mem, 8704)) return 1;
    forge::Request r;
    if(forge::DecodeRequest(request, request_size, r) != forge::Error::None) return 2;
    if(!engine->ApplyPatch(r.patch)) return 3;
    static const uint8_t chord[4] = {48, 55, 60, 64};
    for(int i = 0; i < notes && i < 4; ++i) engine->Note(chord[i], 100, 1);
    return 0;
}
void bench_block(int) {
    for(int i = 0; i < 24; ++i) engine->Process(Noise(), Noise(), out_l[i], out_r[i]);
}
}
