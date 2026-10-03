#pragma once
#include <algorithm>
#include <cstddef>
#include "parameters.h"
#include "reverb.h"
#include "synth.h"

namespace forge {
// Allocation-free source -> stereo delay -> reverb -> output. Caller owns the
// delay buffers and (optionally) reverb memory and must initialize before
// starting audio. No IO, locks, parsing or allocation here. Without reverb
// memory the reverb stage is silent (wet 0) but everything else works.
class Engine {
public:
    bool Init(float sample_rate, float* left, float* right, size_t capacity,
              float* reverb_memory, size_t reverb_capacity) {
        if(!Init(sample_rate, left, right, capacity)) return false;
        has_reverb_ = reverb_.Init(sample_rate, reverb_memory, reverb_capacity);
        if(!has_reverb_) ready_ = false;
        return has_reverb_;
    }
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
        reverb_mix_ = 0.f; reverb_active_ = false; has_reverb_ = false;
        ready_ = true;
        return true;
    }
    bool Apply(Command command) {
        if(!parameters_.Apply(command)) return false;
        if(command.parameter != Parameter::Mix && command.parameter != Parameter::Time
           && command.parameter != Parameter::Feedback && command.parameter != Parameter::Level
           && command.parameter != Parameter::Bypass && command.parameter != Parameter::ReverbMix)
            synth_.Configure(parameters_);   // cutoff, resonance, sampler pitch/start/end (knobs resolve inside)
        return true;
    }
    // Sampler memory (v4); see sample_table.h. May be set before or after Init.
    void SetSamples(const SampleTable* table) { synth_.SetSamples(table); }
    void ReleaseSampleVoices(bool include_recording) { synth_.ReleaseSampleVoices(include_recording); }
    void SetSampleFilesAvailable(bool available) { synth_.SetSampleFilesAvailable(available); }
    bool SampleVoicesActive(bool include_recording) const { return synth_.SampleVoicesActive(include_recording); }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(parameters_.synth) synth_.Note(note, velocity, source);
    }
    // Controller state is kept on either route; Panic and route changes reset it.
    void Pedal(uint8_t source, bool down) { synth_.Pedal(source, down); }
    void Bend(uint8_t source, uint16_t value) { synth_.Bend(source, value); }
    void ResetControllers(uint8_t source) { synth_.ResetControllers(source); }
    void ModWheel(uint8_t value) { synth_.ModWheel(value); }
    void Panic() {
        synth_.Silence();
        // O(1) tail suppression: old delay cells are not read until overwritten.
        flushed_ = capacity_;
        reverb_.Clear();
    }
    unsigned ActiveVoices() const { return synth_.Active(); }
    // Called only by the audio owner, between blocks. Validate before mutation;
    // smoothing and delay state continue uninterrupted across a patch change.
    bool ApplyPatch(const Parameters& patch) {
        if(!patch.Valid()) return false;
        // Structural changes (route, waveform, v1/v2 <-> v3 voice architecture) silence.
        if(patch.synth != parameters_.synth || patch.waveform != parameters_.waveform
           || (patch.version >= 3) != (parameters_.version >= 3) || patch.Sampler() != parameters_.Sampler()
           || (patch.Sampler() && patch.sample_mode != parameters_.sample_mode)) Panic();
        parameters_ = patch;
        synth_.Configure(parameters_);
        if(has_reverb_) reverb_.Configure(parameters_.reverb_size, parameters_.reverb_damping);
        return true;
    }
    const Parameters& GetParameters() const { return parameters_; }

    void Process(float left, float right, float& out_left, float& out_right) {
        if(!ready_) { out_left = out_right = 0.f; return; }
        left = Sanitize(left);
        right = Sanitize(right);
        if(parameters_.synth) synth_.Process(left, right);
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
        float mixed_left = (1.f - mix_) * left + mix_ * delayed_left;
        float mixed_right = (1.f - mix_) * right + mix_ * delayed_right;
        // Reverb (v3 only). Skipped entirely while its mix is zero; restarting it
        // clears stale history in O(1).
        const float reverb_target = has_reverb_ && parameters_.version >= 3 ? parameters_.reverb_mix : 0.f;
        Smooth(reverb_mix_, reverb_target);
        if(reverb_target > 0.f || reverb_mix_ > 1e-4f) {
            if(!reverb_active_) { reverb_.Clear(); reverb_active_ = true; }
            float wet_left, wet_right;
            reverb_.Process(mixed_left, mixed_right, wet_left, wet_right);
            mixed_left += reverb_mix_ * (wet_left - mixed_left);
            mixed_right += reverb_mix_ * (wet_right - mixed_right);
        } else {
            reverb_active_ = false; reverb_mix_ = 0.f;
        }
        out_left = Sanitize(level_ * mixed_left);
        out_right = Sanitize(level_ * mixed_right);
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
    float mix_ = 0.f, feedback_ = 0.f, level_ = 0.f, time_ = 0.f, reverb_mix_ = 0.f;
    Reverb reverb_;
    bool ready_ = false, has_reverb_ = false, reverb_active_ = false;
};
} // namespace forge
