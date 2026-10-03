#pragma once
#include <cmath>
#include <cstdint>

// Keeps rarely-called or large helpers out of the inlined audio callback
// (code space is the firmware's tightest budget; see COMPATIBILITY.md §2).
#if defined(__GNUC__)
#define FORGE_NOINLINE __attribute__((noinline))
#define FORGE_INLINE inline __attribute__((always_inline))
#else
#define FORGE_NOINLINE
#define FORGE_INLINE inline
#endif

namespace forge {
enum class Parameter : uint8_t { Mix, Time, Feedback, Level, Bypass, Cutoff, Resonance, ReverbMix,
                                 SamplePitch, SampleStart, SampleEnd,
                                 // Panel knobs 1-4 (CC20-23): the function depends on the patch's source.
                                 Knob1, Knob2, Knob3, Knob4 };
struct Command { Parameter parameter; float value; };

inline float Clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

// All public control values are normalized. Invalid values are rejected,
// finite out-of-range values are clamped; unknown IDs never mutate state.
// Version 3 fields default to neutral values, so a v1/v2 patch (which never
// carries them) renders exactly as it did before v3 existed. Version 4 adds
// the sampler (see docs/forge/SAMPLING.md); its defaults keep v3 behaviour.
constexpr uint8_t kSampleBanks = 5, kSampleSlots = 15, kRamSlot = 14; // slot 15 (index 14) = recording
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
    // v4 sampler. source 0 = oscillators, 1 = sampler; mode 0 = chromatic
    // (JAMMI), 1 = kit (CUBBI); bank 0-4 (a-e); slot 0-14 (14 = recording).
    uint8_t source = 0, sample_mode = 0, sample_bank = 0, sample_slot = 0;
    float sample_pitch = 0.5f, sample_start = 0.f, sample_end = 1.f, sample_xfade = 0.04f; // pitch +/-24 st; xfade 0-250 ms
    bool sample_loop = false, sample_gate = true, sample_reverse = false;

    bool Sampler() const { return version >= 4 && source == 1; }
    uint8_t MaxVoices() const { return version >= 4 ? 7 : 4; }

    FORGE_NOINLINE bool Valid() const {
        return version >= 1 && version <= 4 && waveform < 4 && !(version == 1 && synth)
            && Unit(attack) && Unit(decay) && Unit(sustain) && Unit(release) && Unit(cutoff)
            && Unit(mix) && Unit(time) && Unit(feedback) && Unit(level)
            && osc2_waveform < 4 && osc2_semitones <= 48 && voices >= 1 && voices <= MaxVoices() && lfo_waveform < 4
            && source < 2 && sample_mode < 2 && sample_bank < kSampleBanks && sample_slot < kSampleSlots
            && Unit(sample_pitch) && Unit(sample_start) && Unit(sample_end) && Unit(sample_xfade)
            && sample_start < sample_end
            && Unit(osc2_level) && Unit(osc2_detune) && Unit(noise) && Unit(glide)
            && Unit(resonance) && Unit(filter_amount) && Unit(filter_attack) && Unit(filter_decay)
            && Unit(filter_sustain) && Unit(filter_release) && Unit(lfo_rate) && Unit(lfo_pitch)
            && Unit(lfo_filter) && Unit(lfo_amp) && Unit(reverb_mix) && Unit(reverb_size) && Unit(reverb_damping);
    }
    static bool Unit(float v) { return std::isfinite(v) && v >= 0.f && v <= 1.f; }

    // Knob n's parameter: TAPE's page 0 (pitch, start, end, effects) when the
    // sampler plays, else mix / time / feedback / level.
    Parameter KnobParameter(unsigned knob) const {
        static const Parameter synth_knobs[4] = {Parameter::Mix, Parameter::Time, Parameter::Feedback, Parameter::Level};
        static const Parameter sampler_knobs[4] = {Parameter::SamplePitch, Parameter::SampleStart, Parameter::SampleEnd, Parameter::Mix};
        return (Sampler() ? sampler_knobs : synth_knobs)[knob & 3];
    }
    float Value(Parameter parameter) const {
        switch(parameter) {
            case Parameter::Mix: return mix;
            case Parameter::Time: return time;
            case Parameter::Feedback: return feedback;
            case Parameter::Level: return level;
            case Parameter::SamplePitch: return sample_pitch;
            case Parameter::SampleStart: return sample_start;
            case Parameter::SampleEnd: return sample_end;
            default: return 0.f;
        }
    }
    FORGE_NOINLINE bool Apply(Command command) {
        if(!std::isfinite(command.value)) return false;
        if(command.parameter >= Parameter::Knob1 && command.parameter <= Parameter::Knob4)
            command.parameter = KnobParameter(static_cast<unsigned>(command.parameter) - static_cast<unsigned>(Parameter::Knob1));
        const float value = Clamp(command.value, 0.f, 1.f);
        constexpr float kStep = 1.f / 16383.f;   // keep start < end at wire resolution
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
            // v4-only sampler controls.
            case Parameter::SamplePitch: if(version < 4) return false; sample_pitch = value; break;
            case Parameter::SampleStart: if(version < 4) return false; sample_start = std::fmin(value, sample_end - kStep); break;
            case Parameter::SampleEnd: if(version < 4) return false; sample_end = std::fmax(value, sample_start + kStep); break;
            default: return false;
        }
        return true;
    }
};

// MIDI channel 1 (zero-based channel 0); full patches use protocol.h.
// Stock CHOMPI convention: CC20+n sets encoder n's position (absolute), so
// CC20-25 = knobs 1-4, SW5, SW6: knobs follow the patch (mix, time, feedback,
// level; or the sampler's pitch, start, end, mix), SW5 cutoff, SW6 level.
// Stock also uses CC14/15 and CC26-33 (virtual keys, second encoder pages);
// Forge ignores those. Extra controls use General MIDI numbers that stock
// leaves free: 71 resonance, 74 cutoff (brightness), 85 wet bypass, 91 reverb.
inline bool DecodeCC(uint8_t channel, uint8_t cc, uint8_t value, Command& out) {
    if(channel != 0 || value > 127) return false;
    Parameter parameter;
    switch(cc) {
        case 20: parameter = Parameter::Knob1; break;
        case 21: parameter = Parameter::Knob2; break;
        case 22: parameter = Parameter::Knob3; break;
        case 23: parameter = Parameter::Knob4; break;
        case 25: parameter = Parameter::Level; break;
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
