#pragma once
#include <cmath>
#include <cstdint>

namespace forge {
enum class Parameter : uint8_t { Mix, Time, Feedback, Level, Bypass };
struct Command { Parameter parameter; float value; };

inline float Clamp(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

// All public control values are normalized. Invalid values are rejected,
// finite out-of-range values are clamped; unknown IDs never mutate state.
struct Parameters {
    float mix = 0.f;
    float time = 0.25f;
    float feedback = 0.25f;
    float level = 0.25f;
    bool bypass = false;

    bool Valid() const {
        return std::isfinite(mix) && mix >= 0.f && mix <= 1.f
            && std::isfinite(time) && time >= 0.f && time <= 1.f
            && std::isfinite(feedback) && feedback >= 0.f && feedback <= 1.f
            && std::isfinite(level) && level >= 0.f && level <= 1.f;
    }

    bool Apply(Command command) {
        if(!std::isfinite(command.value)) return false;
        const float value = Clamp(command.value, 0.f, 1.f);
        switch(command.parameter) {
            case Parameter::Mix: mix = value; break;
            case Parameter::Time: time = value; break;
            case Parameter::Feedback: feedback = value; break;
            case Parameter::Level: level = value; break;
            case Parameter::Bypass: bypass = value >= 0.5f; break;
            default: return false;
        }
        return true;
    }
};

// MIDI channel 1 (zero-based channel 0); full patches use protocol.h.
inline bool DecodeCC(uint8_t channel, uint8_t cc, uint8_t value, Command& out) {
    if(channel != 0 || value > 127) return false;
    Parameter parameter;
    switch(cc) {
        case 20: parameter = Parameter::Mix; break;
        case 21: parameter = Parameter::Time; break;
        case 22: parameter = Parameter::Feedback; break;
        case 23: parameter = Parameter::Level; break;
        case 24: parameter = Parameter::Bypass; break;
        default: return false;
    }
    out = {parameter, static_cast<float>(value) / 127.f};
    return true;
}
} // namespace forge
