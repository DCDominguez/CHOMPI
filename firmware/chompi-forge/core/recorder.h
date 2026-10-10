#pragma once
#include <atomic>
#include <cmath>
#include <cstdint>
#include "parameters.h"
#include "sample_table.h"

namespace forge {
// Recording into the RAM slot (TAPE's "CHOMPI" slot 15). Audio owner only:
// Start/Write/Stop run in the audio callback. Stereo int16 at the output rate.
// 5 ms fade-in and fade-out remove edge clicks; the peak sets a playback gain
// (normalisation to -1 dBFS, at most +24 dB) that is also applied when the
// recording is saved. While a save is in progress (Lock) recording is refused.
enum class RecordSource : uint8_t { Mic, Line, Resample };
class Recorder {
public:
    void Init(int16_t* memory, uint32_t capacity_frames, SampleSlot* slot, float rate) {
        memory_ = memory; capacity_ = capacity_frames; slot_ = slot;
        fade_ = static_cast<uint32_t>(0.005f * rate); if(fade_ < 1) fade_ = 1;
        length_ = 0; recording_ = false; locked_.store(false, std::memory_order_relaxed); peak_ = 0;
        dc_l_ = dc_r_ = prev_l_ = prev_r_ = 0;
        dc_coefficient_ = 1.f - 6.2831853f * 20.f / rate;   // ~20 Hz DC blocker for the mic
        slot_->channels = 0; slot_->frames = 0; slot_->loaded.store(0, std::memory_order_release);
    }
    // Conditions the chosen input (TAPE: mic x5 with DC blocking, line x3,
    // resample = the instrument's own output) into a stereo pair.
    void Input(RecordSource source, float mic, float line_l, float line_r, float out_l, float out_r, float& l, float& r) {
        input_gain_ += .001f * (input_gain_target_ - input_gain_);
        switch(source) {
            case RecordSource::Mic: {
                const float x = 5.f * input_gain_ * mic;
                dc_l_ = x - prev_l_ + dc_coefficient_ * dc_l_; prev_l_ = x;
                l = r = dc_l_;
                break;
            }
            case RecordSource::Line: l = 3.f * input_gain_ * line_l; r = 3.f * input_gain_ * line_r; break;
            default: l = out_l; r = out_r; break;
        }
    }
    // TAPE's input gain (SW6 page 2, 0..1, .75 at power-on), smoothed per sample.
    void SetInputGain(float gain) { input_gain_target_ = Clamp(gain, 0.f, 1.f); }
    // Starts a new take (the old one is discarded). The caller first stops any
    // voice reading the slot; frames = 0 makes the slot unplayable meanwhile.
    bool Start() {
        if(locked_.load(std::memory_order_acquire) || !memory_ || !capacity_) return false;
        slot_->loaded.store(0, std::memory_order_release);
        slot_->frames = 0; slot_->channels = 0;
        length_ = 0; peak_ = 0; peak_abs_ = 0; recording_ = true;
        return true;
    }
    void Write(float l, float r) {
        if(!recording_) return;
        if(length_ >= capacity_) { Stop(); return; }
        const float in = length_ < fade_ ? static_cast<float>(length_) / fade_ : 1.f;
        int16_t* frame = memory_ + 2 * static_cast<size_t>(length_);
        frame[0] = ToInt16(l * in); frame[1] = ToInt16(r * in);
        const int a = frame[0] < 0 ? -frame[0] : frame[0], b = frame[1] < 0 ? -frame[1] : frame[1];
        if(a > peak_abs_) peak_abs_ = a;
        if(b > peak_abs_) peak_abs_ = b;
        ++length_;
    }
    // Ends the take: fade the tail (bounded: 5 ms of frames), publish the slot.
    void Stop() {
        if(!recording_) return;
        recording_ = false;
        const uint32_t tail = length_ < fade_ ? length_ : fade_;
        for(uint32_t k = 0; k < tail; ++k) {
            int16_t* frame = memory_ + 2 * static_cast<size_t>(length_ - 1 - k);
            const float g = static_cast<float>(k) / fade_;
            frame[0] = static_cast<int16_t>(frame[0] * g); frame[1] = static_cast<int16_t>(frame[1] * g);
        }
        const int peak = peak_abs_;
        peak_ = peak / 32768.f;
        slot_->data = memory_; slot_->channels = length_ ? 2 : 0; slot_->frames = length_;
        slot_->rate_ratio = 1.f; slot_->partial = false;
        slot_->gain = peak ? std::fmin(0.891f / peak_, 16.f) : 1.f;
        slot_->loaded.store(length_, std::memory_order_release);
    }
    bool Recording() const { return recording_; }
    uint32_t Length() const { return recording_ ? length_ : slot_ ? slot_->frames : 0; }
    uint32_t Capacity() const { return capacity_; }
    float Peak() const { return peak_; }
    float Gain() const { return slot_ ? slot_->gain : 1.f; }
    // Saving: the audio owner locks (refused while recording); the main loop
    // unlocks after writing the file. While locked, Start() fails.
    bool Lock() { if(recording_ || !Length()) return false; locked_.store(true, std::memory_order_release); return true; }
    void Unlock() { locked_.store(false, std::memory_order_release); }
    bool Locked() const { return locked_.load(std::memory_order_acquire); }
    const int16_t* Data() const { return memory_; }
private:
    float input_gain_ = .75f, input_gain_target_ = .75f;
    static int16_t ToInt16(float x) {
        const float v = x * 32767.f;
        return static_cast<int16_t>(v > 32767.f ? 32767.f : v < -32768.f ? -32768.f : (std::isfinite(v) ? v : 0.f));
    }
    int16_t* memory_ = nullptr;
    SampleSlot* slot_ = nullptr;
    uint32_t capacity_ = 0, length_ = 0, fade_ = 240;
    int peak_abs_ = 0;
    bool recording_ = false;
    std::atomic<bool> locked_{false};
    float peak_ = 0, dc_l_ = 0, dc_r_ = 0, prev_l_ = 0, prev_r_ = 0, dc_coefficient_ = 0.997f;
};
} // namespace forge
