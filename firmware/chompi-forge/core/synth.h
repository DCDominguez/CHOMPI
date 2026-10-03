#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "parameters.h"

namespace forge {
// Up to four voices. All calls belong to the audio owner. Source IDs separate
// keybed, UART and USB so one player's note-off cannot release another's note.
// Version 1/2 patches use the original path (one shared one-pole low-pass);
// version 3 adds a second oscillator, noise, a per-voice resonant filter with
// its own envelope, an LFO, a voice limit and glide. Neutral v3 settings add
// no arithmetic error to the v2 path.
class Synth {
    enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };
    struct Envelope {
        Stage stage = Stage::Off;
        float value = 0, release_step = 0;
    };
    struct Shape { float attack_step = 1, decay_step = 1, sustain = 0.6f, release_samples = 240; };
    struct Voice {
        Envelope amp, filter;
        uint8_t note = 0, source = 0;
        bool sustained = false; // key released while that source's pedal is down
        float phase = 0, phase2 = 0, increment = 0, target = 0, velocity = 0, gain = 0;
        float low = 0, band = 0, g = 0; // per-voice state-variable filter (v3)
        uint32_t age = 0;
    };
public:
    void Init(float rate) {
        rate_ = rate; voices_ = {}; age_ = 0; filter_ = 0; coefficient_ = 0; tick_ = 0;
        gain_slew_ = 1.f - std::exp(-1.f / (0.002f * rate)); // ~2 ms velocity glide on reuse
        bend_slew_ = 1.f - std::exp(-1.f / (0.005f * rate)); // ~5 ms bend smoothing, no zipper
        lfo_phase_ = 0; lfo_value_ = 0; held_ = 0; noise_state_ = 0x12345678u; last_target_ = 0;
        wheel_ = 0;
        ResetAllControllers();
        for(unsigned n = 0; n < 128; ++n)
            frequencies_[n] = 440.f * std::pow(2.f, (int(n) - 69) / 12.f) / rate;
    }
    void Configure(const Parameters& p) {
        legacy_ = p.version < 3;
        waveform_ = p.waveform;
        amp_ = MakeShape(p.attack, p.decay, p.sustain, p.release);
        const float hz = 40.f * std::pow(400.f, p.cutoff);
        target_coefficient_ = 1.f - std::exp(-6.283185307f * Clamp(hz, 40.f, rate_ * 0.4f) / rate_);
        cutoff_target_ = p.cutoff;
        // v3 (all neutral for v1/v2 patches)
        osc2_waveform_ = p.osc2_waveform;
        osc2_level_ = legacy_ ? 0.f : p.osc2_level;
        osc2_ratio_ = std::pow(2.f, ((int(p.osc2_semitones) - 24) + (p.osc2_detune * 2.f - 1.f) * 0.5f) / 12.f);
        noise_ = legacy_ ? 0.f : p.noise;
        mix_scale_ = 1.f / (1.f + osc2_level_ + noise_);
        const float q = 0.70710678f * std::pow(16.f, p.resonance);          // Q 0.707..11.3
        damping_ = 1.f / q;
        resonance_gain_ = 1.f / std::sqrt(q / 0.70710678f);                 // tame the peak
        filter_octaves_ = (p.filter_amount * 2.f - 1.f) * 6.f;              // +/-6 octaves
        filter_shape_ = MakeShape(p.filter_attack, p.filter_decay, p.filter_sustain, p.filter_release);
        lfo_waveform_ = p.lfo_waveform; lfo_wheel_ = p.lfo_wheel;
        lfo_increment_ = 0.05f * std::pow(400.f, p.lfo_rate) / rate_;      // 0.05..20 Hz
        lfo_cents_ = legacy_ ? 0.f : 200.f * p.lfo_pitch;
        lfo_octaves_ = legacy_ ? 0.f : 4.f * p.lfo_filter;
        lfo_amp_ = legacy_ ? 0.f : p.lfo_amp;
        const float glide_s = legacy_ ? 0.f : 2.f * p.glide;
        glide_slew_ = glide_s > 0.f ? 1.f - std::exp(-1.f / (glide_s * 0.25f * rate_)) : 1.f; // ~98% there after glide time
        voices_used_ = legacy_ ? 4 : Clamp(p.voices, 1, 4);
        for(unsigned i = voices_used_; i < voices_.size(); ++i)
            if(voices_[i].amp.stage != Stage::Off && voices_[i].amp.stage != Stage::Release) Release(voices_[i]);
    }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(note > 127 || velocity > 127 || source >= kSources) return;
        if(!velocity) {
            for(auto& v : voices_) if(v.amp.stage != Stage::Off && v.amp.stage != Stage::Release && v.note == note && v.source == source) {
                if(pedal_[source]) v.sustained = true; else Release(v);
            }
            return;
        }
        Voice* selected = nullptr;
        Voice* const end = voices_.data() + voices_used_;
        for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage != Stage::Off && v->note == note && v->source == source) { selected = v; break; }
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage == Stage::Off) { selected = v; break; }
        // Steal the quietest releasing voice, else the oldest sustained, else the oldest held.
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->amp.stage == Stage::Release && (!selected || v->amp.value < selected->amp.value)) selected = v;
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->sustained && (!selected || v->age < selected->age)) selected = v;
        if(!selected) {
            selected = voices_.data();
            for(Voice* v = voices_.data(); v < end; ++v) if(v->age < selected->age) selected = v;
        }
        // A reused voice keeps its level, phases, gain and filter state so the new
        // attack starts where the old sound was (click-free steal).
        const bool sounding = selected->amp.stage != Stage::Off;
        const Voice prior = *selected;
        *selected = Voice{};
        selected->note = note; selected->source = source; selected->velocity = velocity / 127.f;
        selected->gain = sounding ? prior.gain : selected->velocity;
        selected->target = Clamp(frequencies_[note], 0.f, 0.45f);
        // Glide starts from the sounding pitch, else from the last note played.
        const float from = sounding ? prior.increment : (last_target_ > 0.f ? last_target_ : selected->target);
        selected->increment = glide_slew_ < 1.f ? from : selected->target;
        last_target_ = selected->target;
        if(sounding) {
            selected->amp.value = prior.amp.value; selected->filter.value = prior.filter.value;
            selected->phase = prior.phase; selected->phase2 = prior.phase2;
            selected->low = prior.low; selected->band = prior.band; selected->g = prior.g;
        }
        selected->amp.stage = selected->filter.stage = Stage::Attack; selected->age = ++age_;
    }
    // Sustain pedal (CC64) per source: releasing it releases that source's
    // sustained voices; held keys keep sounding.
    void Pedal(uint8_t source, bool down) {
        if(source >= kSources) return;
        pedal_[source] = down;
        if(!down) for(auto& v : voices_) if(v.source == source && v.sustained) Release(v);
    }
    // 14-bit pitch bend per source, +/-2 semitones, 8192 = centre.
    void Bend(uint8_t source, uint16_t value) {
        if(source >= kSources || value > 16383) return;
        bend_target_[source] = std::pow(2.f, (static_cast<float>(value) - 8192.f) / 8192.f * (2.f / 12.f));
    }
    // Mod wheel (CC1, 0..127): scales LFO depth when the patch sets lfo_wheel.
    void ModWheel(uint8_t value) { if(value <= 127) wheel_ = value / 127.f; }
    void ResetControllers(uint8_t source) { Pedal(source, false); Bend(source, 8192); wheel_ = 0; }
    void Silence() { voices_ = {}; filter_ = 0; wheel_ = 0; ResetAllControllers(); }
    unsigned Active() const { unsigned n = 0; for(const auto& v : voices_) if(v.amp.stage != Stage::Off) ++n; return n; }
    float Process() {
        float sum = 0;
        for(unsigned s = 0; s < kSources; ++s) bend_[s] += bend_slew_ * (bend_target_[s] - bend_[s]);
        // LFO (one shared oscillator). Depth scale 1, or the mod wheel when gated.
        float pitch_ratio = 1.f, amp_lfo = 1.f, lfo_octaves = 0.f;
        if(lfo_cents_ > 0.f || lfo_octaves_ > 0.f || lfo_amp_ > 0.f) {
            lfo_phase_ += lfo_increment_;
            if(lfo_phase_ >= 1.f) { lfo_phase_ -= 1.f; held_ = Noise(); }
            const float t = lfo_phase_;
            lfo_value_ = lfo_waveform_ == 0 ? std::sin(6.283185307f * t)
                : lfo_waveform_ == 1 ? 1.f - 4.f * std::fabs(t - 0.5f)
                : lfo_waveform_ == 2 ? (t < 0.5f ? 1.f : -1.f) : held_;
            const float depth = lfo_wheel_ ? wheel_ : 1.f;
            if(lfo_cents_ > 0.f) pitch_ratio = std::exp2(lfo_value_ * depth * lfo_cents_ / 1200.f);
            lfo_octaves = lfo_value_ * depth * lfo_octaves_;
            amp_lfo = 1.f - lfo_amp_ * depth * (0.5f - 0.5f * lfo_value_);
        }
        // v3 filter coefficients are refreshed every 16 samples (one tan per voice).
        const bool refresh = !legacy_ && (tick_++ & 15u) == 0;
        if(!legacy_) cutoff_ += 0.002f * (cutoff_target_ - cutoff_);
        const float base_octave = kLog2Of40 + cutoff_ * kLog2Of400 + lfo_octaves;
        for(auto& v : voices_) {
            if(v.amp.stage == Stage::Off) continue;
            Step(v.amp, amp_);
            if(v.amp.stage == Stage::Off) continue;
            const float dt = std::min(v.increment * bend_[v.source] * pitch_ratio, 0.45f);
            float sample = Oscillator(waveform_, v.phase, dt);
            v.phase += dt; if(v.phase >= 1.f) v.phase -= 1.f;
            if(!legacy_) {
                if(v.increment != v.target) {
                    v.increment += glide_slew_ * (v.target - v.increment);
                    if(std::fabs(v.target - v.increment) < 1e-9f) v.increment = v.target;
                }
                if(osc2_level_ > 0.f) {
                    const float dt2 = std::min(dt * osc2_ratio_, 0.45f);
                    sample += osc2_level_ * Oscillator(osc2_waveform_, v.phase2, dt2);
                    v.phase2 += dt2; if(v.phase2 >= 1.f) v.phase2 -= 1.f;
                }
                if(noise_ > 0.f) sample += noise_ * Noise();
                sample *= mix_scale_;
                Step(v.filter, filter_shape_);
                if(refresh || v.g == 0.f) {
                    const float hz = Clamp(std::exp2(base_octave + filter_octaves_ * v.filter.value), 20.f, 0.45f * rate_);
                    v.g = std::tan(3.14159265f * hz / rate_);
                }
                // Topology-preserving SVF (Zavalishin), low-pass output.
                const float high = (sample - (damping_ + v.g) * v.band - v.low) / (1.f + v.g * (v.g + damping_));
                const float band = v.g * high + v.band;
                const float low = v.g * band + v.low;
                v.band = v.g * high + band; v.low = v.g * band + low;
                if(std::fabs(v.low) < 1e-20f) v.low = 0.f;
                if(std::fabs(v.band) < 1e-20f) v.band = 0.f;
                sample = low * resonance_gain_;
            }
            v.gain += gain_slew_ * (v.velocity - v.gain);
            sum += sample * v.amp.value * v.gain * 0.2f * amp_lfo;
        }
        if(!legacy_) return sum;
        coefficient_ += 0.002f * (target_coefficient_ - coefficient_);
        filter_ += coefficient_ * (sum - filter_);
        if(std::fabs(filter_) < 1e-20f) filter_ = 0;
        return filter_;
    }
private:
    static constexpr unsigned kSources = 3; // UART, USB, keybed
    static constexpr float kLog2Of40 = 5.321928f, kLog2Of400 = 8.643856f;
    static uint8_t Clamp(uint8_t v, uint8_t lo, uint8_t hi) { return v < lo ? lo : v > hi ? hi : v; }
    static float Clamp(float v, float lo, float hi) { return forge::Clamp(v, lo, hi); }
    Shape MakeShape(float a, float d, float s, float r) const {
        Shape shape;
        shape.attack_step = 1.f / (rate_ * (0.001f + 1.999f * a));
        shape.decay_step = 1.f / (rate_ * (0.001f + 1.999f * d));
        shape.sustain = s;
        shape.release_samples = rate_ * (0.005f + 4.995f * r);
        return shape;
    }
    // Off at zero ends a voice (amplitude) or rests there (filter envelope).
    static void Step(Envelope& e, const Shape& s) {
        switch(e.stage) {
            case Stage::Off: break;
            case Stage::Attack:
                e.value += s.attack_step;
                if(e.value >= 1) { e.value = 1; e.stage = Stage::Decay; } break;
            case Stage::Decay:
                e.value -= s.decay_step * (1.f - s.sustain);
                if(e.value <= s.sustain) { e.value = s.sustain; e.stage = Stage::Sustain; } break;
            case Stage::Sustain: e.value += 0.002f * (s.sustain - e.value); break;
            case Stage::Release:
                e.value -= e.release_step;
                if(e.value <= 0.000001f) { e.value = 0; e.stage = Stage::Off; } break;
        }
    }
    void Release(Voice& v) {
        v.sustained = false;
        v.amp.stage = Stage::Release; v.amp.release_step = v.amp.value / amp_.release_samples;
        if(v.filter.stage != Stage::Off) {
            v.filter.stage = Stage::Release; v.filter.release_step = v.filter.value / filter_shape_.release_samples;
        }
    }
    void ResetAllControllers() {
        for(unsigned s = 0; s < kSources; ++s) { pedal_[s] = false; bend_[s] = bend_target_[s] = 1.f; }
    }
    float Noise() { // xorshift32, uniform -1..1
        noise_state_ ^= noise_state_ << 13; noise_state_ ^= noise_state_ >> 17; noise_state_ ^= noise_state_ << 5;
        return static_cast<float>(static_cast<int32_t>(noise_state_)) * (1.f / 2147483648.f);
    }
    static float Oscillator(uint8_t waveform, float t, float dt) {
        if(waveform == 0) return std::sin(6.283185307f * t);
        if(waveform == 1) // polyBLAMP rounds both corners (slope change 8 per cycle)
            return 1.f - 4.f * std::fabs(t - 0.5f) + 4.f * dt * (Blamp(t, dt) - Blamp(t < 0.5f ? t + 0.5f : t - 0.5f, dt));
        if(waveform == 2) return 2.f * t - 1.f - Blep(t, dt);
        return (t < 0.5f ? 1.f : -1.f) + Blep(t, dt) - Blep(t < 0.5f ? t + 0.5f : t - 0.5f, dt);
    }
    // Integrated polyBLEP residual for a slope discontinuity, same scaling as Blep.
    static float Blamp(float t, float dt) {
        if(t < dt) { t = t / dt - 1.f; return -t * t * t / 3.f; }
        if(t > 1.f - dt) { t = (t - 1.f) / dt + 1.f; return t * t * t / 3.f; }
        return 0.f;
    }
    static float Blep(float t, float dt) {
        if(t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if(t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    std::array<Voice, 4> voices_{};
    std::array<float, 128> frequencies_{};
    Shape amp_, filter_shape_;
    float rate_ = 48000, filter_ = 0, coefficient_ = 0, target_coefficient_ = 0, gain_slew_ = 1;
    std::array<bool, kSources> pedal_{};
    std::array<float, kSources> bend_{}, bend_target_{};
    float bend_slew_ = 1, wheel_ = 0;
    // v3
    bool legacy_ = true, lfo_wheel_ = false;
    uint8_t waveform_ = 0, osc2_waveform_ = 0, lfo_waveform_ = 0, voices_used_ = 4;
    float osc2_level_ = 0, osc2_ratio_ = 1, noise_ = 0, mix_scale_ = 1;
    float damping_ = 1.41421356f, resonance_gain_ = 1, filter_octaves_ = 0, cutoff_ = 0.5f, cutoff_target_ = 0.5f;
    float lfo_phase_ = 0, lfo_value_ = 0, lfo_increment_ = 0, lfo_cents_ = 0, lfo_octaves_ = 0, lfo_amp_ = 0, held_ = 0;
    float glide_slew_ = 1, last_target_ = 0;
    uint32_t noise_state_ = 0x12345678u, age_ = 0, tick_ = 0;
};
} // namespace forge
