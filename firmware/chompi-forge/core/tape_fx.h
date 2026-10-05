#pragma once
#include <cmath>
#include <cstdint>
#include "parameters.h"

// TAPE's effects (stock DSPEngine::ApplyFx and the final compressor), ported
// for Forge: DJ filter (low-pass below .5, high-pass above), saturation, warble
// (wow and flutter) and the output compressor. Same formulas and smoothing as
// TAPE; each stage is skipped while it is neutral and settled, so patches that
// never touch them cost (almost) nothing. Audio owner only; no allocation.
namespace forge {
namespace tape {

inline void OnePole(float& out, float in, float coefficient) { out += coefficient * (in - out); }
inline float SoftLimit(float x) { return x * (27.f + x * x) / (27.f + 9.f * x * x); }
inline float SoftClip(float x) { return x < -3.f ? -1.f : x > 3.f ? 1.f : SoftLimit(x); }

// chompi::BasicMMF: 2-pole filter, freq 0..1 (1 = Nyquist). The coefficients are
// shared (Coefficients) so a stereo pair computes them once.
struct MmfCoefficients {
    float freq = .99f, feedback = 0.f;
    void Set(float f, float res) { freq = f; feedback = res + res / (1.f - f); }
};
class Mmf {
public:
    float Low(float in, const MmfCoefficients& c) { Step(in, c); return buf1_; }
    float High(float in, const MmfCoefficients& c) { Step(in, c); return in - buf0_; }
    void Clear() { buf0_ = buf1_ = 0.f; }
private:
    void Step(float in, const MmfCoefficients& c) {
        buf0_ += c.freq * (in - buf0_ + c.feedback * (buf0_ - buf1_));
        buf1_ += c.freq * (buf0_ - buf1_);
        if(std::fabs(buf0_) < 1e-20f) buf0_ = 0.f;
        if(std::fabs(buf1_) < 1e-20f) buf1_ = 0.f;
    }
    float buf0_ = 0.f, buf1_ = 0.f;
};

// TAPE's DjFilter: low-pass then high-pass, one control. Only the side in use runs
// (TAPE runs both; the open side passes the signal almost unchanged), and the
// coefficients are recomputed only while the control glides.
class DjFilter {
public:
    void SetControl(float control, float resonance) {
        control_ = control; res_ = resonance * .95f;
        lp_target_ = Clamp(.01f + control * 2.f, 0.f, .99f); lp_target_ = lp_target_ * lp_target_ * lp_target_;
        hp_target_ = Clamp(control * 1.9f - 1.f, 0.f, 1.f); hp_target_ = hp_target_ * hp_target_ * hp_target_;
        moving_ = true;
    }
    bool Neutral() const { return lp_target_ > .96f && hp_target_ == 0.f && lp_ > .96f && hp_ < 1e-4f; }
    void Process(float& l, float& r) {
        if(moving_) {
            OnePole(lp_, lp_target_, .0002f); OnePole(hp_, hp_target_, .0002f);
            if(std::fabs(lp_ - lp_target_) < 1e-6f && std::fabs(hp_ - hp_target_) < 1e-6f) { lp_ = lp_target_; hp_ = hp_target_; moving_ = false; }
            lp_c_.Set(lp_, res_);
            hp_c_.Set(hp_, hp_ > .8f ? res_ * 5.f * (1.f - control_) : res_);
        }
        if(lp_ < .96f) { l = lpl_.Low(l, lp_c_); r = lpr_.Low(r, lp_c_); }     // low-pass side
        if(hp_ > 1e-4f) { l = hpl_.High(l, hp_c_); r = hpr_.High(r, hp_c_); }  // high-pass side
    }
    void Clear() { lpl_.Clear(); lpr_.Clear(); hpl_.Clear(); hpr_.Clear(); }
private:
    Mmf lpl_, lpr_, hpl_, hpr_;
    MmfCoefficients lp_c_, hp_c_;
    bool moving_ = false;
    float control_ = .5f, res_ = 0.f, lp_ = .97f, lp_target_ = .97f, hp_ = 0.f, hp_target_ = 0.f;
};

// TAPE's Warble: a short delay line whose length wanders (vinyl wow/flutter). Its
// 2 x 1024-float line is the caller's memory (zero-initialised, so it costs no image
// space); without memory the warble is off.
class Warble {
public:
    static constexpr uint32_t kLength = 1024;
    void SetMemory(float* memory) { left_ = memory; right_ = memory ? memory + kLength : nullptr; }
    void Set(float amount) { mix_target_ = amount; frequency_ = amount * 30.f + .1f; }
    bool Neutral() const { return !left_ || (mix_target_ == 0.f && mix_ < 1e-4f); }
    void Process(float& l, float& r) {
        constexpr float kInv31 = 4.656612873077392578125e-10f, kOneOverRate = 2.082725e-5f;
        if(static_cast<float>(Rand()) * kInv31 < kOneOverRate * frequency_) {
            end_ = 100.f + static_cast<float>(Rand()) * kInv31 * 880.f;
            coefficient_ = static_cast<float>(Rand()) * kInv31 * .0001f;
        }
        OnePole(length_, end_, coefficient_);
        OnePole(mix_, mix_target_, .001f);
        left_[write_] = l; right_[write_] = r;
        // Linear read `length_` frames back (DaisySP DelayLine).
        const float d = Clamp(length_, 1.f, static_cast<float>(kLength - 2));
        const int32_t whole = static_cast<int32_t>(d);
        const float frac = d - static_cast<float>(whole);
        const uint32_t a = (write_ + kLength - static_cast<uint32_t>(whole)) & (kLength - 1), b = (a + kLength - 1) & (kLength - 1);
        const float dl = left_[a] + frac * (left_[b] - left_[a]), dr = right_[a] + frac * (right_[b] - right_[a]);
        write_ = (write_ + 1) & (kLength - 1);
        l += mix_ * (dl - l); r += mix_ * (dr - r);
    }
private:
    uint32_t Rand() { seed_ = (1103515245u * seed_ + 12345u) % 2147483648u; return seed_; }
    float* left_ = nullptr;
    float* right_ = nullptr;
    uint32_t write_ = 0, seed_ = 1;
    float length_ = 10.f, end_ = 10.f, coefficient_ = 0.f, mix_ = 0.f, mix_target_ = 0.f, frequency_ = .1f;
};

// TAPE's limiter.h ProcessComp, one channel.
class Compressor {
public:
    float Process(float in, float pregain, float threshold, float ratio, float makeup) {
        const float pre = in * pregain, peak = std::fabs(pre);
        Slope(peak_, peak, .05f, .0002f);
        const float gain = peak_ <= threshold ? 1.f : 1.f / (ratio * (1.f + (peak_ - threshold)));
        Slope(gain_, gain, .001f, .005f);
        return SoftLimit(pre * gain_ * makeup);
    }
private:
    static void Slope(float& out, float in, float up, float down) { const float e = in - out; out += (e > 0.f ? up : down) * e; }
    float peak_ = 0.f, gain_ = 1.f;
};

// The chain Forge inserts before its delay (filter, saturation, warble, as TAPE's
// ApplyFx) and the output compressor.
class Effects {
public:
    void SetWarbleMemory(float* memory) { warble_.SetMemory(memory); }   // 2 * Warble::kLength floats
    void Configure(const Performance& p) {
        dj_.SetControl(p.dj_filter, p.dj_resonance);
        saturation_target_ = std::log(1.7f * p.saturation + 1.f) * 13.f + 1.f;
        warble_.Set(p.warble);
        compressor_target_ = p.compressor;
        active_ = true;                          // re-evaluated once everything settles
    }
    // All neutral and settled: one flag test per sample (inlined); the work is out of line.
    void Process(float& l, float& r) { if(active_) ProcessActive(l, r); }
    void Output(float& l, float& r) { if(active_) OutputActive(l, r); }
private:
    // Idle stages are skipped, the DJ filter clears once when it goes neutral, smoothers
    // stop when settled.
    FORGE_NOINLINE void ProcessActive(float& l, float& r) {
        if(!dj_.Neutral()) { dj_.Process(l, r); dj_active_ = true; }
        else if(dj_active_) { dj_.Clear(); dj_active_ = false; }
        if(saturation_ != saturation_target_) {
            OnePole(saturation_, saturation_target_, .001f);
            if(std::fabs(saturation_ - saturation_target_) < 1e-5f) saturation_ = saturation_target_;
            saturation_gain_ = 1.f - SoftClip(.4f * (saturation_ - 1.f)) * .7f;   // TAPE's level compensation
        }
        if(saturation_ > 1.f) {
            l = SoftClip(saturation_ * l) * saturation_gain_; r = SoftClip(saturation_ * r) * saturation_gain_;
        }
        const bool warble = !warble_.Neutral();
        if(warble) warble_.Process(l, r);
        if(!dj_active_ && !warble && saturation_ == 1.f && saturation_target_ == 1.f
           && compressor_ == 0.f && compressor_target_ == 0.f) active_ = false;
    }
    FORGE_NOINLINE void OutputActive(float& l, float& r) {
        if(compressor_ != compressor_target_) {
            OnePole(compressor_, compressor_target_, .001f);
            if(std::fabs(compressor_ - compressor_target_) < 1e-6f) compressor_ = compressor_target_;
            const float c = compressor_;                       // TAPE's curve, recomputed only while it moves
            threshold_ = 1.f / (10.f * c + 4.f); ratio_ = 1.f + c * c * 7.f; makeup_ = .9f + c * .6f; pregain_ = 7.f * c + 1.f;
        }
        if(compressor_ == 0.f) return;
        l = left_.Process(l, pregain_, threshold_, ratio_, makeup_);
        r = right_.Process(r, pregain_, threshold_, ratio_, makeup_);
    }
    DjFilter dj_;
    Warble warble_;
    Compressor left_, right_;
    float saturation_ = 1.f, saturation_target_ = 1.f, saturation_gain_ = 1.f, compressor_ = 0.f, compressor_target_ = 0.f;
    bool dj_active_ = false, active_ = false;
    float threshold_ = .25f, ratio_ = 1.f, makeup_ = .9f, pregain_ = 1.f;
};
} // namespace tape
} // namespace forge
