#pragma once
#include <algorithm>
#include <cstddef>
#include "parameters.h"
#include "synth.h"

namespace forge {
// Allocation-free stereo delay. Caller owns two distinct buffers and must
// initialize before starting audio. No IO, locks, parsing or allocation here.
class Engine {
public:
    bool Init(float sample_rate, float* left, float* right, size_t capacity) {
        ready_ = false;
        if(!std::isfinite(sample_rate) || sample_rate < 1000.f
           || sample_rate > 192000.f || !left || !right || left == right
           || capacity < static_cast<size_t>(sample_rate) + 2) return false;
        sample_rate_ = sample_rate;
        left_ = left;
        right_ = right;
        capacity_ = capacity;
        std::fill(left_, left_ + capacity_, 0.f);
        std::fill(right_, right_ + capacity_, 0.f);
        parameters_ = Parameters{};
        synth_.Init(sample_rate); synth_.Configure(parameters_);
        mix_ = parameters_.mix;
        feedback_ = parameters_.feedback * 0.85f;
        level_ = 0.f; // fade up from silence at boot
        time_ = DelaySamples();
        smoothing_ = 1.f - std::exp(-1.f / (0.02f * sample_rate_));
        write_ = 0;
        flushed_ = 0;
        ready_ = true;
        return true;
    }
    bool Apply(Command command) {
        if(!parameters_.Apply(command)) return false;
        if(command.parameter == Parameter::Cutoff) synth_.Configure(parameters_);
        return true;
    }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(parameters_.synth) synth_.Note(note, velocity, source);
    }
    void Panic() {
        synth_.Silence();
        // O(1) tail suppression: old delay cells are not read until overwritten.
        flushed_ = capacity_;
    }
    unsigned ActiveVoices() const { return synth_.Active(); }
    // Called only by the audio owner, between blocks. Validate before mutation;
    // smoothing and delay state continue uninterrupted across a patch change.
    bool ApplyPatch(const Parameters& patch) {
        if(!patch.Valid()) return false;
        if(patch.synth != parameters_.synth || patch.waveform != parameters_.waveform) Panic();
        parameters_ = patch;
        synth_.Configure(parameters_);
        return true;
    }
    const Parameters& GetParameters() const { return parameters_; }

    void Process(float left, float right, float& out_left, float& out_right) {
        if(!ready_) { out_left = out_right = 0.f; return; }
        left = Sanitize(left);
        right = Sanitize(right);
        if(parameters_.synth) left = right = synth_.Process();
        Smooth(mix_, parameters_.bypass ? 0.f : parameters_.mix);
        Smooth(feedback_, parameters_.feedback * 0.85f);
        Smooth(level_, parameters_.level);
        Smooth(time_, DelaySamples());
        float read = static_cast<float>(write_) - time_;
        if(read < 0.f) read += static_cast<float>(capacity_);
        const size_t index = static_cast<size_t>(read);
        const size_t next = (index + 1) % capacity_;
        const float fraction = read - static_cast<float>(index);
        const bool unread = flushed_ && static_cast<float>(capacity_ - flushed_) <= time_ + 1.f;
        const float delayed_left = unread ? 0.f : left_[index] + fraction * (left_[next] - left_[index]);
        const float delayed_right = unread ? 0.f : right_[index] + fraction * (right_[next] - right_[index]);
        if(flushed_) --flushed_;
        left_[write_] = left + feedback_ * delayed_left;
        right_[write_] = right + feedback_ * delayed_right;
        write_ = (write_ + 1) % capacity_;
        out_left = Sanitize(level_ * ((1.f - mix_) * left + mix_ * delayed_left));
        out_right = Sanitize(level_ * ((1.f - mix_) * right + mix_ * delayed_right));
    }
private:
    static float Sanitize(float value) {
        return std::isfinite(value) ? Clamp(value, -1.f, 1.f) : 0.f;
    }
    float DelaySamples() const { return sample_rate_ * (0.01f + 0.99f * parameters_.time); }
    void Smooth(float& current, float target) { current += smoothing_ * (target - current); }
    Parameters parameters_{};
    Synth synth_;
    float *left_ = nullptr, *right_ = nullptr;
    size_t capacity_ = 0, write_ = 0;
    size_t flushed_ = 0;
    float sample_rate_ = 48000.f, smoothing_ = 0.f;
    float mix_ = 0.f, feedback_ = 0.f, level_ = 0.f, time_ = 0.f;
    bool ready_ = false;
};
} // namespace forge
