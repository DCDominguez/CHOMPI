#pragma once
#include <cmath>
#include <cstddef>
#include "parameters.h"

namespace forge {
// Allocation-free stereo reverb: a 4-line feedback delay network with an
// orthogonal (Hadamard) mix and per-line damping. Line gains are set from the requested decay time, so it is stable
// for every setting. The caller owns the memory (SDRAM on firmware).
// Clear() is O(1): reads return silence until every cell was rewritten.
class Reverb {
public:
    static size_t Required(float rate) {
        size_t total = 0;
        for(unsigned i = 0; i < kLines; ++i) total += Scaled(BaseLength(i), rate);
        return total;
    }
    bool Init(float rate, float* memory, size_t capacity) {
        ready_ = false;
        if(!memory || !(rate >= 1000.f) || capacity < Required(rate)) return false;
        rate_ = rate;
        float* cursor = memory;
        for(unsigned i = 0; i < kLines; ++i) {
            line_[i] = cursor; length_[i] = Scaled(BaseLength(i), rate); cursor += length_[i];
        }
        size_t longest = 0;
        for(unsigned i = 0; i < kLines; ++i) longest = length_[i] > longest ? length_[i] : longest;
        longest_ = longest;
        Configure(0.5f, 0.5f);
        Clear();
        ready_ = true;
        return true;
    }
    // size and damping are normalized 0..1 (see PROTOCOL for physical units).
    void Configure(float size, float damping) {
        const float rt60 = 0.2f * std::pow(50.f, Clamp(size, 0.f, 1.f));          // 0.2..10 s
        for(unsigned i = 0; i < kLines; ++i)
            gain_[i] = std::pow(10.f, -3.f * static_cast<float>(length_[i]) / (rt60 * rate_));
        const float hz = 16000.f * std::pow(1500.f / 16000.f, Clamp(damping, 0.f, 1.f)); // 16k..1.5k
        damp_ = 1.f - std::exp(-6.283185307f * Clamp(hz, 20.f, 0.45f * rate_) / rate_);
    }
    void Clear() {
        unread_ = longest_;
        for(unsigned i = 0; i < kLines; ++i) { low_[i] = 0.f; index_[i] = 0; }
    }
    // Returns the wet signal only.
    void Process(float left, float right, float& wet_left, float& wet_right) {
        if(!ready_) { wet_left = wet_right = 0.f; return; }
        const bool silent = unread_ > 0;
        float y[kLines];
        for(unsigned i = 0; i < kLines; ++i) y[i] = silent ? 0.f : line_[i][index_[i]];
        if(unread_) --unread_;
        // Hadamard 4x4 / 2: orthogonal, so losses come only from gain_ and damping.
        const float a = y[0] + y[1], b = y[0] - y[1], c = y[2] + y[3], d = y[2] - y[3];
        const float mixed[kLines] = {0.5f * (a + c), 0.5f * (b + d), 0.5f * (a - c), 0.5f * (b - d)};
        const float input = 0.5f * (left + right);
        for(unsigned i = 0; i < kLines; ++i) {
            low_[i] += damp_ * (mixed[i] - low_[i]);
            if(std::fabs(low_[i]) < 1e-20f) low_[i] = 0.f;
            line_[i][index_[i]] = (i & 1 ? -0.5f : 0.5f) * input + gain_[i] * low_[i];
            if(++index_[i] >= length_[i]) index_[i] = 0;
        }
        wet_left = 0.5f * (y[0] + y[2]);
        wet_right = 0.5f * (y[1] - y[3]);
    }
private:
    static constexpr unsigned kLines = 4;
    static size_t BaseLength(unsigned i) { // samples at 48 kHz, mutually prime
        return i == 0 ? 1601 : i == 1 ? 1949 : i == 2 ? 2311 : 2741;
    }
    static size_t Scaled(size_t length, float rate) {
        return static_cast<size_t>(static_cast<float>(length) * rate / 48000.f) + 1;
    }
    float* line_[kLines]{};
    size_t length_[kLines]{}, index_[kLines]{};
    float gain_[kLines]{}, low_[kLines]{};
    size_t longest_ = 0, unread_ = 0;
    float rate_ = 48000.f, damp_ = 1.f;
    bool ready_ = false;
};
} // namespace forge
