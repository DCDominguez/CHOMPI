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
// How a patch meets TAPE's per-slot settings (core/slot_settings.h) when it selects a
// chromatic sample slot: Recall (a Forge preset) and Select (the Samples menu) load the
// slot's saved settings; Select also resets an unsaved file slot to TAPE's defaults (as
// TAPE); Patch (the webapp / AI) keeps the patch's own values.
enum class SlotPolicy : uint8_t { Patch, Recall, Select };

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
        PushPerformance();
        synth_.SnapPerformance();
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
        if(SlotOwned(command.parameter) && SlotMode()) return ApplySlot(command);
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
    FORGE_NOINLINE float Value(Parameter parameter) const {
        parameter = parameters_.Resolve(parameter);
        if(parameter == Parameter::Space) return split_delay_ ? space_ : parameters_.feedback;
        if(SlotOwned(parameter) && SlotMode() && Kit()) return Field(PadValues(focus_), parameter);
        if(IsPerformance(parameter)) { float* f = const_cast<Performance&>(performance_).Field(parameter); return f ? *f : 0.f; }
        return parameters_.Value(parameter);
    }
    // Back to the patch's value (performance controls: TAPE's default). Knob long press.
    FORGE_NOINLINE bool ResetControl(Parameter parameter) {
        parameter = parameters_.Resolve(parameter);
        if(parameter == Parameter::Space) {
            const bool a = Apply({Parameter::Mix, patch_.mix}), b = Apply({Parameter::Feedback, patch_.feedback});
            if(parameters_.version >= 3) Apply({Parameter::ReverbMix, patch_.reverb_mix});
            return a && b;
        }
        if(SlotOwned(parameter) && SlotMode()) return Apply({parameter, Field(SlotValues{}, parameter)});   // TAPE's default
        if(IsPerformance(parameter)) return Apply({parameter, Performance::Default(parameter)});
        return Apply({parameter, patch_.Value(parameter)});
    }
    const Performance& GetPerformance() const { return performance_; }
    void SetSplitDelay(bool split) { split_delay_ = split; space_ = .5f; }
    // TAPE menu presses (SW1 / SW2): the sampler's auto-loop and sustain (hold) on/off.
    // With per-slot settings they belong to the slot (kit: the pad last played).
    FORGE_COLD void ToggleSampleLoop() {
        if(SlotMode() && Kit()) { SlotValues v = PadValues(focus_); v.loop = !v.loop; SavePad(v); return; }
        parameters_.sample_loop = !parameters_.sample_loop; synth_.Configure(parameters_); SaveSlot();
    }
    FORGE_COLD void ToggleSampleHold() {
        if(SlotMode() && Kit()) { SlotValues v = PadValues(focus_); v.sustain = !v.sustain; SavePad(v); return; }
        parameters_.sample_gate = !parameters_.sample_gate; synth_.Configure(parameters_); SaveSlot();
    }
    // TAPE's per-slot settings (shared with the main loop, which loads and writes the file).
    void SetSlotSettings(SlotSettings* settings) { slots_ = settings; slots_seen_ = settings ? settings->Changes() - 1 : 0; }
    // Once per block: pads follow changes made elsewhere (menu copy/erase, a new card).
    void SyncSlotSettings() { if(slots_ && slots_->Changes() != slots_seen_) Resync(); }
    FORGE_COLD void Resync() {
        slots_seen_ = slots_->Changes();
        if(SlotMode() && Kit()) LoadPads();
    }
    uint8_t FocusPad() const { return focus_; }
    // TAPE's warble needs 2 * tape::Warble::kLength floats of zeroed memory (off without).
    void SetWarbleMemory(float* memory) { effects_.SetWarbleMemory(memory); }
    // Sampler memory (v4); see sample_table.h. May be set before or after Init.
    void SetSamples(const SampleTable* table) { synth_.SetSamples(table); }
    void ReleaseSampleVoices(bool include_recording) { synth_.ReleaseSampleVoices(include_recording); }
    void SetSampleFilesAvailable(bool available) { synth_.SetSampleFilesAvailable(available); }
    bool SampleVoicesActive(bool include_recording) const { return synth_.SampleVoicesActive(include_recording); }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(velocity && looper_) looper_->NoteStarted();      // an armed looper starts recording
        if(velocity && SlotMode() && Kit()) { const int pad = Synth::KitSlot(note); if(pad >= 0) focus_ = static_cast<uint8_t>(pad); }
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
    FORGE_NOINLINE bool ApplyPatch(const Parameters& patch, SlotPolicy policy = SlotPolicy::Patch) {
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
        if(SlotMode()) {
            if(Kit()) LoadPads();
            else if(policy != SlotPolicy::Patch) {
                const unsigned slot = parameters_.sample_slot;
                if(slots_->Valid(0, parameters_.sample_bank, slot)) LoadSlot(slots_->Get(0, parameters_.sample_bank, slot));
                else if(policy == SlotPolicy::Select && slot != kRamSlot) LoadSlot(SlotValues{});
            }
        }
        PushPerformance();
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

    // monitor_l/r: input monitored before the effects (TAPE monitor modes BOTH and
    // SEND_RET); it goes through the effects and the looper like the instrument.
    void Process(float left, float right, float& out_left, float& out_right, float monitor_l = 0.f, float monitor_r = 0.f) {
        if(!ready_) { out_left = out_right = 0.f; return; }
        left = Sanitize(left);
        right = Sanitize(right);
        // While the looper records or overdubs, sampler voices are capped (kLooperVoiceCap)
        // so the worst case stays within the CPU budget (docs/forge/LOOPING.md).
        if(looper_) synth_.SetVoiceCap(looper_->Writing() ? kLooperVoiceCap : 7);
        if(parameters_.synth) synth_.Process(left, right);
        left += monitor_l; right += monitor_r;
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
            space_ = v;
            // Split Delay (options.json): left of centre = delay only, right = reverb only (centre = dry).
            const float delay = split_delay_ ? (v < .5f ? (.5f - v) * 2.f : 0.f) : v;
            const float reverb = split_delay_ ? (v > .5f ? (v - .5f) * 2.f : 0.f) : v;
            const bool ok = parameters_.Apply({Parameter::Feedback, delay}) && parameters_.Apply({Parameter::Mix, .5f * delay});
            parameters_.Apply({Parameter::ReverbMix, reverb});      // v3 and newer
            return ok;
        }
        float* field = performance_.Field(command.parameter);
        if(!field) return false;
        *field = v;
        PushPerformance();
        effects_.Configure(performance_);
        return true;
    }
    // Kit pads carry their own pitch, gain and pan, so the shared ones stay neutral there.
    FORGE_NOINLINE void PushPerformance() {
        if(SlotMode() && Kit()) synth_.SetPerformance(1.f, kUnityGain, .5f);
        else synth_.SetPerformance(TapeSpeedRatio(performance_.speed), performance_.voice_gain, performance_.pan);
    }
    static constexpr float kUnityGain = .703562f;            // 2v^2 + .01 = 1
    // Per-slot settings: pitch (Speed), gain, pan, start, end, attack and release (decay)
    // of a sampler patch belong to its sample slot; loop and sustain too (toggles).
    static bool SlotOwned(Parameter p) {
        return p == Parameter::Speed || p == Parameter::VoiceGain || p == Parameter::Pan || p == Parameter::SampleStart
            || p == Parameter::SampleEnd || p == Parameter::Attack || p == Parameter::Release;
    }
    bool SlotMode() const { return slots_ && parameters_.Sampler(); }
    bool Kit() const { return parameters_.sample_mode == 1; }
    FORGE_NOINLINE static float Field(const SlotValues& v, Parameter p) {
        switch(p) {
            case Parameter::Speed: return v.pitch;       case Parameter::VoiceGain: return v.gain;
            case Parameter::Pan: return v.pan;           case Parameter::SampleStart: return v.start;
            case Parameter::SampleEnd: return v.end;     case Parameter::Attack: return v.attack;
            case Parameter::Release: return v.release;   default: return 0.f;
        }
    }
    // The playing controls as one slot's settings (chromatic, or a kit pad without its own).
    FORGE_NOINLINE SlotValues Current() const {
        SlotValues v;
        v.pitch = performance_.speed; v.gain = performance_.voice_gain; v.pan = performance_.pan;
        v.start = parameters_.sample_start; v.end = parameters_.sample_end;
        v.attack = parameters_.attack; v.release = parameters_.release;
        v.loop = parameters_.sample_loop; v.sustain = parameters_.sample_gate;
        return v;
    }
    FORGE_NOINLINE SlotValues PadValues(unsigned pad) const {
        return slots_->Valid(1, parameters_.sample_bank, pad) ? slots_->Get(1, parameters_.sample_bank, pad) : Current();
    }
    FORGE_COLD bool ApplySlot(Command command) {
        if(!std::isfinite(command.value)) return false;
        const float x = Clamp(command.value, 0.f, 1.f);
        if(Kit()) {
            SlotValues v = PadValues(focus_);
            switch(command.parameter) {
                case Parameter::Speed: v.pitch = x; break;   case Parameter::VoiceGain: v.gain = x; break;
                case Parameter::Pan: v.pan = x; break;       case Parameter::Attack: v.attack = x; break;
                case Parameter::Release: v.release = x; break;
                case Parameter::SampleStart: v.start = std::fmin(x, v.end - .01f); break;
                case Parameter::SampleEnd: v.end = std::fmax(x, v.start + .01f); break;
                default: return false;
            }
            SavePad(v);
            return true;
        }
        command.value = x;
        bool ok;
        if(IsPerformance(command.parameter)) ok = ApplyPerformance(command);
        else { ok = parameters_.Apply(command); if(ok) synth_.Configure(parameters_); }
        if(ok) SaveSlot();
        return ok;
    }
    // TAPE saves the moment a knob turns (DumpValuePresets).
    FORGE_NOINLINE void SaveSlot() { if(SlotMode() && !Kit()) slots_->Set(0, parameters_.sample_bank, parameters_.sample_slot, Current()); }
    FORGE_COLD void SavePad(const SlotValues& v) {
        slots_->Set(1, parameters_.sample_bank, focus_, v);
        synth_.SetPad(focus_, true, v);
        slots_seen_ = slots_->Changes();
    }
    FORGE_COLD void LoadSlot(const SlotValues& v) {
        performance_.speed = v.pitch; performance_.voice_gain = v.gain; performance_.pan = v.pan;
        parameters_.sample_start = Clamp(v.start, 0.f, .99f);
        parameters_.sample_end = std::fmax(Clamp(v.end, 0.f, 1.f), parameters_.sample_start + .01f);
        parameters_.attack = v.attack; parameters_.release = v.release;
        parameters_.sample_loop = v.loop; parameters_.sample_gate = v.sustain;
        synth_.Configure(parameters_);
    }
    FORGE_COLD void LoadPads() {
        for(unsigned pad = 0; pad < kSampleSlots; ++pad)
            synth_.SetPad(pad, slots_->Valid(1, parameters_.sample_bank, pad), slots_->Get(1, parameters_.sample_bank, pad));
        slots_seen_ = slots_->Changes();
    }
    void Silence() {
        synth_.Silence();
        // O(1) tail suppression: old delay cells are not read until overwritten.
        flushed_ = capacity_;
        reverb_.Clear();
    }
    SlotSettings* slots_ = nullptr;
    uint32_t slots_seen_ = 0;
    uint8_t focus_ = 0;                          // kit: the pad the knobs edit (the last one played)
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
    bool split_delay_ = false;
    float space_ = .5f;
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
