#pragma once
#include <algorithm>
#include <cstddef>
#include "looper.h"
#include "parameters.h"
#include "reverb.h"
#include "synth.h"
#include "tape_fx.h"

namespace forge {
// Sampler voices while the looper writes. 6 keeps "7 sampler voices + overdub"
// under WAVE's emulated cost (make bench); raise to 7 if hardware CPU allows.
constexpr unsigned kLooperVoiceCap = 6;
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
        parameters_ = patch_ = Parameters{};
        performance_ = Performance{};
        synth_.Init(sample_rate); synth_.Configure(parameters_);
        synth_.SetPerformance(TapeSpeedRatio(performance_.speed), performance_.voice_gain, performance_.pan);
        effects_.Configure(performance_);
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
        command.parameter = parameters_.Resolve(command.parameter);       // knobs -> their control
        if(IsPerformance(command.parameter)) return ApplyPerformance(command);
        if(!parameters_.Apply(command)) return false;
        const Parameter p = command.parameter;
        if(p == Parameter::ReverbSize || p == Parameter::ReverbDamping) {
            if(has_reverb_) reverb_.Configure(parameters_.reverb_size, parameters_.reverb_damping);
        } else if(p != Parameter::Mix && p != Parameter::Time && p != Parameter::Feedback && p != Parameter::Level
                  && p != Parameter::Bypass && p != Parameter::ReverbMix)
            synth_.Configure(parameters_);   // filter, envelope, LFO, oscillator, sampler controls
        return true;
    }
    // A control's current value (patch or performance).
    float Value(Parameter parameter) const {
        parameter = parameters_.Resolve(parameter);
        if(parameter == Parameter::Space) return parameters_.feedback;
        if(IsPerformance(parameter)) { float* f = const_cast<Performance&>(performance_).Field(parameter); return f ? *f : 0.f; }
        return parameters_.Value(parameter);
    }
    // Back to the patch's value (performance controls: TAPE's default). Knob long press.
    bool ResetControl(Parameter parameter) {
        parameter = parameters_.Resolve(parameter);
        if(parameter == Parameter::Space) {
            const bool a = Apply({Parameter::Mix, patch_.mix}), b = Apply({Parameter::Feedback, patch_.feedback});
            if(parameters_.version >= 3) Apply({Parameter::ReverbMix, patch_.reverb_mix});
            return a && b;
        }
        if(IsPerformance(parameter)) return Apply({parameter, Performance::Default(parameter)});
        return Apply({parameter, patch_.Value(parameter)});
    }
    const Performance& GetPerformance() const { return performance_; }
    // TAPE's warble needs 2 * tape::Warble::kLength floats of zeroed memory (off without).
    void SetWarbleMemory(float* memory) { effects_.SetWarbleMemory(memory); }
    // Sampler memory (v4); see sample_table.h. May be set before or after Init.
    void SetSamples(const SampleTable* table) { synth_.SetSamples(table); }
    void ReleaseSampleVoices(bool include_recording) { synth_.ReleaseSampleVoices(include_recording); }
    void SetSampleFilesAvailable(bool available) { synth_.SetSampleFilesAvailable(available); }
    bool SampleVoicesActive(bool include_recording) const { return synth_.SampleVoicesActive(include_recording); }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(velocity && looper_) looper_->NoteStarted();      // an armed looper starts recording
        if(parameters_.synth) synth_.Note(note, velocity, source);
    }
    // Controller state is kept on either route; Panic and route changes reset it.
    void Pedal(uint8_t source, bool down) { synth_.Pedal(source, down); }
    void Bend(uint8_t source, uint16_t value) { synth_.Bend(source, value); }
    void ResetControllers(uint8_t source) { synth_.ResetControllers(source); }
    void ModWheel(uint8_t value) { synth_.ModWheel(value); }
    // Panic (SW5, CC 120/123, SysEx, recovery): everything silent at once; a
    // loop is paused, not lost. Patch changes use Silence() and leave the loop.
    void Panic() { Silence(); if(looper_) looper_->Panic(); }
    // Looper (docs/forge/LOOPING.md). Owned by the caller; audio owner only.
    void SetLooper(Looper* looper) { looper_ = looper; }
    Looper* GetLooper() const { return looper_; }
    void SetFxBeforeLoop(bool before) { fx_before_loop_ = before; }
    bool FxBeforeLoop() const { return fx_before_loop_; }
    // MIDI: CC 26 PLAY, CC 27 LOOP (as TAPE); CC 24 follows SW5: the looper
    // transport while a loop exists, else the filter cutoff.
    FORGE_NOINLINE void LooperControl(uint8_t control, uint8_t value) {
        if(control == 2) {
            if(looper_ && looper_->HasLoop()) looper_->SetSpeed(value * (4.f / 127.f) - 2.f);
            else { Command command; if(DecodeCC(0, 24, value, command)) Apply(command); }
            return;
        }
        if(!looper_ || control > 1) return;
        const int edge = looper_cc_[control].Edge(value);
        if(edge && control == 0) looper_->Play(edge > 0);
        if(edge && control == 1) looper_->Loop(edge > 0);
    }
    unsigned ActiveVoices() const { return synth_.Active(); }
    // Called only by the audio owner, between blocks. Validate before mutation;
    // smoothing and delay state continue uninterrupted across a patch change.
    FORGE_NOINLINE bool ApplyPatch(const Parameters& patch) {
        if(!patch.Valid()) return false;
        // Structural changes (route, waveform, v1/v2 <-> v3 voice architecture) silence.
        if(patch.synth != parameters_.synth || patch.waveform != parameters_.waveform
           || (patch.version >= 3) != (parameters_.version >= 3) || patch.Sampler() != parameters_.Sampler()
           || (patch.Sampler() && patch.sample_mode != parameters_.sample_mode)) Silence();
        parameters_ = patch_ = patch;
#ifdef FORGE_TEST_HOOKS
        ++patch_revision_;
#endif
        synth_.Configure(parameters_);
        if(has_reverb_) reverb_.Configure(parameters_.reverb_size, parameters_.reverb_damping);
        return true;
    }
    const Parameters& GetParameters() const { return parameters_; }
#ifdef FORGE_TEST_HOOKS
    uint32_t PatchRevision() const { return patch_revision_; }
    void ObserveVoiceEdges(SpscQueue<InspectorEvent,64>& events, std::atomic<uint32_t>& drops, uint32_t now) {
        synth_.ObserveVoiceEdges(events,drops,now);
    }
    void Inspect(InspectorAudio& a) const {
        a.patch=parameters_; synth_.Inspect(a);
        a.mix=mix_; a.feedback=feedback_; a.level=level_; a.delay_samples=time_; a.reverb_mix=reverb_mix_;
        if(looper_) {
            a.loop_flags=static_cast<uint8_t>(static_cast<unsigned>(looper_->GetState())|(looper_->Overdubbing()?8:0)
                                              |(fx_before_loop_?16:0)|(looper_->Locked()?32:0));
            a.loop_length=looper_->Length(); a.loop_position=looper_->Position();
            a.loop_speed=looper_->Speed(); a.loop_feedback=looper_->Feedback();
        }
    }
#endif

    void Process(float left, float right, float& out_left, float& out_right) {
        if(!ready_) { out_left = out_right = 0.f; return; }
        left = Sanitize(left);
        right = Sanitize(right);
        // While the looper records or overdubs, sampler voices are capped (kLooperVoiceCap)
        // so the worst case stays within the CPU budget (docs/forge/LOOPING.md).
        if(looper_) synth_.SetVoiceCap(looper_->Writing() ? kLooperVoiceCap : 7);
        if(parameters_.synth) synth_.Process(left, right);
        if(looper_ && !fx_before_loop_) looper_->Process(left, right);     // effects after the loop
        effects_.Process(left, right);                                      // TAPE: filter, saturation, warble
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
        if(looper_ && fx_before_loop_) looper_->Process(mixed_left, mixed_right);   // the loop records what you hear
        out_left = level_ * mixed_left; out_right = level_ * mixed_right;
        effects_.Output(out_left, out_right);                               // TAPE's output compressor
        out_left = Sanitize(out_left);
        out_right = Sanitize(out_right);
    }
private:
    FORGE_NOINLINE bool ApplyPerformance(Command command) {
        if(!std::isfinite(command.value)) return false;
        const float v = Clamp(command.value, 0.f, 1.f);
        if(command.parameter == Parameter::Space) {            // TAPE SW3 page 1: delay and reverb from one value
            const bool ok = parameters_.Apply({Parameter::Feedback, v}) && parameters_.Apply({Parameter::Mix, .5f * v});
            parameters_.Apply({Parameter::ReverbMix, v});      // v3 and newer
            return ok;
        }
        float* field = performance_.Field(command.parameter);
        if(!field) return false;
        *field = v;
        synth_.SetPerformance(TapeSpeedRatio(performance_.speed), performance_.voice_gain, performance_.pan);
        effects_.Configure(performance_);
        return true;
    }
    void Silence() {
        synth_.Silence();
        // O(1) tail suppression: old delay cells are not read until overwritten.
        flushed_ = capacity_;
        reverb_.Clear();
    }
    Looper* looper_ = nullptr;
    bool fx_before_loop_ = true;
    CcButton looper_cc_[2];
#ifdef FORGE_TEST_HOOKS
    uint32_t patch_revision_=0;
#endif
    static float Sanitize(float value) {
        return std::isfinite(value) ? Clamp(value, -1.f, 1.f) : 0.f;
    }
    float DelaySamples() const { return sample_rate_ * (0.01f + 0.99f * parameters_.time); }
    void Smooth(float& current, float target) { current += smoothing_ * (target - current); }
    Parameters parameters_{}, patch_{};
    Performance performance_{};
    tape::Effects effects_;
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
