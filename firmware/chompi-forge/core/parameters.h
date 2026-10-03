#pragma once
#include <cmath>
#include <cstdint>

namespace forge {
enum class Parameter : uint8_t { Mix, Time, Feedback, Level, Bypass, Cutoff, Resonance, ReverbMix };
struct Command { Parameter parameter; float value; };

inline float Clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

// All public control values are normalized. Invalid values are rejected,
// finite out-of-range values are clamped; unknown IDs never mutate state.
// Version 3 fields default to neutral values, so a v1/v2 patch (which never
// carries them) renders exactly as it did before v3 existed.
struct Parameters {
    uint8_t version = 1, waveform = 0;
    bool synth = false;
    float attack = 0.0045f, decay = 0.1f, sustain = 0.6f, release = 0.08f, cutoff = 0.5f;
    float mix = 0.f;
    float time = 0.25f;
    float feedback = 0.25f;
    float level = 0.25f;
    bool bypass = false;
    // v3 synth: second oscillator, noise, voices, glide
    uint8_t osc2_waveform = 0, osc2_semitones = 24, voices = 4; // semitones byte 0..48 = -24..+24
    float osc2_level = 0.f, osc2_detune = 0.5f, noise = 0.f, glide = 0.f;
    // v3 resonant per-voice filter with its own envelope (amount 0.5 = none)
    float resonance = 0.f, filter_amount = 0.5f;
    float filter_attack = 0.0045f, filter_decay = 0.1f, filter_sustain = 0.f, filter_release = 0.08f;
    // v3 LFO; depths 0 = off; mod wheel gates depth when lfo_wheel is set
    uint8_t lfo_waveform = 0; bool lfo_wheel = false;
    float lfo_rate = 0.5f, lfo_pitch = 0.f, lfo_filter = 0.f, lfo_amp = 0.f;
    // v3 reverb after the delay
    float reverb_mix = 0.f, reverb_size = 0.5f, reverb_damping = 0.5f;

    bool Valid() const {
        return version >= 1 && version <= 3 && waveform < 4 && !(version == 1 && synth)
            && Unit(attack) && Unit(decay) && Unit(sustain) && Unit(release) && Unit(cutoff)
            && Unit(mix) && Unit(time) && Unit(feedback) && Unit(level)
            && osc2_waveform < 4 && osc2_semitones <= 48 && voices >= 1 && voices <= 4 && lfo_waveform < 4
            && Unit(osc2_level) && Unit(osc2_detune) && Unit(noise) && Unit(glide)
            && Unit(resonance) && Unit(filter_amount) && Unit(filter_attack) && Unit(filter_decay)
            && Unit(filter_sustain) && Unit(filter_release) && Unit(lfo_rate) && Unit(lfo_pitch)
            && Unit(lfo_filter) && Unit(lfo_amp) && Unit(reverb_mix) && Unit(reverb_size) && Unit(reverb_damping);
    }
    static bool Unit(float v) { return std::isfinite(v) && v >= 0.f && v <= 1.f; }

    bool Apply(Command command) {
        if(!std::isfinite(command.value)) return false;
        const float value = Clamp(command.value, 0.f, 1.f);
        switch(command.parameter) {
            case Parameter::Mix: mix = value; break;
            case Parameter::Time: time = value; break;
            case Parameter::Feedback: feedback = value; break;
            case Parameter::Level: level = value; break;
            case Parameter::Bypass: bypass = value >= 0.5f; break;
            case Parameter::Cutoff: cutoff = value; break;
            // v3-only controls: older patches cannot report them in status.
            case Parameter::Resonance: if(version < 3) return false; resonance = value; break;
            case Parameter::ReverbMix: if(version < 3) return false; reverb_mix = value; break;
            default: return false;
        }
        return true;
    }
};

// MIDI channel 1 (zero-based channel 0); full patches use protocol.h.
// Stock CHOMPI convention: CC20+n sets encoder n's position (absolute), so
// CC20-25 = SW1-SW6 = mix, time, feedback, level, cutoff (SW5), level (SW6).
// Stock also uses CC14/15 and CC26-33 (virtual keys, second encoder pages);
// Forge ignores those. Extra controls use General MIDI numbers that stock
// leaves free: 71 resonance, 74 cutoff (brightness), 85 wet bypass, 91 reverb.
inline bool DecodeCC(uint8_t channel, uint8_t cc, uint8_t value, Command& out) {
    if(channel != 0 || value > 127) return false;
    Parameter parameter;
    switch(cc) {
        case 20: parameter = Parameter::Mix; break;
        case 21: parameter = Parameter::Time; break;
        case 22: parameter = Parameter::Feedback; break;
        case 23: case 25: parameter = Parameter::Level; break;
        case 24: case 74: parameter = Parameter::Cutoff; break;
        case 71: parameter = Parameter::Resonance; break;
        case 85: parameter = Parameter::Bypass; break;
        case 91: parameter = Parameter::ReverbMix; break;
        default: return false;
    }
    out = {parameter, static_cast<float>(value) / 127.f};
    return true;
}
} // namespace forge
