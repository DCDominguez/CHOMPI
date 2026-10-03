// TAPE 2.0 per-block DSP for the CPU benchmark (emulator only, not firmware).
// Uses TAPE's own classes from firmware/chompi-tape (read-only) and reproduces
// DSPEngine::ApplyFx and the "main gain control" loop of DSPEngine::Process
// statement for statement. Not included: the 7 sample voices (they need SD
// streaming state), the looper and input monitoring, so this is a LOWER BOUND
// of TAPE's real audio-callback load.
#include <algorithm>
#include <cstdint>
#include <new>
#include "daisy_core.h"
#include "daisysp.h"
#include "DJFilter.h"
#include "Warble.h"
#include "InterpolatedDelayLine.h"
#include "reverb.h"
#include "limiter.h"
#include "EnvFollower.h"

using namespace daisysp;
using chompi::InterpolatedDelayLine;

namespace {
constexpr size_t kMaxDelayTime = 48128 * 2;      // DSPEngine.h
constexpr float kLineOutGain = .3f, kHpGain = .2f; // DSPEngine.h
InterpolatedDelayLine::AudioSample __attribute__((section(".sdram_bss"))) del_mem[kMaxDelayTime];

struct TapeFx {
    daisysp::Reverb* reverb_;
    InterpolatedDelayLine del_;
    DjFilter filter_;
    chompi::Warble warble_;
    daisysp::DcBlock dcblock_fx_l_, dcblock_fx_r_;
    chompi::Limiter lim_hp_l_, lim_hp_r_, lim_line_l_, lim_line_r_;
    chompi::EnvFollower output_env_follower;
    float cutoff_, cutoff_target_, res_, res_target_, saturate_amt_, saturate_amt_target_;
    float fx_env_, fx_env_target_, dly_feedback_, dly_feedback_target_, dly_time_, dly_time_target_;
    float dly_amt_, dly_amt_target_, reverb_amt_, reverb_amt_target_, reverb_time_, reverb_time_target_;
    float mgain_, mgain_target_, ingain_, ingain_target_, final_lim_, final_lim_target_;

    void Init(float samplerate, daisysp::Reverb* reverb) {   // from DSPEngine::Init
        output_env_follower.Init();
        reverb_ = reverb;
        reverb_->Init(samplerate);
        reverb_->SetAmount(0.f);
        reverb_->SetInputGain(.3f);
        reverb_->SetLowpass(1.f);
        del_.Init(del_mem, kMaxDelayTime);
        del_.SetDelay(kMaxDelayTime * .5f);
        filter_.Init(samplerate);
        filter_.SetControl(.5f);
        warble_.Init(samplerate);
        warble_.SetFreq(.1f);
        dcblock_fx_l_.Init(samplerate);
        dcblock_fx_r_.Init(samplerate);
        lim_hp_l_.Init(); lim_hp_r_.Init(); lim_line_l_.Init(); lim_line_r_.Init();
        // A busy but typical setting: filter off-centre with resonance, saturation,
        // warble, delay and reverb all active (worst case for this stage).
        cutoff_ = cutoff_target_ = .3f; res_ = res_target_ = .5f;
        saturate_amt_ = saturate_amt_target_ = 1.5f;
        fx_env_ = fx_env_target_ = 1.f;
        dly_feedback_ = dly_feedback_target_ = .6f;
        dly_time_ = dly_time_target_ = kMaxDelayTime * .3f;
        dly_amt_ = dly_amt_target_ = .5f;
        reverb_amt_ = reverb_amt_target_ = .6f; reverb_time_ = reverb_time_target_ = .7f;
        mgain_ = mgain_target_ = .8f; ingain_ = ingain_target_ = .75f;
        final_lim_ = final_lim_target_ = .5f;
        warble_.SetFreq(.5f);
    }

    // DSPEngine::ApplyFx, verbatim apart from member declarations above.
    void ApplyFx(float* outl, float* outr, size_t size) {
        for(size_t i = 0; i < size; i++) {
            fonepole(cutoff_, cutoff_target_, .001f);
            fonepole(res_, res_target_, .001f);
            fonepole(saturate_amt_, saturate_amt_target_, .001f);
            fonepole(fx_env_, fx_env_target_, .001f);
            outl[i] *= fx_env_;
            outr[i] *= fx_env_;
            filter_.SetControl(cutoff_);
            filter_.SetRes(res_);
            outl[i] = dcblock_fx_l_.Process(outl[i]);
            outr[i] = dcblock_fx_r_.Process(outr[i]);
            filter_.Process(outl[i], outr[i], &outl[i], &outr[i]);
            outl[i] = daisysp::SoftClip(saturate_amt_ * outl[i]);
            outr[i] = daisysp::SoftClip(saturate_amt_ * outr[i]);
            const float gain = 1.f - daisysp::SoftClip(.4f * (saturate_amt_ - 1.f)) * .7f;
            outl[i] *= gain;
            outr[i] *= gain;
            warble_.Process(outl[i], outr[i], &outl[i], &outr[i]);
        }
        for(size_t i = 0; i < size; i++) {
            fonepole(dly_feedback_, dly_feedback_target_, .001f);
            fonepole(dly_time_, dly_time_target_, .001f);
            fonepole(dly_amt_, dly_amt_target_, .001f);
            del_.SetDelay(dly_time_);
            const float del_vol = dly_feedback_ < .2f ? dly_feedback_ * 5.f : 1.f;
            InterpolatedDelayLine::AudioSample del_read = del_.Read();
            const float delsig_l = s162f(del_read.l) * del_vol;
            const float delsig_r = s162f(del_read.r) * del_vol;
            const float mono_sum = (outl[i] + outr[i]) * .5f;
            const float del_in = mono_sum + delsig_r * powf(dly_feedback_, .7f);
            const InterpolatedDelayLine::AudioSample del_write = {int16_t(f2s16(del_in)), int16_t(f2s16(delsig_l))};
            del_.Write(del_write);
            const float wet_mix = dly_feedback_ > .25f ? .5f : 2.f * dly_feedback_;
            const float dry_mix = dly_feedback_ > .83f ? .5f : (1 - .6f * dly_feedback_);
            outl[i] = outl[i] * dry_mix + delsig_l * wet_mix;
            outr[i] = outr[i] * dry_mix + delsig_r * wet_mix;
        }
        for(size_t i = 0; i < size; i++) {
            fonepole(reverb_amt_, reverb_amt_target_, .001f);
            fonepole(reverb_time_, reverb_time_target_, .001f);
            reverb_->SetAmount(reverb_amt_ * reverb_amt_ * .8f);
            reverb_->SetTime(reverb_time_);
            reverb_->SetLowpass(reverb_amt_ * .6f + .4f);
            reverb_->SetDiffusion(reverb_amt_ * .6f);
            reverb_->Process(&outl[i], &outr[i]);
        }
    }

    // The voice-sum soft clip, ApplyFx, line-out copy and "main gain control"
    // loop of DSPEngine::Process (voices, looper, monitoring and record omitted).
    void Process(float** out, size_t size) {
        for(size_t i = 0; i < size; i++) {
            out[0][i] = daisysp::SoftClip(out[0][i]);
            out[1][i] = daisysp::SoftClip(out[1][i]);
        }
        ApplyFx(out[0], out[1], size);
        std::copy(out[0], out[0] + size, out[2]);
        std::copy(out[1], out[1] + size, out[3]);
        for(size_t i = 0; i < size; i++) {
            fonepole(mgain_, mgain_target_, .001f);
            fonepole(ingain_, ingain_target_, .001f);
            fonepole(final_lim_, final_lim_target_, .001f);
            out[0][i] *= kHpGain * mgain_;
            out[1][i] *= kHpGain * mgain_;
            out[2][i] *= kLineOutGain * mgain_;
            out[3][i] *= kLineOutGain * mgain_;
            const float thresh = 1.f / (10.f * final_lim_ + 4.f);
            const float ratio = 1.f + final_lim_ * final_lim_ * 7.f;
            const float makeup = .9f + final_lim_ * .6f;
            const float pregain = 7.f * final_lim_ + 1.f;
            out[0][i] = lim_hp_l_.ProcessComp(out[0][i], pregain, thresh, ratio, makeup);
            out[1][i] = lim_hp_r_.ProcessComp(out[1][i], pregain, thresh, ratio, makeup);
            out[2][i] = lim_line_l_.ProcessComp(out[2][i], pregain, thresh, ratio, makeup);
            out[3][i] = lim_line_r_.ProcessComp(out[3][i], pregain, thresh, ratio, makeup);
            output_env_follower.Process((out[0][i] + out[1][i]));
        }
    }
};
daisysp::Reverb __attribute__((section(".dtcmram_bss"))) reverb;   // DTCM, as in TAPE's chompi_main.cpp
alignas(TapeFx) unsigned char storage[sizeof(TapeFx)];
TapeFx* fx;
uint32_t seed = 1;
float Noise() { seed = seed * 1664525u + 1013904223u; return static_cast<int32_t>(seed) * (0.25f / 2147483648.f); }
}

extern "C" {
float out0[24], out1[24], out2[24], out3[24];
int bench_init(int) {
    new(&reverb) daisysp::Reverb();
    fx = new(storage) TapeFx();
    fx->Init(48000.f, &reverb);
    return 0;
}
void bench_block(int) {
    float* out[4] = {out0, out1, out2, out3};
    for(int i = 0; i < 24; ++i) { out0[i] = Noise(); out1[i] = Noise(); }   // stands in for the voice sum
    fx->Process(out, 24);
}
}
