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
// Sampler: a 2 s stereo noise sample in SDRAM behind every slot (worst case:
// stereo, cache-unfriendly reads).
constexpr uint32_t kSampleFrames = 96000;
int16_t SDRAM sample_data[2 * kSampleFrames];
forge::SampleTable samples;
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
    if(r.patch.Sampler()) {
        for(uint32_t i = 0; i < 2 * kSampleFrames; ++i) sample_data[i] = static_cast<int16_t>(Noise() * 100000.f);
        for(auto& slot : samples.slots) {
            slot.data = sample_data; slot.frames = kSampleFrames; slot.channels = 2; slot.rate_ratio = 1.f; slot.gain = 1.f;
            slot.loaded.store(kSampleFrames, std::memory_order_release);
        }
        engine->SetSamples(&samples);
    }
    if(!engine->ApplyPatch(r.patch)) return 3;
    // First four notes as before (v1-v3 results unchanged); v4 adds up to seven.
    static const uint8_t chord[7] = {48, 55, 60, 64, 67, 71, 72};
    static const uint8_t white[7] = {48, 50, 52, 53, 55, 57, 59};          // kit mode: one slot each
    const bool kit = r.patch.Sampler() && r.patch.sample_mode == 1;
    for(int i = 0; i < notes && i < 7; ++i) engine->Note(kit ? white[i] : chord[i], 100, 1);
    return 0;
}
void bench_block(int) {
    for(int i = 0; i < 24; ++i) engine->Process(Noise(), Noise(), out_l[i], out_r[i]);
}
}
