#pragma once
#include <cstdint>
#include <cstring>
#include "parameters.h"

namespace forge {
// The FX section (0.20 development, docs/forge/CONTINUE.md): four modules, three effects each, one
// chosen per module, in a route. Module order = the FX page's key groups and its four knobs.
// Audio owner only; no allocation (the caller gives each effect its memory, D2 SRAM on firmware);
// bounded work per sample; every effect is skipped (one test) while its amount is at zero.
namespace fx {

enum class Module : uint8_t { Space, Colour, Texture, Motion };
constexpr unsigned kModules = 4, kChoices = 3;
// Route: the order of the modules after the DJ filter (always first, as before the FX section).
enum class Route : uint8_t { Classic, WearLast, WashFirst, SideBySide };
constexpr unsigned kRoutes = 4;
// Stages each route runs, in order (Space = delay + reverb / cloud; SideBySide runs its delay and
// reverb in parallel, the rest as Classic).
constexpr Module kOrder[kRoutes][kModules] = {
    {Module::Colour, Module::Motion, Module::Texture, Module::Space},     // Classic: the pre-FX chain
    {Module::Colour, Module::Motion, Module::Space, Module::Texture},     // Wear last: worn after the space
    {Module::Colour, Module::Texture, Module::Space, Module::Motion},     // Wash first: the wash moves
    {Module::Colour, Module::Motion, Module::Texture, Module::Space},     // Side by side
};
// Choices per module, in key order. Phase 1 builds the first of each plus Space's cloud and
// Motion's chorus; the others are Phase 2 (selecting them is refused until they exist).
enum class SpaceChoice : uint8_t { EchoRoom, Cloud, TapeEcho };
enum class ColourChoice : uint8_t { Saturation, Fold, Crush };
enum class TextureChoice : uint8_t { Cassette, Broken, Squash };
enum class MotionChoice : uint8_t { Chorus, Phaser, Tremolo };
inline bool Built(Module m, uint8_t choice) {
    if(choice >= kChoices) return false;
    switch(m) {
        case Module::Space: return choice <= static_cast<uint8_t>(SpaceChoice::Cloud);
        case Module::Motion: return choice == static_cast<uint8_t>(MotionChoice::Chorus);
        default: return choice == 0;
    }
}

// Motion: chorus. One modulated tap per side on a short line (triangle LFOs a quarter cycle apart:
// width), blended with the dry signal. amount 0..1 = wet blend (to 50 %) and a little more depth;
// rate 0..1 = LFO 0.08..2 Hz. Memory: 2 * kLength floats.
class Chorus {
public:
    static constexpr uint32_t kLength = 2048;                       // 42.7 ms at 48 kHz
    void Init(float* memory, float rate) {
        left_ = memory; right_ = memory ? memory + kLength : nullptr; rate_ = rate; dirty_ = true;
        Set(0.f, .3f);
    }
    void Set(float amount, float speed) {
        amount = Clamp(amount, 0.f, 1.f); speed = Clamp(speed, 0.f, 1.f);
        mix_target_ = .5f * amount;
        depth_ = (.002f + .004f * amount) * rate_;                  // 2..6 ms swing
        increment_ = (.08f + 1.92f * speed * speed) / rate_;
    }
    bool Neutral() const { return !left_ || (mix_target_ == 0.f && mix_ < 1e-4f); }
    // Skipped while neutral: the line is cleared on the way back so no old audio replays.
    void Rest() { mix_ = 0.f; dirty_ = true; }
    void Process(float& l, float& r) {
        if(dirty_) { std::memset(left_, 0, 2 * kLength * sizeof(float)); dirty_ = false; }
        mix_ += .001f * (mix_target_ - mix_);
        phase_ += increment_; if(phase_ >= 1.f) phase_ -= 1.f;
        float other = phase_ + .25f; if(other >= 1.f) other -= 1.f;
        left_[write_] = l; right_[write_] = r;
        const float base = .015f * rate_;                           // 15 ms centre
        const float wl = Read(left_, base + depth_ * Triangle(phase_));
        const float wr = Read(right_, base + depth_ * Triangle(other));
        if(++write_ >= kLength) write_ = 0;
        l += mix_ * (wl - l); r += mix_ * (wr - r);
    }
private:
    static float Triangle(float x) { const float t = x - .5f; return 4.f * (t < 0.f ? -t : t) - 1.f; }
    float Read(const float* line, float delay) const {
        float position = static_cast<float>(write_) - delay;
        if(position < 0.f) position += static_cast<float>(kLength);
        const uint32_t i = static_cast<uint32_t>(position);
        const uint32_t j = i + 1 >= kLength ? 0 : i + 1;
        const float f = position - static_cast<float>(i);
        return line[i] + f * (line[j] - line[i]);
    }
    float* left_ = nullptr; float* right_ = nullptr;
    uint32_t write_ = 0;
    float rate_ = 48000.f, mix_ = 0.f, mix_target_ = 0.f, depth_ = 0.f, phase_ = 0.f, increment_ = 0.f;
    bool dirty_ = true;
};

// Space: cloud's front end. Four all-pass stages per side whose lengths drift slowly (a few
// samples), smearing every note into a haze before the reverb's tail (Reverb in its cloud setting:
// longer, darker). All-passes keep the level; the drift keeps the tail from ringing.
// Memory: Required() floats.
class Diffuser {
public:
    static constexpr unsigned kStages = 4;
    static constexpr uint32_t kSwing = 12, kPad = 2 * kSwing + 2;   // drift in samples, line headroom
    static constexpr float kGain = .62f;
    static uint32_t Length(unsigned side, unsigned stage) {         // samples at 48 kHz, mutually prime
        static const uint16_t kLengths[2][kStages] = {{241, 383, 571, 773}, {263, 401, 599, 797}};
        return kLengths[side][stage];
    }
    static uint32_t Required() {
        uint32_t total = 0;
        for(unsigned s = 0; s < 2; ++s) for(unsigned k = 0; k < kStages; ++k) total += Length(s, k) + kPad;
        return total;
    }
    bool Init(float* memory, uint32_t capacity, float rate) {
        ready_ = false;
        if(!memory || capacity < Required() || !(rate >= 1000.f)) return false;
        float* cursor = memory;
        for(unsigned s = 0; s < 2; ++s) for(unsigned k = 0; k < kStages; ++k) {
            line_[s][k] = cursor; size_[s][k] = Length(s, k) + kPad; cursor += size_[s][k];
        }
        memory_ = memory; total_ = Required();
        increment_ = .13f / rate;                                   // one drift cycle per ~8 s
        dirty_ = ready_ = true;
        return true;
    }
    void Rest() { dirty_ = true; }
    void Process(float& l, float& r) {
        if(!ready_) return;
        if(dirty_) { std::memset(memory_, 0, total_ * sizeof(float)); dirty_ = false; }
        phase_ += increment_; if(phase_ >= 1.f) phase_ -= 1.f;
        l = Side(0, l); r = Side(1, r);
    }
private:
    float Side(unsigned s, float x) {
        for(unsigned k = 0; k < kStages; ++k) {
            // Each stage drifts on its own phase offset (triangle), so the smear never lines up.
            float p = phase_ + .25f * static_cast<float>(k) + .125f * static_cast<float>(s);
            while(p >= 1.f) p -= 1.f;
            const float t = p - .5f, tri = 4.f * (t < 0.f ? -t : t) - 1.f;
            const float delay = static_cast<float>(Length(s, k)) + static_cast<float>(kSwing) * (1.f + tri);
            float* line = line_[s][k]; const uint32_t size = size_[s][k];
            float position = static_cast<float>(write_[s][k]) - delay;
            if(position < 0.f) position += static_cast<float>(size);
            const uint32_t i = static_cast<uint32_t>(position);
            const uint32_t j = i + 1 >= size ? 0 : i + 1;
            const float f = position - static_cast<float>(i);
            const float delayed = line[i] + f * (line[j] - line[i]);
            const float v = x + kGain * delayed;                    // Schroeder all-pass
            line[write_[s][k]] = v;
            if(++write_[s][k] >= size) write_[s][k] = 0;
            x = delayed - kGain * v;
        }
        return x;
    }
    float* line_[2][kStages]{};
    uint32_t size_[2][kStages]{}, write_[2][kStages]{};
    float* memory_ = nullptr; uint32_t total_ = 0;
    float phase_ = 0.f, increment_ = 0.f;
    bool dirty_ = true, ready_ = false;
};

} // namespace fx
} // namespace forge
