#pragma once
#include <cstdint>
#include <cmath>
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
// Choices per module, in key order.
// The FX page's keys (MIDI notes of the panel keys): each module's three choices, in key order.
constexpr uint8_t kKeys[kModules][kChoices] = {{48, 50, 52}, {53, 55, 57}, {60, 62, 64}, {65, 67, 69}};
constexpr uint8_t kRouteKey = 58, kBeforeLoopKey = 54, kAfterLoopKey = 56;   // A#3, F#3, G#3
enum class SpaceChoice : uint8_t { EchoRoom, Cloud, TapeEcho };
enum class ColourChoice : uint8_t { Saturation, Fold, Crush };
enum class TextureChoice : uint8_t { Cassette, Broken, Squash };
enum class MotionChoice : uint8_t { Chorus, Phaser, Tremolo };
inline bool Built(Module, uint8_t choice) { return choice < kChoices; }   // Phase 2 (0.20): all twelve

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

// ---- Phase 2 (0.20): the other seven effects. Each has Set(amount, character) with amount 0 = off
// (skipped once settled), bounded work per sample and no allocation. character is the module's
// hold + turn control (0 = default). ----

inline float Triangle01(float x) { const float t = x - .5f; return 4.f * (t < 0.f ? -t : t) - 1.f; }   // phase 0..1 -> -1..1
inline float Floor(float x) { const float i = static_cast<float>(static_cast<int>(x)); return i > x ? i - 1.f : i; }
inline float Clip(float x) { return x / (1.f + (x < 0.f ? -x : x)); }      // soft, never above 1

// Colour: fold. A triangle wavefolder: driven past full scale the signal folds back, glassy then
// metallic. amount = drive 1..8; character = bias (asymmetric folds: even harmonics).
class Fold {
public:
    void Set(float amount, float character) {
        amount = Clamp(amount, 0.f, 1.f); on_ = amount > 0.f;
        drive_target_ = 1.f + 7.f * amount; bias_target_ = .35f * Clamp(character, 0.f, 1.f);
    }
    bool Neutral() const { return !on_ && mix_ < 1e-4f; }
    void Process(float& l, float& r) {
        mix_ += .002f * ((on_ ? 1.f : 0.f) - mix_);
        drive_ += .002f * (drive_target_ - drive_); bias_ += .002f * (bias_target_ - bias_);
        const float centre = Shape(bias_);
        l += mix_ * (.8f * (Shape(drive_ * l + bias_) - centre) - l);
        r += mix_ * (.8f * (Shape(drive_ * r + bias_) - centre) - r);
    }
private:
    static float Shape(float x) { float t = (x + 1.f) * .25f; t -= Floor(t); const float d = t - .5f; return 1.f - 4.f * (d < 0.f ? -d : d); }
    float drive_ = 1.f, drive_target_ = 1.f, bias_ = 0.f, bias_target_ = 0.f, mix_ = 0.f;
    bool on_ = false;
};

// Colour: crush. Fewer bits (12 down to 3) and, with character, a lower sample rate (each sample held
// up to 24 times): grit, then aliasing lo-fi.
class Crush {
public:
    void Set(float amount, float character) {
        amount = Clamp(amount, 0.f, 1.f); character = Clamp(character, 0.f, 1.f); on_ = amount > 0.f;
        levels_ = static_cast<float>(1u << (12u - static_cast<unsigned>(9.f * amount + .5f)));
        hold_ = 1u + static_cast<uint32_t>(23.f * character * character + .5f);
    }
    bool Neutral() const { return !on_ && mix_ < 1e-4f; }
    void Process(float& l, float& r) {
        mix_ += .002f * ((on_ ? 1.f : 0.f) - mix_);
        if(++count_ >= hold_) { count_ = 0; held_l_ = Quantise(l); held_r_ = Quantise(r); }
        l += mix_ * (held_l_ - l); r += mix_ * (held_r_ - r);
    }
private:
    float Quantise(float x) const { const float y = x * levels_; return Floor(y + .5f) / levels_; }
    float levels_ = 4096.f, held_l_ = 0.f, held_r_ = 0.f, mix_ = 0.f;
    uint32_t hold_ = 1, count_ = 0;
    bool on_ = false;
};

// Texture: squash. A heavy compressor that pumps: fast attack, release 40-400 ms (character), the
// threshold falling and the make-up rising with amount; soft-clipped so it never exceeds full scale.
class Squash {
public:
    void Init(float rate) { rate_ = rate; attack_ = 1.f - std::exp(-1.f / (.001f * rate)); Set(0.f, 0.f); }
    void Set(float amount, float character) {
        amount = Clamp(amount, 0.f, 1.f); on_ = amount > 0.f;
        threshold_ = .5f - .45f * amount; makeup_ = 1.f + 2.5f * amount;
        release_ = 1.f - std::exp(-1.f / ((.04f + .36f * Clamp(character, 0.f, 1.f)) * rate_));
    }
    bool Neutral() const { return !on_ && mix_ < 1e-4f; }
    void Process(float& l, float& r) {
        mix_ += .002f * ((on_ ? 1.f : 0.f) - mix_);
        const float peak = std::fmax(std::fabs(l), std::fabs(r));
        env_ += (peak > env_ ? attack_ : release_) * (peak - env_);
        const float gain = (env_ > threshold_ ? threshold_ / env_ : 1.f) * makeup_;
        l += mix_ * (Clip(l * gain) - l); r += mix_ * (Clip(r * gain) - r);
    }
private:
    float rate_ = 48000.f, attack_ = 0.f, release_ = 0.f, threshold_ = .5f, makeup_ = 1.f, env_ = 0.f, mix_ = 0.f;
    bool on_ = false;
};

// Texture: broken. Failing tape: the sound drops out for moments at random (amount: how often and how
// deep), with character the dropouts get longer and turn into stutters (the signal chopped on and off).
class Broken {
public:
    void Init(float rate) { rate_ = rate; Set(0.f, 0.f); }
    void Set(float amount, float character) { amount_ = Clamp(amount, 0.f, 1.f); character_ = Clamp(character, 0.f, 1.f); }
    bool Neutral() const { return amount_ == 0.f && gain_ > .9999f && !left_; }
    void Process(float& l, float& r) {
        if(left_) {                                                      // inside a dropout
            --left_;
            const bool chop = character_ > .5f && ((left_ / static_cast<uint32_t>(.016f * rate_)) & 1u);
            target_ = chop ? 1.f : floor_;
        } else {
            target_ = 1.f;
            if(amount_ > 0.f && wait_ == 0) {
                rand_ ^= rand_ << 13; rand_ ^= rand_ >> 17; rand_ ^= rand_ << 5;
                const float u = static_cast<float>(rand_ >> 8) * (1.f / 16777216.f);
                left_ = static_cast<uint32_t>((.02f + .23f * character_ + .1f * u) * rate_);
                floor_ = 1.f - (.5f + .5f * amount_) * (.6f + .4f * u);
                wait_ = static_cast<uint32_t>((.25f + 2.5f * (1.f - amount_) * (.5f + u)) * rate_);
            } else if(wait_) --wait_;
        }
        gain_ += .004f * (target_ - gain_);                              // ~5 ms edges: no clicks
        l *= gain_; r *= gain_;
    }
private:
    float rate_ = 48000.f, amount_ = 0.f, character_ = 0.f, gain_ = 1.f, target_ = 1.f, floor_ = 1.f;
    uint32_t left_ = 0, wait_ = 0, rand_ = 0x6d2b79f5u;
};

// Motion: phaser. Four all-pass stages per side swept by a slow LFO (the sides a quarter apart), with
// feedback; mixed half and half with the dry signal for the notches. amount = depth and feedback;
// rate as Motion's (0.05-1.3 Hz here). The coefficients follow the sweep every 16 samples.
class Phaser {
public:
    void Init(float rate) { rate_ = rate; Set(0.f, .3f); }
    void Set(float amount, float speed) {
        amount = Clamp(amount, 0.f, 1.f); on_ = amount > 0.f;
        depth_ = amount; feedback_ = .45f * amount; wet_gain_ = 1.f / (1.f + feedback_);   // feedback resonance: level kept
        increment_ = (.05f + 1.25f * Clamp(speed, 0.f, 1.f) * Clamp(speed, 0.f, 1.f)) / rate_;
    }
    bool Neutral() const { return !on_ && mix_ < 1e-4f; }
    void Rest() { for(auto& s : z_) for(float& v : s) v = 0.f; last_[0] = last_[1] = 0.f; }
    void Process(float& l, float& r) {
        mix_ += .002f * ((on_ ? .5f : 0.f) - mix_);
        phase_ += increment_; if(phase_ >= 1.f) phase_ -= 1.f;
        if(++tick_ >= 16) {
            tick_ = 0;
            for(unsigned s = 0; s < 2; ++s) {
                float p = phase_ + .25f * static_cast<float>(s); if(p >= 1.f) p -= 1.f;
                const float sweep = .5f + .5f * depth_ * Triangle01(p);             // 0..1
                const float hz = 200.f * std::exp(2.996f * sweep);                  // 200 Hz .. 4 kHz
                const float w = 3.14159265f * hz / rate_;
                a_[s] = (w - 1.f) / (w + 1.f);
            }
        }
        l += mix_ * (wet_gain_ * Side(0, l) - l); r += mix_ * (wet_gain_ * Side(1, r) - r);
    }
private:
    float Side(unsigned s, float x) {
        float y = x + feedback_ * last_[s];
        for(float& z : z_[s]) { const float out = a_[s] * y + z; z = y - a_[s] * out; y = out; }
        if(std::fabs(y) < 1e-20f) y = 0.f;
        last_[s] = y;
        return y;
    }
    float rate_ = 48000.f, depth_ = 0.f, feedback_ = 0.f, wet_gain_ = 1.f, increment_ = 0.f, phase_ = 0.f, mix_ = 0.f;
    float a_[2] = {-.97f, -.97f}, z_[2][4] = {}, last_[2] = {};
    unsigned tick_ = 16;
    bool on_ = false;
};

// Motion: tremolo / pan. The level pulses (0.3-10 Hz, Motion's rate), the two sides in opposite phase,
// so at full depth the sound swings left and right.
class Tremolo {
public:
    void Init(float rate) { rate_ = rate; Set(0.f, .3f); }
    void Set(float amount, float speed) {
        depth_target_ = Clamp(amount, 0.f, 1.f);
        increment_ = (.3f + 9.7f * Clamp(speed, 0.f, 1.f) * Clamp(speed, 0.f, 1.f)) / rate_;
    }
    bool Neutral() const { return depth_target_ == 0.f && depth_ < 1e-4f; }
    void Process(float& l, float& r) {
        depth_ += .002f * (depth_target_ - depth_);
        phase_ += increment_; if(phase_ >= 1.f) phase_ -= 1.f;
        const float x = 2.f * phase_ - 1.f, s = 4.f * x * (1.f - (x < 0.f ? -x : x));   // parabolic sine
        l *= 1.f - depth_ * (.5f + .5f * s); r *= 1.f - depth_ * (.5f - .5f * s);
    }
private:
    float rate_ = 48000.f, depth_ = 0.f, depth_target_ = 0.f, increment_ = 0.f, phase_ = 0.f;
};

} // namespace fx
} // namespace forge
