#pragma once
#include <cmath>
#include <cstdint>
#include <initializer_list>

// Keeps rarely-called or large helpers out of the inlined audio callback
// (code space is the firmware's tightest budget; see COMPATIBILITY.md §2).
#if defined(__GNUC__)
#define FORGE_NOINLINE __attribute__((noinline))
#define FORGE_INLINE inline __attribute__((always_inline))
// Once per audio block or rarer (panel, menu, LEDs, start-up): compiled for size.
#define FORGE_COLD __attribute__((noinline, optimize("Os")))
#else
#define FORGE_NOINLINE
#define FORGE_INLINE inline
#define FORGE_COLD
#endif

namespace forge {
enum class Parameter : uint8_t { Mix, Time, Feedback, Level, Bypass, Cutoff, Resonance, ReverbMix,
                                 SamplePitch, SampleStart, SampleEnd,
                                 // Panel knobs 1-4 (CC20-23): the patch's assignment (v5) or its source's default.
                                 Knob1, Knob2, Knob3, Knob4,
                                 // Knob pages and v5 knob assignments (docs/forge/KNOBS.md).
                                 FilterAmount, Attack, Decay, Sustain, Release, LfoRate, LfoPitch, LfoFilter, LfoAmp,
                                 Osc2Level, Osc2Detune, Noise, Glide, ReverbSize, ReverbDamping, SampleXfade,
                                 // Performance controls (TAPE's knobs; device state, not in patches): see Performance.
                                 Speed, VoiceGain, Pan, Space, Saturation, Warble, DjFilter, DjResonance, Compressor,
                                 InputGain, Count };
// First performance control; ids from here on are never on the wire.
constexpr Parameter kFirstPerformance = Parameter::Speed;
inline bool IsPerformance(Parameter p) { return p >= kFirstPerformance && p < Parameter::Count; }
struct Command { Parameter parameter; float value; };

inline float Clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

// All public control values are normalized. Invalid values are rejected,
// finite out-of-range values are clamped; unknown IDs never mutate state.
// Version 3 fields default to neutral values, so a v1/v2 patch (which never
// carries them) renders exactly as it did before v3 existed. Version 4 adds
// the sampler (see docs/forge/SAMPLING.md); its defaults keep v3 behaviour.
// Version 5 adds what knobs 1-4 control (knobs[n]: 0 = the source's default,
// else Parameter id + 1; see docs/forge/KNOBS.md); otherwise it is version 4.
constexpr uint8_t kSampleBanks = 5, kSampleSlots = 15, kRamSlot = 14; // slot 15 (index 14) = recording
// Patch v7 parts words (layout in core/parts.h PackArp / PackClock): pattern < 6, rate < 6,
// gate step < 20, bass mode < 5, bass octave < 3, tempo 40-300 BPM.
inline bool PartsWordsValid(uint32_t arp, uint32_t clock) {
    const uint32_t bpm = clock & 511u;
    return !(arp >> 21) && !(clock >> 21) && (arp & 7u) < 6 && (arp >> 3 & 7u) < 6 && (arp >> 8 & 31u) < 20
        && (arp >> 14 & 7u) < 5 && (arp >> 19 & 3u) < 3 && bpm >= 40 && bpm <= 300;
}
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
    // v5 knob assignments (page 1 of each knob).
    uint8_t knobs[4] = {0, 0, 0, 0};
    // Version 6 (0.13) adds harmony mode: harmony::Pack (core/harmony.h), 17 bits; request bytes 87-89.
    uint32_t harmony = 0;
    // Version 7 (0.14) adds the parts (clock, arp, bass): parts::PackArp / PackClock
    // (core/parts.h), two 21-bit words; request bytes 90-92 and 93-95.
    uint32_t parts_arp = 0, parts_clock = 0;

    bool Sampler() const { return version >= 4 && source == 1; }
    uint8_t MaxVoices() const { return version >= 4 ? 7 : 4; }

    FORGE_COLD bool Valid() const {
        return version >= 1 && version <= 7 && waveform < 4 && !(version == 1 && synth)
            && (version < 7 || PartsWordsValid(parts_arp, parts_clock))
            && harmony < (1u << 17) && (harmony & 15u) < 12 && ((harmony >> 4) & 15u) < 9 && ((harmony >> 8) & 7u) < 6
            && Unit(attack) && Unit(decay) && Unit(sustain) && Unit(release) && Unit(cutoff)
            && Unit(mix) && Unit(time) && Unit(feedback) && Unit(level)
            && osc2_waveform < 4 && osc2_semitones <= 48 && voices >= 1 && voices <= MaxVoices() && lfo_waveform < 4
            && source < 2 && sample_mode < 2 && sample_bank < kSampleBanks && sample_slot < kSampleSlots
            && Unit(sample_pitch) && Unit(sample_start) && Unit(sample_end) && Unit(sample_xfade)
            && sample_start < sample_end
            && Unit(osc2_level) && Unit(osc2_detune) && Unit(noise) && Unit(glide)
            && Unit(resonance) && Unit(filter_amount) && Unit(filter_attack) && Unit(filter_decay)
            && Unit(filter_sustain) && Unit(filter_release) && Unit(lfo_rate) && Unit(lfo_pitch)
            && Unit(lfo_filter) && Unit(lfo_amp) && Unit(reverb_mix) && Unit(reverb_size) && Unit(reverb_damping)
            && KnobValid(0) && KnobValid(1) && KnobValid(2) && KnobValid(3);
    }
    // A knob may be assigned any continuous control (not bypass, not another knob).
    static bool Assignable(Parameter p) {
        return p < kFirstPerformance && p != Parameter::Bypass && (p < Parameter::Knob1 || p > Parameter::Knob4);
    }
    bool KnobValid(unsigned knob) const {
        return !knobs[knob] || (version >= 5 && knobs[knob] <= static_cast<unsigned>(kFirstPerformance)
                                && Assignable(static_cast<Parameter>(knobs[knob] - 1)));
    }
    static bool Unit(float v) { return std::isfinite(v) && v >= 0.f && v <= 1.f; }

    // Knob n's parameter: the patch's v5 assignment, else TAPE's page 0 (pitch,
    // start, end, effects) when the sampler plays, else mix / time / feedback / level.
    Parameter KnobParameter(unsigned knob) const {
        if(knobs[knob & 3]) return static_cast<Parameter>(knobs[knob & 3] - 1);
        static const Parameter synth_knobs[4] = {Parameter::Mix, Parameter::Time, Parameter::Feedback, Parameter::Level};
        static const Parameter sampler_knobs[4] = {Parameter::SamplePitch, Parameter::SampleStart, Parameter::SampleEnd, Parameter::Mix};
        return (Sampler() ? sampler_knobs : synth_knobs)[knob & 3];
    }
    Parameter Resolve(Parameter parameter) const {
        return parameter >= Parameter::Knob1 && parameter <= Parameter::Knob4
            ? KnobParameter(static_cast<unsigned>(parameter) - static_cast<unsigned>(Parameter::Knob1)) : parameter;
    }
    // Continuous controls by id (Value / Apply); nullptr for bypass, knobs and unknown ids.
    float* Field(Parameter parameter) {
        switch(parameter) {
            case Parameter::Mix: return &mix;
            case Parameter::Time: return &time;
            case Parameter::Feedback: return &feedback;
            case Parameter::Level: return &level;
            case Parameter::Cutoff: return &cutoff;
            case Parameter::Resonance: return &resonance;
            case Parameter::ReverbMix: return &reverb_mix;
            case Parameter::SamplePitch: return &sample_pitch;
            case Parameter::SampleStart: return &sample_start;
            case Parameter::SampleEnd: return &sample_end;
            case Parameter::FilterAmount: return &filter_amount;
            case Parameter::Attack: return &attack;
            case Parameter::Decay: return &decay;
            case Parameter::Sustain: return &sustain;
            case Parameter::Release: return &release;
            case Parameter::LfoRate: return &lfo_rate;
            case Parameter::LfoPitch: return &lfo_pitch;
            case Parameter::LfoFilter: return &lfo_filter;
            case Parameter::LfoAmp: return &lfo_amp;
            case Parameter::Osc2Level: return &osc2_level;
            case Parameter::Osc2Detune: return &osc2_detune;
            case Parameter::Noise: return &noise;
            case Parameter::Glide: return &glide;
            case Parameter::ReverbSize: return &reverb_size;
            case Parameter::ReverbDamping: return &reverb_damping;
            case Parameter::SampleXfade: return &sample_xfade;
            default: return nullptr;
        }
    }
    // The patch version that first carries a control (status must be able to report it).
    static uint8_t MinVersion(Parameter p) {
        switch(p) {
            case Parameter::Mix: case Parameter::Time: case Parameter::Feedback: case Parameter::Level:
            case Parameter::Bypass: case Parameter::Cutoff: return 1;
            case Parameter::Attack: case Parameter::Decay: case Parameter::Sustain: case Parameter::Release: return 2;
            case Parameter::SamplePitch: case Parameter::SampleStart: case Parameter::SampleEnd: case Parameter::SampleXfade: return 4;
            default: return 3;
        }
    }
    float Value(Parameter parameter) const {
        float* field = const_cast<Parameters*>(this)->Field(Resolve(parameter));
        return field ? *field : 0.f;
    }
    // Controls newer than the patch's version are refused: older patches cannot
    // report them in status (v2 envelope, v3 synth/reverb, v4 sampler).
    FORGE_NOINLINE bool Apply(Command command) {
        if(!std::isfinite(command.value)) return false;
        command.parameter = Resolve(command.parameter);
        const float value = Clamp(command.value, 0.f, 1.f);
        if(command.parameter == Parameter::Bypass) { bypass = value >= 0.5f; return true; }
        float* field = Field(command.parameter);
        if(!field || version < MinVersion(command.parameter)) return false;
        constexpr float kStep = 1.f / 16383.f;   // keep start < end at wire resolution
        if(command.parameter == Parameter::SampleStart) *field = std::fmin(value, sample_end - kStep);
        else if(command.parameter == Parameter::SampleEnd) *field = std::fmax(value, sample_start + kStep);
        else *field = value;
        return true;
    }
};

// TAPE's performance controls (docs/forge/TAPE_CONTROLS.md): knob positions 0..1
// with TAPE's defaults, kept by the device across patch changes (TAPE resets them at
// power-on). Not part of patches; the panel and CC turn them.
struct Performance {
    float speed = .83f;        // SW4 page 1: .83 = 1x, .5 = stopped, below = reverse
    float voice_gain = .704f;  // SW4 page 2: 2v^2 + .01 (.704 ~ 1x)
    float pan = .5f;           // menu SW4 page 2
    float saturation = 0.f, warble = 0.f;
    float dj_filter = .5f, dj_resonance = 0.f;   // .5 = open; below low-pass, above high-pass
    float compressor = 0.f;    // menu SW6
    float input_gain = .75f;   // SW6 page 2: mic x5 / line x3 scaled by this (TAPE)
    float* Field(Parameter p) {
        switch(p) {
            case Parameter::Speed: return &speed;
            case Parameter::VoiceGain: return &voice_gain;
            case Parameter::Pan: return &pan;
            case Parameter::Saturation: return &saturation;
            case Parameter::Warble: return &warble;
            case Parameter::DjFilter: return &dj_filter;
            case Parameter::DjResonance: return &dj_resonance;
            case Parameter::Compressor: return &compressor;
            case Parameter::InputGain: return &input_gain;
            default: return nullptr;
        }
    }
    static float Default(Parameter p) { Performance d; float* f = d.Field(p); return f ? *f : 0.f; }
};
// TAPE's free pitch curve (DSPEngine::SetGlobalPitchFree): 0.5 = stopped, above
// forward, below reverse; each side .01-.5x, .5-1x, 1-2x over thirds. Signed ratio.
inline float TapeSpeedRatio(float knob) {
    const float val = knob < .5f ? (.5f - knob) * -2.f : (knob - .5f) * 2.f;
    const float inv = val < 0.f ? -1.f : 1.f, a = std::fabs(val);
    if(a < .33f) return val * 1.484848f + .01f * inv;
    if(a < .66f) return (val - .33f * inv) * 1.515151f + .5f * inv;
    return (val - .66f * inv) * 2.941176f + 1.f * inv;
}

// Inverse of TapeSpeedRatio: the knob position for a signed ratio (|ratio| .01..2).
inline float TapeSpeedKnob(float ratio) {
    const float r = Clamp(std::fabs(ratio), .01f, 2.f);
    const float a = r < .5f ? (r - .01f) / 1.484848f : r < 1.f ? (r - .5f) / 1.515151f + .33f : (r - 1.f) / 2.941176f + .66f;
    return Clamp(ratio < 0.f ? .5f - .5f * a : .5f + .5f * a, 0.f, 1.f);
}
// TAPE's quantised pitch (SetGlobalPitchQuantized): fifths and fourths alternately,
// up to 2x, down to 1/16x, then through to reverse. One step per 4 knob clicks.
inline float QuantisedSpeedStep(float ratio, int direction) {
    static const float kSteps[11] = {.0625f, .09375f, .125f, .1875f, .25f, .375f, .5f, .75f, 1.f, 1.5f, 2.f};
    // Signed positions -11..-1 (reverse) and 1..11 (forward); index of the nearest.
    int best = 1; float error = 1e9f;
    for(int i = 0; i < 11; ++i) for(int sign : {-1, 1}) {
        const float e = std::fabs(ratio - sign * kSteps[i]);
        if(e < error) { error = e; best = sign * (i + 1); }
    }
    int next = best + direction;
    if(next == 0) next = direction > 0 ? 1 : -1;
    if(next > 11) next = 11;
    if(next < -11) next = -11;
    return next > 0 ? kSteps[next - 1] : -kSteps[-next - 1];
}

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
