// WAVE 1.0 synth engine for the CPU benchmark (emulator only, not firmware).
// Runs WAVE's own myEngine::Process (8 wavetable voices with per-voice DjFilter
// and ADSR, filter and pitch LFOs, delay or reverb, limiters, soft clip, pan)
// exactly as chompi_main.cpp does each block, with all 8 voices sounding.
// The wavetable is a sine in SDRAM instead of a file from SD. bench_init(0):
// delay side of WAVE's delay/reverb knob; bench_init(1): reverb side.
#include <cmath>
#include <cstdint>
#include <new>
// Same headers, order and using-directives as WAVE's chompi_main.cpp.
#include "hardware.h"
#include "temp_led_stuff.h"
#include "ui.h"
#include "daisysp.h"
#include "fatfs.h"
#include "diskio.h"
#include "subtractiveEngine.h"
#include "Sequencer.h"
#include "InterpolatedDelayLine.h"
#include "OptionsManager.h"
#include "WavetableManager.h"
#include "clockManager.h"
#include "MidiManager.h"
#define MAX_CYCLES 256              // chompi_main.cpp
using namespace daisy;
using namespace chompi;

namespace {
chompi::InterpolatedDelayLine::AudioSample __attribute__((section(".sdram_bss"))) del_mem[kMaxDelayTime];
float wavetable_memory[MAX_CYCLES][MAX_SAMPLES_PER_CYCLE] __attribute__((section(".sdram_bss")));
daisysp::Reverb __attribute__((section(".dtcmram_bss"))) reverb;   // DTCM, as in WAVE
alignas(myEngine) unsigned char engine_storage[sizeof(myEngine)];
alignas(wavetableLoader) unsigned char loader_storage[sizeof(wavetableLoader)];
myEngine* engine;
float in0[24], in1[24], in2[24], in3[24];
}

extern "C" {
float out0[24], out1[24], out2[24], out3[24];
int bench_init(int reverb_side) {
    for(size_t i = 0; i < MAX_SAMPLES_PER_CYCLE; ++i)
        wavetable_memory[0][i] = std::sin(6.2831853f * i / MAX_SAMPLES_PER_CYCLE);
    auto* loader = new(loader_storage) wavetableLoader();
    loader->wavetableMemory_ = wavetable_memory;
    new(&reverb) daisysp::Reverb();
    engine = new(engine_storage) myEngine();
    engine->Init(48000.f, &del_mem[0], &reverb, loader);
    // Busy setting: both LFOs on with depth, resonant filter, limiter engaged.
    engine->setPitchLfoOn(true); engine->setFilterLfoOn(true);
    engine->setLfoDepth(.5f); engine->setPitchLfoDepth(.2f);
    engine->setMasterCutoff(.4f); engine->setMasterResonance(.6f);
    engine->setDelayTime(.4f); engine->setDelayFeedback(reverb_side ? .85f : .15f);
    engine->setFinalComp(.4f); engine->setAttack(.01f); engine->setRelease(.3f);
    engine->setGain(.8f);   // the UI sets this from saved options at boot
    static const float hz[8] = {130.8f, 164.8f, 196.f, 246.9f, 261.6f, 329.6f, 392.f, 493.9f};
    for(int v = 0; v < NUM_VOICES; ++v) {
        engine->myVoices[v].setFrequency(hz[v]);
        engine->myVoices[v].velocity = 1.f;
        engine->myVoices[v].gate = true;
    }
    return 0;
}
void bench_block(int) {
    const float* in[4] = {in0, in1, in2, in3};
    float* out[4] = {out0, out1, out2, out3};
    engine->Process(in, out, 24);
}
}
