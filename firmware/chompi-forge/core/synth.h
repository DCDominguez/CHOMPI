#pragma once
#include <algorithm>
#include <array>
#include "parameters.h"

namespace forge {
// Four fixed voices. All calls belong to the audio owner. Source IDs separate
// keybed, UART and USB so one player's note-off cannot release another's note.
class Synth {
    enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };
    struct Voice {
        Stage stage = Stage::Off;
        uint8_t note = 0, source = 0;
        bool sustained = false; // key released while that source's pedal is down
        float phase = 0, increment = 0, envelope = 0, velocity = 0, gain = 0, release_step = 0;
        uint32_t age = 0;
    };
public:
    void Init(float rate) {
        rate_ = rate; voices_ = {}; age_ = 0; filter_ = 0; coefficient_ = 0;
        gain_slew_ = 1.f - std::exp(-1.f / (0.002f * rate)); // ~2 ms velocity glide on reuse
        bend_slew_ = 1.f - std::exp(-1.f / (0.005f * rate)); // ~5 ms bend smoothing, no zipper
        ResetAllControllers();
        for(unsigned n = 0; n < 128; ++n)
            frequencies_[n] = 440.f * std::pow(2.f, (int(n) - 69) / 12.f) / rate;
    }
    void Configure(const Parameters& p) {
        waveform_ = p.waveform;
        attack_step_ = 1.f / (rate_ * (0.001f + 1.999f * p.attack));
        decay_step_ = 1.f / (rate_ * (0.001f + 1.999f * p.decay));
        sustain_ = p.sustain;
        release_samples_ = rate_ * (0.005f + 4.995f * p.release);
        const float hz = 40.f * std::pow(400.f, p.cutoff);
        target_coefficient_ = 1.f - std::exp(-6.283185307f * Clamp(hz, 40.f, rate_ * 0.4f) / rate_);
    }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(note > 127 || velocity > 127 || source >= kSources) return;
        if(!velocity) {
            for(auto& v : voices_) if(v.stage != Stage::Off && v.stage != Stage::Release && v.note == note && v.source == source) {
                if(pedal_[source]) v.sustained = true; else Release(v);
            }
            return;
        }
        Voice* selected = nullptr;
        for(auto& v : voices_) if(v.stage != Stage::Off && v.note == note && v.source == source) { selected = &v; break; }
        if(!selected) for(auto& v : voices_) if(v.stage == Stage::Off) { selected = &v; break; }
        // Steal the quietest releasing voice, else the oldest held one.
        if(!selected) for(auto& v : voices_)
            if(v.stage == Stage::Release && (!selected || v.envelope < selected->envelope)) selected = &v;
        if(!selected) for(auto& v : voices_)
            if(v.sustained && (!selected || v.age < selected->age)) selected = &v;
        if(!selected) {
            selected = &voices_[0];
            for(auto& v : voices_) if(v.age < selected->age) selected = &v;
        }
        // A reused voice keeps its level, phase and gain so the new attack starts
        // where the old sound was instead of jumping to zero (click-free steal).
        const bool sounding = selected->stage != Stage::Off;
        const Voice prior = *selected;
        *selected = Voice{};
        selected->note = note; selected->source = source; selected->velocity = velocity / 127.f;
        selected->gain = sounding ? prior.gain : selected->velocity;
        if(sounding) { selected->envelope = prior.envelope; selected->phase = prior.phase; }
        selected->increment = Clamp(frequencies_[note], 0.f, 0.45f);
        selected->stage = Stage::Attack; selected->age = ++age_;
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
    void ResetControllers(uint8_t source) { Pedal(source, false); Bend(source, 8192); }
    void Silence() { voices_ = {}; filter_ = 0; ResetAllControllers(); }
    unsigned Active() const { unsigned n = 0; for(const auto& v : voices_) if(v.stage != Stage::Off) ++n; return n; }
    float Process() {
        float sum = 0;
        for(unsigned s = 0; s < kSources; ++s) bend_[s] += bend_slew_ * (bend_target_[s] - bend_[s]);
        for(auto& v : voices_) {
            switch(v.stage) {
                case Stage::Off: continue;
                case Stage::Attack:
                    v.envelope += attack_step_;
                    if(v.envelope >= 1) { v.envelope = 1; v.stage = Stage::Decay; } break;
                case Stage::Decay:
                    v.envelope -= decay_step_ * (1.f - sustain_);
                    if(v.envelope <= sustain_) { v.envelope = sustain_; v.stage = Stage::Sustain; } break;
                case Stage::Sustain: v.envelope += 0.002f * (sustain_ - v.envelope); break;
                case Stage::Release:
                    v.envelope -= v.release_step;
                    if(v.envelope <= 0.000001f) { v.envelope = 0; v.stage = Stage::Off; } break;
            }
            const float t = v.phase, dt = std::min(v.increment * bend_[v.source], 0.45f);
            float sample;
            if(waveform_ == 0) sample = std::sin(6.283185307f * t);
            else if(waveform_ == 1) // polyBLAMP rounds both corners (slope change 8 per cycle)
                sample = 1.f - 4.f * std::fabs(t - 0.5f) + 4.f * dt * (Blamp(t, dt) - Blamp(t < 0.5f ? t + 0.5f : t - 0.5f, dt));
            else if(waveform_ == 2) sample = 2.f * t - 1.f - Blep(t, dt);
            else sample = (t < 0.5f ? 1.f : -1.f) + Blep(t, dt) - Blep(t < 0.5f ? t + 0.5f : t - 0.5f, dt);
            v.gain += gain_slew_ * (v.velocity - v.gain);
            sum += sample * v.envelope * v.gain * 0.2f;
            v.phase += dt; if(v.phase >= 1.f) v.phase -= 1.f;
        }
        coefficient_ += 0.002f * (target_coefficient_ - coefficient_);
        filter_ += coefficient_ * (sum - filter_);
        if(std::fabs(filter_) < 1e-20f) filter_ = 0;
        return filter_;
    }
private:
    static constexpr unsigned kSources = 3; // UART, USB, keybed
    void Release(Voice& v) { v.sustained = false; v.stage = Stage::Release; v.release_step = v.envelope / release_samples_; }
    void ResetAllControllers() {
        for(unsigned s = 0; s < kSources; ++s) { pedal_[s] = false; bend_[s] = bend_target_[s] = 1.f; }
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
    float rate_ = 48000, attack_step_ = 1, decay_step_ = 1, sustain_ = 0.6f;
    float release_samples_ = 240, filter_ = 0, coefficient_ = 0, target_coefficient_ = 0, gain_slew_ = 1;
    std::array<bool, kSources> pedal_{};
    std::array<float, kSources> bend_{}, bend_target_{};
    float bend_slew_ = 1;
    uint8_t waveform_ = 0;
    uint32_t age_ = 0;
};
} // namespace forge
