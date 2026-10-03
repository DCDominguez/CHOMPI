// TEMPO 1.0 per-block DSP for the CPU benchmark (emulator only, not firmware).
// Calls TEMPO's own fxEngine::Process and ::ApplyOutputFX (granular delay,
// Clouds-style reverb, compressor, limiters, soft clip) exactly as
// chompi_main.cpp does each block. The chromatic and slice sample engines feed
// fxEngine from SD-streamed sample buffers; here noise stands in for their
// output, so this is a LOWER BOUND of TEMPO's real audio-callback load.
#include <cstdint>
#include <new>
// Same headers, order and using-directives as TEMPO's chompi_main.cpp (its
// headers depend on that order).
#include "hardware.h"
#include "temp_led_stuff.h"
#include "ui.h"
#include "daisysp.h"
#include "fatfs.h"
#include "diskio.h"
#include "OptionsManager.h"
#include "EngineBase.h"
#include "SampleEngine.h"
#include "SliceEngine.h"
#include "SampleManager.h"
#include "ArpeggiatorSequencer.h"
#include "clockManager.h"
#include "MidiManager.h"
#include "FxEngine.h"
#include "granularDelay.h"
#include "StateSaver.h"
#include "reverb.h"
using namespace daisy;
using namespace chompi;

namespace {
constexpr size_t kBufferSize = 480000;   // chompi_main.cpp
float __attribute__((section(".sdram_bss"))) granularBuffer[kBufferSize * 2];
float __attribute__((section(".sdram_bss"))) frozenBuffer[kBufferSize * 2];
Reverb __attribute__((section(".dtcmram_bss"))) reverb;   // DTCM, as in TEMPO
alignas(fxEngine) unsigned char fx_storage[sizeof(fxEngine)];
alignas(granularDelay) unsigned char delay_storage[sizeof(granularDelay)];
alignas(clockManager) unsigned char clock_storage[sizeof(clockManager)];
alignas(daisy::TimerHandle) unsigned char timer_storage[sizeof(daisy::TimerHandle)];
fxEngine* fx;
float chroma[2][24], slice[2][24], in0[24], in1[24], in2[24], in3[24];
uint32_t seed = 1;
float Noise() { seed = seed * 1664525u + 1013904223u; return static_cast<int32_t>(seed) * (0.25f / 2147483648.f); }
}

extern "C" {
float out0[24], out1[24], out2[24], out3[24];
int bench_init(int) {
    auto* timer = new(timer_storage) daisy::TimerHandle();
    auto* clock = new(clock_storage) clockManager();
    auto* delay = new(delay_storage) granularDelay();
    new(&reverb) Reverb();
    fx = new(fx_storage) fxEngine();
    reverb.Init(48000.f);
    delay->Init(granularBuffer, frozenBuffer, kBufferSize, clock, false);
    fx->Init(nullptr, 48000.f, delay, &reverb, 0);
    clock->Init(timer, 240000000);   // timer register writes land in emulator scratch memory
    // Busy, typical performance setting: granular "reverb delay" side of the main
    // knob (delay + reverb + boost), feedback with compression, half wet mix, limiter.
    fx->setGranularMain(.8f);
    fx->setGranularAlt(.5f);
    fx->setGranularFeedback(.8f);
    fx->setGranularMix(.6f, 0);
    fx->setGranularMix(.6f, 1);
    fx->setFinalComp(.4f);
    return 0;
}
void bench_block(int) {
    for(int i = 0; i < 24; ++i) {
        chroma[0][i] = Noise(); chroma[1][i] = Noise(); slice[0][i] = Noise(); slice[1][i] = Noise();
    }
    const float* in[4] = {in0, in1, in2, in3};
    float* out[4] = {out0, out1, out2, out3};
    float* chroma_buff[2] = {chroma[0], chroma[1]};
    float* slice_buff[2] = {slice[0], slice[1]};
    fx->Process(in, out, 24, chroma_buff, slice_buff);
    fx->ApplyOutputFX(in, out, 24);
}
}
