#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include "parameters.h"
#include "sample_table.h"

namespace forge {
// Up to four voices (seven on v4). All calls belong to the audio owner. Source
// IDs separate keybed, UART and USB so one player's note-off cannot release
// another's note. Version 1/2 patches use the original path (one shared
// one-pole low-pass); version 3 adds a second oscillator, noise, a per-voice
// resonant filter with its own envelope, an LFO, a voice limit and glide.
// Neutral v3 settings add no arithmetic error to the v2 path. Version 4 with
// source = sampler plays samples from a SampleTable (stereo) through the same
// envelopes, filter, LFO and glide.
class Synth {
    enum class Stage : uint8_t { Off, Attack, Decay, Sustain, Release };
    struct Envelope {
        Stage stage = Stage::Off;
        float value = 0, release_step = 0;
    };
    struct Shape { float attack_step = 1, decay_step = 1, sustain = 0.6f, release_samples = 240; };
    struct Voice {
        Envelope amp, filter;
        uint8_t note = 0, source = 0;
        bool sustained = false; // key released while that source's pedal is down
        float phase = 0, phase2 = 0, increment = 0, target = 0, velocity = 0, gain = 0;
        float low = 0, band = 0, g = 0; // per-voice state-variable filter (v3)
        uint32_t age = 0;
        // v4 sampler voice: integer frame + fraction, right-channel filter state,
        // and a decaying tail of the previous sound when a voice is restarted.
        bool sampled = false, reverse = false;
        uint8_t slot = 0;
        int32_t pos = 0;
        float frac = 0, low_r = 0, band_r = 0, tail_l = 0, tail_r = 0, last_l = 0, last_r = 0;
    };
public:
    // Samples for v4 sampler patches (may be null: sampler notes are ignored).
    void SetSamples(const SampleTable* table) { table_ = table; }
    // False while the main loop rewrites file slots (SampleHandoff): only the recording plays.
    void SetSampleFilesAvailable(bool available) { files_available_ = available; }
    void Init(float rate) {
        rate_ = rate; voices_ = {}; age_ = 0; filter_ = 0; coefficient_ = 0; tick_ = 0;
        gain_slew_ = 1.f - std::exp(-1.f / (0.002f * rate)); // ~2 ms velocity glide on reuse
        bend_slew_ = 1.f - std::exp(-1.f / (0.005f * rate)); // ~5 ms bend smoothing, no zipper
        lfo_phase_ = 0; lfo_value_ = 0; held_ = 0; noise_state_ = 0x12345678u; last_target_ = 0;
        wheel_ = 0;
        ResetAllControllers();
        for(unsigned n = 0; n < 128; ++n)
            frequencies_[n] = 440.f * std::pow(2.f, (int(n) - 69) / 12.f) / rate;
    }
    FORGE_NOINLINE void Configure(const Parameters& p) {
        legacy_ = p.version < 3;
        waveform_ = p.waveform;
        amp_ = MakeShape(p.attack, p.decay, p.sustain, p.release);
        const float hz = 40.f * std::pow(400.f, p.cutoff);
        target_coefficient_ = 1.f - std::exp(-6.283185307f * Clamp(hz, 40.f, rate_ * 0.4f) / rate_);
        cutoff_target_ = p.cutoff;
        // v3 (all neutral for v1/v2 patches)
        osc2_waveform_ = p.osc2_waveform;
        osc2_level_ = legacy_ ? 0.f : p.osc2_level;
        osc2_ratio_ = std::pow(2.f, ((int(p.osc2_semitones) - 24) + (p.osc2_detune * 2.f - 1.f) * 0.5f) / 12.f);
        noise_ = legacy_ ? 0.f : p.noise;
        mix_scale_ = 1.f / (1.f + osc2_level_ + noise_);
        const float q = 0.70710678f * std::pow(16.f, p.resonance);          // Q 0.707..11.3
        damping_ = 1.f / q;
        resonance_gain_ = 1.f / std::sqrt(q / 0.70710678f);                 // tame the peak
        filter_octaves_ = (p.filter_amount * 2.f - 1.f) * 6.f;              // +/-6 octaves
        filter_shape_ = MakeShape(p.filter_attack, p.filter_decay, p.filter_sustain, p.filter_release);
        lfo_waveform_ = p.lfo_waveform; lfo_wheel_ = p.lfo_wheel;
        lfo_increment_ = 0.05f * std::pow(400.f, p.lfo_rate) / rate_;      // 0.05..20 Hz
        lfo_cents_ = legacy_ ? 0.f : 200.f * p.lfo_pitch;
        lfo_octaves_ = legacy_ ? 0.f : 4.f * p.lfo_filter;
        lfo_amp_ = legacy_ ? 0.f : p.lfo_amp;
        const float glide_s = legacy_ ? 0.f : 2.f * p.glide;
        glide_slew_ = glide_s > 0.f ? 1.f - std::exp(-1.f / (glide_s * 0.25f * rate_)) : 1.f; // ~98% there after glide time
        voices_used_ = legacy_ ? 4 : Clamp(p.voices, 1, p.MaxVoices());
        // v4 sampler
        sampler_ = p.Sampler();
        kit_ = p.sample_mode == 1; sample_slot_ = p.sample_slot;
        sample_gate_ = p.sample_gate; sample_loop_ = p.sample_loop; sample_reverse_ = p.sample_reverse;
        sample_start_ = p.sample_start; sample_end_ = p.sample_end;
        xfade_frames_ = p.sample_xfade * 0.25f * rate_;
        pitch_target_ = std::exp2((p.sample_pitch - 0.5f) * 4.f);       // +/-24 semitones
        for(unsigned i = voices_used_; i < voices_.size(); ++i)
            if(voices_[i].amp.stage != Stage::Off && voices_[i].amp.stage != Stage::Release) Release(voices_[i]);
    }
    // Kit mode: white keys C3..C5 (MIDI 48-72, the keybed's white keys) -> slots 0-14.
    static int KitSlot(uint8_t note) {
        static const int8_t degree[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
        if(note < 48 || note > 72 || degree[note % 12] < 0) return -1;
        return (note - 48) / 12 * 7 + degree[note % 12];
    }
    void Note(uint8_t note, uint8_t velocity, uint8_t source) {
        if(note > 127 || velocity > 127 || source >= kSources) return;
        if(!velocity) {
            for(auto& v : voices_) if(v.amp.stage != Stage::Off && v.amp.stage != Stage::Release && v.note == note && v.source == source) {
                if(v.sampled && !sample_gate_) continue;     // trigger mode ignores key-up
                if(pedal_[source]) v.sustained = true; else Release(v);
            }
            return;
        }
        if(sampler_) { SampleNote(note, velocity, source); return; }
        Voice* selected = nullptr;
        Voice* const end = voices_.data() + voices_used_;
        for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage != Stage::Off && v->note == note && v->source == source) { selected = v; break; }
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage == Stage::Off) { selected = v; break; }
        // Steal the quietest releasing voice, else the oldest sustained, else the oldest held.
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->amp.stage == Stage::Release && (!selected || v->amp.value < selected->amp.value)) selected = v;
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->sustained && (!selected || v->age < selected->age)) selected = v;
        if(!selected) {
            selected = voices_.data();
            for(Voice* v = voices_.data(); v < end; ++v) if(v->age < selected->age) selected = v;
        }
        // A reused voice keeps its level, phases, gain and filter state so the new
        // attack starts where the old sound was (click-free steal).
        const bool sounding = selected->amp.stage != Stage::Off;
        const Voice prior = *selected;
        *selected = Voice{};
        selected->note = note; selected->source = source; selected->velocity = velocity / 127.f;
        selected->gain = sounding ? prior.gain : selected->velocity;
        selected->target = Clamp(frequencies_[note], 0.f, 0.45f);
        // Glide starts from the sounding pitch, else from the last note played.
        const float from = sounding ? prior.increment : (last_target_ > 0.f ? last_target_ : selected->target);
        selected->increment = glide_slew_ < 1.f ? from : selected->target;
        last_target_ = selected->target;
        if(sounding) {
            selected->amp.value = prior.amp.value; selected->filter.value = prior.filter.value;
            selected->phase = prior.phase; selected->phase2 = prior.phase2;
            selected->low = prior.low; selected->band = prior.band; selected->g = prior.g;
        }
        selected->amp.stage = selected->filter.stage = Stage::Attack; selected->age = ++age_;
    }
    // Sustain pedal (CC64) per source: releasing it releases that source's
    // sustained voices; held keys keep sounding.
    void Pedal(uint8_t source, bool down) {
        if(source >= kSources) return;
        pedal_[source] = down;
        if(!down) for(auto& v : voices_) if(v.source == source && v.sustained) Release(v);
    }
    // 14-bit pitch bend per source, +/-2 semitones, 8192 = centre.
    void Bend(uint8_t source, uint16_t value) {
        if(source >= kSources || value > 16383) return;
        bend_target_[source] = std::pow(2.f, (static_cast<float>(value) - 8192.f) / 8192.f * (2.f / 12.f));
    }
    // Mod wheel (CC1, 0..127): scales LFO depth when the patch sets lfo_wheel.
    void ModWheel(uint8_t value) { if(value <= 127) wheel_ = value / 127.f; }
    void ResetControllers(uint8_t source) { Pedal(source, false); Bend(source, 8192); wheel_ = 0; }
    void Silence() { voices_ = {}; filter_ = 0; wheel_ = 0; ResetAllControllers(); }
    // Sample memory handoff: fade out (2 ms) voices reading file slots (and the
    // recording slot if asked), and report whether any still sound.
    void ReleaseSampleVoices(bool include_recording) {
        for(auto& v : voices_) if(v.sampled && v.amp.stage != Stage::Off && (include_recording || v.slot != kRamSlot)) {
            v.amp.stage = Stage::Release; v.amp.release_step = std::max(v.amp.value / (0.002f * rate_), 1e-6f);
        }
    }
    bool SampleVoicesActive(bool include_recording) const {
        for(const auto& v : voices_) if(v.sampled && v.amp.stage != Stage::Off && (include_recording || v.slot != kRamSlot)) return true;
        return false;
    }
    unsigned Active() const { unsigned n = 0; for(const auto& v : voices_) if(v.amp.stage != Stage::Off) ++n; return n; }
    // Mono patches (v1-v3, oscillators) return the same value on both sides.
    void Process(float& left, float& right) {
        if(sampler_) { ProcessSampler(left, right); return; }
        left = right = Process();
    }
    float Process() {
        float sum = 0;
        float pitch_ratio = 1.f, amp_lfo = 1.f, lfo_octaves = 0.f;
        Modulation(pitch_ratio, amp_lfo, lfo_octaves);
        // v3 filter coefficients are refreshed every 16 samples (one tan per voice).
        const bool refresh = !legacy_ && (tick_++ & 15u) == 0;
        if(!legacy_) cutoff_ += 0.002f * (cutoff_target_ - cutoff_);
        const float base_octave = kLog2Of40 + cutoff_ * kLog2Of400 + lfo_octaves;
        for(auto& v : voices_) {
            if(v.amp.stage == Stage::Off) continue;
            Step(v.amp, amp_);
            if(v.amp.stage == Stage::Off) continue;
            const float dt = std::min(v.increment * bend_[v.source] * pitch_ratio, 0.45f);
            float sample = Oscillator(waveform_, v.phase, dt);
            v.phase += dt; if(v.phase >= 1.f) v.phase -= 1.f;
            if(!legacy_) {
                if(v.increment != v.target) {
                    v.increment += glide_slew_ * (v.target - v.increment);
                    if(std::fabs(v.target - v.increment) < 1e-9f) v.increment = v.target;
                }
                if(osc2_level_ > 0.f) {
                    const float dt2 = std::min(dt * osc2_ratio_, 0.45f);
                    sample += osc2_level_ * Oscillator(osc2_waveform_, v.phase2, dt2);
                    v.phase2 += dt2; if(v.phase2 >= 1.f) v.phase2 -= 1.f;
                }
                if(noise_ > 0.f) sample += noise_ * Noise();
                sample *= mix_scale_;
                Step(v.filter, filter_shape_);
                if(refresh || v.g == 0.f) {
                    const float hz = Clamp(std::exp2(base_octave + filter_octaves_ * v.filter.value), 20.f, 0.45f * rate_);
                    v.g = std::tan(3.14159265f * hz / rate_);
                }
                // Topology-preserving SVF (Zavalishin), low-pass output.
                const float high = (sample - (damping_ + v.g) * v.band - v.low) / (1.f + v.g * (v.g + damping_));
                const float band = v.g * high + v.band;
                const float low = v.g * band + v.low;
                v.band = v.g * high + band; v.low = v.g * band + low;
                if(std::fabs(v.low) < 1e-20f) v.low = 0.f;
                if(std::fabs(v.band) < 1e-20f) v.band = 0.f;
                sample = low * resonance_gain_;
            }
            v.gain += gain_slew_ * (v.velocity - v.gain);
            sum += sample * v.amp.value * v.gain * 0.2f * amp_lfo;
        }
        if(!legacy_) return sum;
        coefficient_ += 0.002f * (target_coefficient_ - coefficient_);
        filter_ += coefficient_ * (sum - filter_);
        if(std::fabs(filter_) < 1e-20f) filter_ = 0;
        return filter_;
    }
private:
    static constexpr unsigned kSources = 3; // UART, USB, keybed
    static constexpr int32_t kMinLoop = 1024;   // frames (TAPE: 4096 at its 2x files)
    // Bend smoothing and the shared LFO. Depth scale 1, or the mod wheel when gated.
    void Modulation(float& pitch_ratio, float& amp_lfo, float& lfo_octaves) {
        for(unsigned s = 0; s < kSources; ++s) bend_[s] += bend_slew_ * (bend_target_[s] - bend_[s]);
        if(lfo_cents_ > 0.f || lfo_octaves_ > 0.f || lfo_amp_ > 0.f) {
            lfo_phase_ += lfo_increment_;
            if(lfo_phase_ >= 1.f) { lfo_phase_ -= 1.f; held_ = Noise(); }
            const float t = lfo_phase_;
            lfo_value_ = lfo_waveform_ == 0 ? std::sin(6.283185307f * t)
                : lfo_waveform_ == 1 ? 1.f - 4.f * std::fabs(t - 0.5f)
                : lfo_waveform_ == 2 ? (t < 0.5f ? 1.f : -1.f) : held_;
            const float depth = lfo_wheel_ ? wheel_ : 1.f;
            if(lfo_cents_ > 0.f) pitch_ratio = std::exp2(lfo_value_ * depth * lfo_cents_ / 1200.f);
            lfo_octaves = lfo_value_ * depth * lfo_octaves_;
            amp_lfo = 1.f - lfo_amp_ * depth * (0.5f - 0.5f * lfo_value_);
        }
    }
    // Topology-preserving SVF (Zavalishin), low-pass output.
    float Svf(float in, float g, float& low_state, float& band_state) const {
        const float high = (in - (damping_ + g) * band_state - low_state) / (1.f + g * (g + damping_));
        const float band = g * high + band_state;
        const float low = g * band + low_state;
        band_state = g * high + band; low_state = g * band + low;
        if(std::fabs(low_state) < 1e-20f) low_state = 0.f;
        if(std::fabs(band_state) < 1e-20f) band_state = 0.f;
        return low * resonance_gain_;
    }
    // Loop/one-shot window in frames, at least kMinLoop long where possible.
    void Window(const SampleSlot& s, int32_t& start, int32_t& end) const {
        const int32_t frames = static_cast<int32_t>(s.frames);
        start = static_cast<int32_t>(sample_start_ * frames); end = static_cast<int32_t>(sample_end_ * frames);
        if(end > frames) end = frames;
        if(end - start < kMinLoop) { end = std::min(frames, start + kMinLoop); start = std::max<int32_t>(0, end - kMinLoop); }
    }
    FORGE_NOINLINE void SampleNote(uint8_t note, uint8_t velocity, uint8_t source) {
        const int slot = kit_ ? KitSlot(note) : sample_slot_;
        if(slot < 0 || !table_ || (slot != kRamSlot && !files_available_)
           || !table_->slots[slot].channels || !table_->slots[slot].frames) return;
        const SampleSlot& s = table_->slots[slot];
        // Same rules as the oscillators: retrigger, idle, quietest releasing, oldest.
        Voice* selected = nullptr;
        Voice* const end = voices_.data() + voices_used_;
        for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage != Stage::Off && v->note == note && v->source == source) { selected = v; break; }
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v) if(v->amp.stage == Stage::Off) { selected = v; break; }
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->amp.stage == Stage::Release && (!selected || v->amp.value < selected->amp.value)) selected = v;
        if(!selected) for(Voice* v = voices_.data(); v < end; ++v)
            if(v->sustained && (!selected || v->age < selected->age)) selected = v;
        if(!selected) {
            selected = voices_.data();
            for(Voice* v = voices_.data(); v < end; ++v) if(v->age < selected->age) selected = v;
        }
        // A restarted voice keeps a ~1 ms decaying tail of what it was playing
        // (the sample restarts from its start point, so the level cannot carry over).
        const bool sounding = selected->amp.stage != Stage::Off;
        const Voice prior = *selected;
        *selected = Voice{};
        if(sounding) { selected->tail_l = prior.last_l + prior.tail_l; selected->tail_r = prior.last_r + prior.tail_r; }
        selected->sampled = true; selected->slot = static_cast<uint8_t>(slot); selected->reverse = sample_reverse_;
        selected->note = note; selected->source = source; selected->velocity = velocity / 127.f;
        selected->gain = selected->velocity;
        const float key = kit_ ? 1.f : std::exp2((int(note) - 60) / 12.f);
        selected->target = key * s.rate_ratio;
        const float from = kit_ || last_target_ <= 0.f ? selected->target : last_target_;
        selected->increment = glide_slew_ < 1.f && !kit_ ? from : selected->target;
        last_target_ = selected->target;
        int32_t start, stop; Window(s, start, stop);
        selected->pos = selected->reverse ? stop - 1 : start;
        selected->amp.stage = selected->filter.stage = Stage::Attack; selected->age = ++age_;
    }
    // 4-point Hermite read of frame `pos` + `frac`; frames not yet loaded read as silence.
    FORGE_NOINLINE static void Read(const SampleSlot& s, int32_t pos, float frac, uint32_t readable, float& l, float& r) {
        float x[2][4];
        for(int k = 0; k < 4; ++k) {
            int32_t i = pos - 1 + k;
            if(i < 0) i = 0;
            if(i >= static_cast<int32_t>(s.frames)) i = static_cast<int32_t>(s.frames) - 1;
            if(static_cast<uint32_t>(i) >= readable) { x[0][k] = x[1][k] = 0.f; continue; }
            const int16_t* frame = s.data + static_cast<size_t>(i) * s.channels;
            x[0][k] = frame[0];
            x[1][k] = s.channels == 2 ? frame[1] : frame[0];
        }
        float out[2];
        for(int c = 0; c < 2; ++c) {
            const float c1 = 0.5f * (x[c][2] - x[c][0]);
            const float c2 = x[c][0] - 2.5f * x[c][1] + 2.f * x[c][2] - 0.5f * x[c][3];
            const float c3 = 0.5f * (x[c][3] - x[c][0]) + 1.5f * (x[c][1] - x[c][2]);
            out[c] = ((c3 * frac + c2) * frac + c1) * frac + x[c][1];
        }
        const float scale = s.gain * (0.5f / 32768.f);   // 0.5: two full-scale voices reach 1.0
        l = out[0] * scale; r = out[1] * scale;
    }
    // One sample of a sampler voice; returns false when the voice has finished.
    bool SampleFrame(Voice& v, float step, float& l, float& r) {
        const SampleSlot& s = table_->slots[v.slot];
        if(!s.frames || !s.channels) return false;                  // slot emptied (new recording)
        const uint32_t readable = s.Readable();
        int32_t start, end; Window(s, start, end);
        const int32_t length = end - start;
        Read(s, v.pos, v.frac, readable, l, r);
        float gain = 1.f;
        const float fade = 0.002f * rate_;                          // 2 ms edges
        if(sample_loop_) {
            // Crossfade into the material before the loop start (after the end
            // when reversed); without room for that, dip at the boundary.
            const float room = static_cast<float>(v.reverse ? static_cast<int32_t>(s.frames) - end : start);
            const float span = std::min({xfade_frames_, room, 0.5f * length});
            const float at = static_cast<float>(v.pos) + v.frac;
            if(span >= 48.f) {
                const float into = v.reverse ? static_cast<float>(start) + span - at : at - (static_cast<float>(end) - span);
                if(into > 0.f) {
                    const float t = std::min(into / span, 1.f);
                    float ol, orr;
                    Read(s, v.reverse ? v.pos + length : v.pos - length, v.frac, readable, ol, orr);
                    l += t * (ol - l); r += t * (orr - r);
                }
            } else {
                gain = std::min({1.f, (at - static_cast<float>(start)) / fade, (static_cast<float>(end) - at) / fade});
            }
        } else {
            const float left_frames = v.reverse ? static_cast<float>(v.pos - start) + v.frac
                                                : static_cast<float>(end - v.pos) - v.frac;
            gain = std::min(1.f, left_frames / (step * fade));
        }
        gain = std::max(gain, 0.f);
        l *= gain; r *= gain;
        // Advance.
        if(!v.reverse) {
            v.frac += step;
            const int32_t whole = static_cast<int32_t>(v.frac);
            v.pos += whole; v.frac -= static_cast<float>(whole);
            if(v.pos < start) v.pos = start;                         // start moved past the playhead
            if(v.pos >= end) {
                if(!sample_loop_) return false;
                v.pos = start + (v.pos - start) % length;
            }
        } else {
            v.frac -= step;
            if(v.frac < 0.f) {
                const int32_t borrow = static_cast<int32_t>(-v.frac) + 1;
                v.pos -= borrow; v.frac += static_cast<float>(borrow);
            }
            if(v.pos >= end) v.pos = end - 1;                        // end moved below the playhead
            if(v.pos < start) {
                if(!sample_loop_) return false;
                v.pos = end - 1 - (start - 1 - v.pos) % length;
            }
        }
        return true;
    }
    FORGE_NOINLINE void ProcessSampler(float& left, float& right) {
        float pitch_ratio = 1.f, amp_lfo = 1.f, lfo_octaves = 0.f;
        Modulation(pitch_ratio, amp_lfo, lfo_octaves);
        pitch_ += 0.002f * (pitch_target_ - pitch_);
        const bool refresh = (tick_++ & 15u) == 0;
        cutoff_ += 0.002f * (cutoff_target_ - cutoff_);
        const float base_octave = kLog2Of40 + cutoff_ * kLog2Of400 + lfo_octaves;
        float sum_l = 0.f, sum_r = 0.f;
        for(auto& v : voices_) {
            if(v.tail_l != 0.f || v.tail_r != 0.f) {          // declick tail of a restarted voice
                sum_l += v.tail_l; sum_r += v.tail_r;
                v.tail_l *= 0.98f; v.tail_r *= 0.98f;
                if(std::fabs(v.tail_l) + std::fabs(v.tail_r) < 1e-6f) v.tail_l = v.tail_r = 0.f;
            }
            if(v.amp.stage == Stage::Off || !v.sampled || !table_) continue;
            Step(v.amp, amp_);
            if(!sample_gate_ && (v.amp.stage == Stage::Decay || v.amp.stage == Stage::Sustain)) Release(v); // TAPE trigger mode
            if(v.amp.stage == Stage::Off) { v.last_l = v.last_r = 0.f; continue; }
            if(v.increment != v.target) {
                v.increment += glide_slew_ * (v.target - v.increment);
                if(std::fabs(v.target - v.increment) < 1e-9f) v.increment = v.target;
            }
            const float step = std::min(v.increment * pitch_ * bend_[v.source] * pitch_ratio, 8.f);
            float l, r;
            if(!SampleFrame(v, step, l, r)) { v.amp = Envelope{}; v.last_l = v.last_r = 0.f; continue; }
            Step(v.filter, filter_shape_);
            if(refresh || v.g == 0.f) {
                const float hz = Clamp(std::exp2(base_octave + filter_octaves_ * v.filter.value), 20.f, 0.45f * rate_);
                v.g = std::tan(3.14159265f * hz / rate_);
            }
            l = Svf(l, v.g, v.low, v.band);
            r = Svf(r, v.g, v.low_r, v.band_r);
            v.gain += gain_slew_ * (v.velocity - v.gain);
            const float k = v.amp.value * v.gain * amp_lfo;
            v.last_l = l * k; v.last_r = r * k;
            sum_l += v.last_l; sum_r += v.last_r;
        }
        left = sum_l; right = sum_r;
    }
    static constexpr float kLog2Of40 = 5.321928f, kLog2Of400 = 8.643856f;
    static uint8_t Clamp(uint8_t v, uint8_t lo, uint8_t hi) { return v < lo ? lo : v > hi ? hi : v; }
    static float Clamp(float v, float lo, float hi) { return forge::Clamp(v, lo, hi); }
    Shape MakeShape(float a, float d, float s, float r) const {
        Shape shape;
        shape.attack_step = 1.f / (rate_ * (0.001f + 1.999f * a));
        shape.decay_step = 1.f / (rate_ * (0.001f + 1.999f * d));
        shape.sustain = s;
        shape.release_samples = rate_ * (0.005f + 4.995f * r);
        return shape;
    }
    // Off at zero ends a voice (amplitude) or rests there (filter envelope).
    static void Step(Envelope& e, const Shape& s) {
        switch(e.stage) {
            case Stage::Off: break;
            case Stage::Attack:
                e.value += s.attack_step;
                if(e.value >= 1) { e.value = 1; e.stage = Stage::Decay; } break;
            case Stage::Decay:
                e.value -= s.decay_step * (1.f - s.sustain);
                if(e.value <= s.sustain) { e.value = s.sustain; e.stage = Stage::Sustain; } break;
            case Stage::Sustain: e.value += 0.002f * (s.sustain - e.value); break;
            case Stage::Release:
                e.value -= e.release_step;
                if(e.value <= 0.000001f) { e.value = 0; e.stage = Stage::Off; } break;
        }
    }
    void Release(Voice& v) {
        v.sustained = false;
        v.amp.stage = Stage::Release; v.amp.release_step = v.amp.value / amp_.release_samples;
        if(v.filter.stage != Stage::Off) {
            v.filter.stage = Stage::Release; v.filter.release_step = v.filter.value / filter_shape_.release_samples;
        }
    }
    void ResetAllControllers() {
        for(unsigned s = 0; s < kSources; ++s) { pedal_[s] = false; bend_[s] = bend_target_[s] = 1.f; }
    }
    float Noise() { // xorshift32, uniform -1..1
        noise_state_ ^= noise_state_ << 13; noise_state_ ^= noise_state_ >> 17; noise_state_ ^= noise_state_ << 5;
        return static_cast<float>(static_cast<int32_t>(noise_state_)) * (1.f / 2147483648.f);
    }
    static float Oscillator(uint8_t waveform, float t, float dt) {
        if(waveform == 0) return std::sin(6.283185307f * t);
        if(waveform == 1) // polyBLAMP rounds both corners (slope change 8 per cycle)
            return 1.f - 4.f * std::fabs(t - 0.5f) + 4.f * dt * (Blamp(t, dt) - Blamp(t < 0.5f ? t + 0.5f : t - 0.5f, dt));
        if(waveform == 2) return 2.f * t - 1.f - Blep(t, dt);
        return (t < 0.5f ? 1.f : -1.f) + Blep(t, dt) - Blep(t < 0.5f ? t + 0.5f : t - 0.5f, dt);
    }
    // Integrated polyBLEP residual for a slope discontinuity, same scaling as Blep.
    static float Blamp(float t, float dt) {
        if(t < dt) { t = t / dt - 1.f; return -t * t * t / 3.f; }
        if(t > 1.f - dt) { t = (t - 1.f) / dt + 1.f; return t * t * t / 3.f; }
        return 0.f;
    }
    static float Blep(float t, float dt) {
        if(t < dt) { t /= dt; return t + t - t * t - 1.f; }
        if(t > 1.f - dt) { t = (t - 1.f) / dt; return t * t + t + t + 1.f; }
        return 0.f;
    }
    std::array<Voice, 7> voices_{};
    std::array<float, 128> frequencies_{};
    Shape amp_, filter_shape_;
    float rate_ = 48000, filter_ = 0, coefficient_ = 0, target_coefficient_ = 0, gain_slew_ = 1;
    std::array<bool, kSources> pedal_{};
    std::array<float, kSources> bend_{}, bend_target_{};
    float bend_slew_ = 1, wheel_ = 0;
    // v3
    bool legacy_ = true, lfo_wheel_ = false;
    uint8_t waveform_ = 0, osc2_waveform_ = 0, lfo_waveform_ = 0, voices_used_ = 4;
    float osc2_level_ = 0, osc2_ratio_ = 1, noise_ = 0, mix_scale_ = 1;
    float damping_ = 1.41421356f, resonance_gain_ = 1, filter_octaves_ = 0, cutoff_ = 0.5f, cutoff_target_ = 0.5f;
    float lfo_phase_ = 0, lfo_value_ = 0, lfo_increment_ = 0, lfo_cents_ = 0, lfo_octaves_ = 0, lfo_amp_ = 0, held_ = 0;
    float glide_slew_ = 1, last_target_ = 0;
    // v4 sampler
    const SampleTable* table_ = nullptr;
    bool files_available_ = true, sampler_ = false, kit_ = false, sample_gate_ = true, sample_loop_ = false, sample_reverse_ = false;
    uint8_t sample_slot_ = 0;
    float sample_start_ = 0, sample_end_ = 1, xfade_frames_ = 0, pitch_ = 1, pitch_target_ = 1;
    uint32_t noise_state_ = 0x12345678u, age_ = 0, tick_ = 0;
};
} // namespace forge
