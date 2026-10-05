#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include "parameters.h"

namespace forge {
// TAPE-style looper (docs/forge/LOOPING.md). Audio owner only: buttons, Tick
// and Process run in the audio callback. Stereo int16 loop memory (SDRAM),
// varispeed -2..+2 (negative = reverse) with linear interpolation, overdub
// with feedback ("dub gain") through a soft limiter, 5 ms fades written into
// the first take (seamless seam), 5 ms output fades for pause/clear/panic.
// Work per sample is bounded: one interpolated read and at most two writes.
//
// Keys (TAPE): LOOP = KEY_28, PLAY = KEY_27.
//   LOOP: empty -> first take; first take -> close, straight into overdub;
//         playing -> overdub on/off; paused -> resume with overdub; armed -> disarm.
//   PLAY: first take -> close, plain playback; overdub -> overdub off;
//         playing -> pause; paused -> resume on release (unless held).
//   Hold PLAY 2 s while paused: back to the start. Hold PLAY + LOOP 2 s: clear.
//   PLAY + LOOP while empty: armed; the next note starts the first take.
// Presses wait 10 ms so two keys pressed together count as a combination.
class Looper {
public:
    enum class State : uint8_t { Empty, Armed, FirstTake, Playing, Paused };

    void Init(int16_t* memory, uint32_t capacity_frames, float rate) {
        memory_ = memory; capacity_ = capacity_frames;
        fade_ = static_cast<uint32_t>(0.005f * rate); if(fade_ < 1) fade_ = 1;
        combo_ = static_cast<uint32_t>(0.010f * rate);
        hold_ = static_cast<uint32_t>(2.f * rate);
        scrub_period_ = static_cast<uint32_t>(0.125f * rate);
        gain_step_ = 1.f / fade_;                       // linear 5 ms output fades
        Reset();
        play_down_ = loop_down_ = pending_play_ = pending_loop_ = false;
        combo_used_ = jumped_ = false; held_ = 0; since_press_ = 0;
        feedback_ = feedback_target_ = 1.f; speed_target_ = 1.f; tape_slew_ = false;
    }

    // ---- controls (audio owner) ----
    void Play(bool down) {
        if(down && !play_down_) { play_down_ = true; pending_play_ = true; since_press_ = 0; held_ = 0; jumped_ = false;
                                   was_paused_ = state_ == State::Paused; }
        else if(!down && play_down_) {
            if(pending_play_) { pending_play_ = false; if(!loop_down_ && !combo_used_) PlayPress(); }
            play_down_ = false;
            // Paused before this press: resume on release, unless held to jump or used in a combination.
            if(was_paused_ && state_ == State::Paused && !jumped_ && !combo_used_) Resume(false);
            if(!loop_down_) combo_used_ = false;
        }
    }
    void Loop(bool down) {
        if(down && !loop_down_) { loop_down_ = true; pending_loop_ = true; since_press_ = 0; held_ = 0; }
        else if(!down && loop_down_) {
            if(pending_loop_) { pending_loop_ = false; if(!play_down_ && !combo_used_) LoopPress(); }
            loop_down_ = false;
            if(!play_down_) combo_used_ = false;
        }
    }
    // Once per audio block: resolves key combinations, hold gestures and scrubbing.
    void Tick(uint32_t frames) {
        since_press_ += frames;
        if(play_down_ || loop_down_) held_ += frames;            // since the latest press
        if(play_down_ && loop_down_) {
            pending_play_ = pending_loop_ = false;
            if(!combo_used_ && state_ == State::Empty) { state_ = State::Armed; combo_used_ = true; }
            if(held_ >= hold_ && state_ != State::Empty && state_ != State::Armed && !clearing_) { Clear(); combo_used_ = true; }
        } else if(since_press_ >= combo_) {
            if(pending_play_) { pending_play_ = false; PlayPress(); }
            if(pending_loop_) { pending_loop_ = false; LoopPress(); }
            if(play_down_ && held_ >= hold_ && state_ == State::Paused && !jumped_) {
                index_ = speed_ < 0.f ? length_ - 1 : 0; frac_ = 0.f; jumped_ = true;
            }
        }
        scrub_clock_ += frames;
        if(scrub_clock_ >= scrub_period_) { scrub_ = Clamp(scrub_turns_ * 0.2f, -2.f, 2.f); scrub_turns_ = 0.f; scrub_clock_ = 0; }
    }
    // Armed: the first played note starts the first take (TAPE "record arm").
    void NoteStarted() { if(state_ == State::Armed) StartTake(); }
    // Panic: silent at once, overdub off, the loop is kept (paused).
    void Panic() {
        overdub_ = false;
        if(state_ == State::FirstTake) CloseTake(false);
        if(state_ == State::Playing) state_ = State::Paused;
        if(state_ == State::Armed) state_ = State::Empty;
        gain_ = 0.f; speed_ = 0.f;                         // tape stopped dead, no fade
    }
    void Clear() { if(Locked()) return; if(state_ == State::FirstTake) CloseTake(false); clearing_ = state_ != State::Empty && state_ != State::Armed; if(!clearing_) Reset(); overdub_ = false; }
    // Transport (SW5 / CC 24): speed -2..+2, 1 = original, negative = reverse.
    void SetSpeed(float speed) { speed_target_ = Clamp(speed, -2.f, 2.f); }
    void NudgeSpeed(float delta) { SetSpeed(speed_target_ + delta); }          // SW5 push and turn (TAPE: .012 per click)
    void ResetSpeed() { speed_target_ = 1.f; }
    float SpeedTarget() const { return speed_target_; }
    void Scrub(int turns) { scrub_turns_ += static_cast<float>(turns); }       // while paused
    void SetTapeSlew(bool on) { tape_slew_ = on; }
    void AdjustFeedback(float delta) { feedback_target_ = Clamp(feedback_target_ + delta, 0.f, 1.f); }

    // ---- audio: (l, r) in = what the looper hears; out = that plus the loop ----
    FORGE_INLINE void Process(float& l, float& r) {
        if(state_ == State::FirstTake) { WriteTake(l, r); return; }
        if(!length_) return;
        const bool playing = state_ == State::Playing && !clearing_;
        const float target = playing ? speed_target_ : scrub_;
        speed_ += (target - speed_) * (tape_slew_ ? 0.0001f : 0.01f);
        const float audible = playing ? 1.f : clearing_ ? 0.f : Clamp(std::fabs(speed_) * 4.f, 0.f, 1.f);
        gain_ = audible > gain_ ? std::fmin(audible, gain_ + gain_step_) : std::fmax(audible, gain_ - gain_step_);
        feedback_ += (feedback_target_ - feedback_) * 0.001f;
        if(clearing_ && gain_ <= 0.f) { Reset(); return; }
        const uint32_t next = index_ + 1 < length_ ? index_ + 1 : 0;
        const int16_t* a = memory_ + 2 * static_cast<size_t>(index_);
        const int16_t* b = memory_ + 2 * static_cast<size_t>(next);
        const float out_l = (a[0] + (b[0] - a[0]) * frac_) * (gain_ / 32768.f);
        const float out_r = (a[1] + (b[1] - a[1]) * frac_) * (gain_ / 32768.f);
        // Advance; every frame the read head leaves is overdubbed (when on).
        frac_ += speed_;
        const bool dub = overdub_ && playing;
        while(frac_ >= 1.f) { if(dub) Dub(index_, l, r); index_ = index_ + 1 < length_ ? index_ + 1 : 0; frac_ -= 1.f; }
        while(frac_ < 0.f) { if(dub) Dub(index_, l, r); index_ = index_ ? index_ - 1 : length_ - 1; frac_ += 1.f; }
        l += out_l; r += out_r;
    }

    // ---- state (LEDs, Inspector, saving) ----
    State GetState() const { return clearing_ ? State::Paused : state_; }
    bool Empty() const { return state_ == State::Empty || state_ == State::Armed; }
    bool HasLoop() const { return length_ > 0 && !clearing_; }
    bool Overdubbing() const { return overdub_ && state_ == State::Playing; }
    // Saving the loop to a sample slot: the audio owner locks it (refused while
    // recording or overdubbing, or when empty); while locked the loop plays but
    // cannot be overdubbed, re-recorded or cleared. The main loop unlocks.
    bool Lock() {
        if(!HasLoop() || Writing() || locked_.load(std::memory_order_acquire)) return false;
        locked_.store(true, std::memory_order_release); return true;
    }
    void Unlock() { locked_.store(false, std::memory_order_release); }
    bool Locked() const { return locked_.load(std::memory_order_acquire); }
    bool Writing() const { return state_ == State::FirstTake || (overdub_ && state_ == State::Playing && !clearing_); }
    bool Clearing() const { return clearing_; }
    float Position() const { return length_ ? (index_ + frac_) / length_ : 0.f; }
    uint32_t Length() const { return state_ == State::FirstTake ? take_ : length_; }
    uint32_t Capacity() const { return capacity_; }
    float Speed() const { return speed_target_; }
    float Feedback() const { return feedback_target_; }
    const int16_t* Data() const { return memory_; }

private:
    static FORGE_INLINE float SoftLimit(float x) {        // transparent below 0.8, never beyond 1
        const float a = std::fabs(x);
        if(a <= 0.8f) return x;
        const float over = (a - 0.8f) * 5.f;
        return std::copysign(0.8f + 0.2f * over / (1.f + over), x);
    }
    static FORGE_INLINE int16_t ToInt(float x) { return static_cast<int16_t>(x * 32767.f); }
    FORGE_INLINE void Dub(uint32_t i, float l, float r) {
        int16_t* f = memory_ + 2 * static_cast<size_t>(i);
        f[0] = ToInt(SoftLimit(f[0] * (feedback_ / 32768.f) + l));
        f[1] = ToInt(SoftLimit(f[1] * (feedback_ / 32768.f) + r));
    }
    void WriteTake(float l, float r) {
        const float in = take_ < fade_ ? static_cast<float>(take_) / fade_ : 1.f;
        int16_t* f = memory_ + 2 * static_cast<size_t>(take_);
        f[0] = ToInt(SoftLimit(l * in)); f[1] = ToInt(SoftLimit(r * in));
        if(++take_ >= capacity_) CloseTake(false);     // full: plain playback (as TAPE)
    }
    void StartTake() {
        if(!memory_ || capacity_ < 2 * fade_) return;
        take_ = 0; length_ = 0; overdub_ = false; clearing_ = false; state_ = State::FirstTake;
    }
    void CloseTake(bool overdub) {
        if(take_ < 2 * fade_) { Reset(); return; }      // shorter than its fades: discard
        for(uint32_t k = 0; k < fade_; ++k) {           // fade the tail (bounded: 5 ms of frames)
            int16_t* f = memory_ + 2 * static_cast<size_t>(take_ - 1 - k);
            const float g = static_cast<float>(k) / fade_;
            f[0] = static_cast<int16_t>(f[0] * g); f[1] = static_cast<int16_t>(f[1] * g);
        }
        length_ = take_; index_ = 0; frac_ = 0.f; speed_ = speed_target_; gain_ = 1.f;
        overdub_ = overdub; state_ = State::Playing;
    }
    void PlayPress() {
        switch(state_) {
            case State::FirstTake: CloseTake(false); break;
            case State::Playing:
                if(overdub_) overdub_ = false;
                else state_ = State::Paused;
                break;
            default: break;                             // paused: resumes on release; empty/armed: nothing
        }
    }
    void LoopPress() {
        if(Locked()) return;                            // being saved: no overdub / new take
        switch(state_) {
            case State::Empty: StartTake(); break;
            case State::Armed: state_ = State::Empty; break;
            case State::FirstTake: CloseTake(true); break;
            case State::Playing: overdub_ = !overdub_; break;
            case State::Paused: Resume(true); break;
        }
    }
    void Resume(bool overdub) { if(length_ && !clearing_) { state_ = State::Playing; overdub_ = overdub; } }
    void Reset() {
        state_ = State::Empty; length_ = take_ = 0; index_ = 0; frac_ = 0.f;
        speed_ = 1.f; scrub_ = scrub_turns_ = 0.f; scrub_clock_ = 0; gain_ = 0.f;
        overdub_ = clearing_ = false;
    }

    int16_t* memory_ = nullptr;
    uint32_t capacity_ = 0, fade_ = 1, combo_ = 1, hold_ = 1, scrub_period_ = 1;
    State state_ = State::Empty;
    uint32_t length_ = 0, take_ = 0, index_ = 0;
    float frac_ = 0.f, speed_ = 1.f, speed_target_ = 1.f, gain_ = 0.f, gain_step_ = 0.f;
    float feedback_ = 1.f, feedback_target_ = 1.f, scrub_ = 0.f, scrub_turns_ = 0.f;
    uint32_t scrub_clock_ = 0, since_press_ = 0, held_ = 0;
    bool overdub_ = false, clearing_ = false, tape_slew_ = false;
    bool play_down_ = false, loop_down_ = false, pending_play_ = false, pending_loop_ = false;
    bool combo_used_ = false, jumped_ = false, was_paused_ = false;
    std::atomic<bool> locked_{false};
};

// MIDI CC 26 (PLAY) / 27 (LOOP), as TAPE: >= 85 press, <= 41 release, the
// middle third is a dead zone; repeated values do nothing.
struct CcButton {
    bool down = false;
    int Edge(uint8_t value) {                           // +1 press, -1 release, 0 nothing
        if(value >= 85 && !down) { down = true; return 1; }
        if(value <= 41 && down) { down = false; return -1; }
        return 0;
    }
};
} // namespace forge
