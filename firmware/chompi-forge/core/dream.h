#pragma once
#include <cstdint>
#include "harmony.h"
#include "magic.h"
#include "parameters.h"

// Dream mode, toggled (0.19.2, the owner's redesign): hold SW4 + SW1 and CHOMPI plays an ambient
// piece on its own, an auto drone to listen to and watch, until the same gesture, a key or panic
// stops it. What it plays follows the sound: long, pad-like sounds get Drone (slow overlapping
// chords with a high glint now and then), short plucky or keys sounds get Shimmer (drifting
// clusters of chord tones and breaths), a drum kit gets Patter (sparse, soft hits). Chords follow
// harmony's key and mode through one of a few gentle progressions. The idle player (magic::Dream,
// 2 minutes untouched) is now "nap". Fixed arrays, integer frames at 48 kHz; audio owner only.
namespace forge {
namespace dream {

enum class Style : uint8_t { Drone, Shimmer, Patter };
// Pad-like: slow attack or long release with sustain, or a looping sample. A kit: Patter.
inline Style StyleFor(const Parameters& p) {
    if(p.Sampler() && p.sample_mode == 1) return Style::Patter;
    const bool long_notes = (p.sustain >= .5f && (p.attack >= .15f || p.release >= .3f)) || (p.Sampler() && p.sample_loop);
    return long_notes ? Style::Drone : Style::Shimmer;
}

class Show {
public:
    static constexpr unsigned kHeld = 6, kProgressions = 4, kChords = 4;
    static constexpr uint32_t kMs = 48;
    bool Active() const { return active_; }
    Style GetStyle() const { return style_; }
    // The chord being played, as a colour step (0-6 = its degree), for the lights.
    uint8_t Colour() const { return degree_; }
    // Notes sounding now, one bit per key C3..C5 (folded into that range).
    uint32_t KeyMask() const {
        uint32_t m = 0;
        for(const Held& h : held_) if(h.left) { int n = h.note; while(n > 72) n -= 12; while(n < 48) n += 12; m |= 1u << (n - 48); }
        return m;
    }
    FORGE_COLD void Start(const Parameters& p, magic::Rng& r) {
        active_ = true; style_ = StyleFor(p);
        max_notes_ = static_cast<uint8_t>(p.voices < 2 ? 1 : p.voices > kHeld ? kHeld : p.voices);
        progression_ = static_cast<uint8_t>(r.Pick(kProgressions)); step_ = 0; chord_left_ = 0; next_ = 0; cluster_ = 0; glint_ = 4000 * kMs;
    }
    template<typename F> FORGE_COLD void Stop(F note) {
        for(Held& h : held_) if(h.left) { note(h.note, 0); h.left = 0; }
        active_ = false;
    }
    template<typename F> FORGE_COLD void Tick(uint32_t frames, const harmony::State& key, magic::Rng& r, F note) {
        if(!active_) return;
        for(Held& h : held_) if(h.left) { if(h.left > frames) h.left -= frames; else { h.left = 0; note(h.note, 0); } }
        if(chord_left_ > frames) chord_left_ -= frames; else NextChord(key, r, note);
        if(style_ == Style::Drone) {
            if(glint_ > frames) glint_ -= frames;
            else {                                                    // a soft high chord tone
                glint_ = (3000 + r.Pick(3000)) * kMs;
                if(max_notes_ >= 4) Play(static_cast<uint8_t>(chord_[r.Pick(count_)] + 24), 38 + static_cast<uint8_t>(r.Pick(12)), 2200 * kMs, note);
            }
            return;
        }
        if(next_ > frames) { next_ -= frames; return; }
        if(style_ == Style::Patter) {                                 // kit: a soft hit on a white key, sometimes two
            static const uint8_t white[14] = {48, 50, 52, 53, 55, 57, 59, 60, 62, 64, 65, 67, 69, 71};
            Play(white[r.Pick(14)], static_cast<uint8_t>(40 + r.Pick(36)), 200 * kMs, note);
            next_ = r.Chance(.25f) ? 140 * kMs : (400 + r.Pick(1600)) * kMs;
            return;
        }
        // Shimmer: a cluster of 3-5 chord tones walking across two octaves, then a breath.
        if(!cluster_) cluster_ = static_cast<uint8_t>(3 + r.Pick(3));
        static const int8_t moves[5] = {-2, -1, 1, 1, 2};
        walk_ = static_cast<int8_t>(walk_ + moves[r.Pick(5)]);
        if(walk_ < 0) walk_ = 1;
        if(walk_ > 2 * static_cast<int>(count_) - 1) walk_ = static_cast<int8_t>(2 * count_ - 2);
        const uint8_t pitch = static_cast<uint8_t>(chord_[walk_ % count_] + 12 * (1 + walk_ / count_));
        Play(pitch, static_cast<uint8_t>(45 + r.Pick(26)), 1500 * kMs, note);
        next_ = --cluster_ ? (250 + r.Pick(350)) * kMs : (1500 + r.Pick(1500)) * kMs;
    }
private:
    struct Held { uint8_t note = 0; uint32_t left = 0; };
    template<typename F> FORGE_COLD void Play(uint8_t n, uint8_t velocity, uint32_t length, F note) {
        unsigned sounding = 0; Held* slot = nullptr; Held* soonest = nullptr;
        for(Held& h : held_) { if(h.left) { ++sounding; if(!soonest || h.left < soonest->left) soonest = &h; } else if(!slot) slot = &h; }
        if((!slot || sounding >= max_notes_) && soonest) { slot = soonest; note(slot->note, 0); }   // within the patch's voices
        if(!slot) return;
        slot->note = n; slot->left = length; note(n, velocity);
    }
    // Degrees (0 = I) of the progressions; each chord is the key's own (stacked thirds in the mode).
    template<typename F> FORGE_COLD void NextChord(const harmony::State& key, magic::Rng& r, F note) {
        static const uint8_t progressions[kProgressions][kChords] = {{0, 5, 3, 4}, {0, 3, 0, 4}, {5, 3, 0, 4}, {0, 4, 5, 3}};
        degree_ = progressions[progression_][step_++ % kChords];
        harmony::Function f; f.kind = harmony::Kind::Diatonic; f.degree = degree_;
        const harmony::Chord c = harmony::Resolve(f, key.tonic, key.mode, false);
        uint8_t tones[harmony::kMaxNotes];
        const uint8_t n = harmony::Tones(c, harmony::Extension::Seventh, harmony::kMaxNotes, tones);
        count_ = 0;
        for(uint8_t i = 0; i < n && count_ < 4; ++i) chord_[count_++] = static_cast<uint8_t>(48 + c.root + tones[i]);
        if(!count_) { chord_[0] = static_cast<uint8_t>(48 + key.tonic % 12); count_ = 1; }
        if(style_ == Style::Drone) {
            chord_left_ = (8000 + r.Pick(6000)) * kMs;
            for(Held& h : held_) if(h.left > 2000 * kMs) h.left = 2000 * kMs;   // the old chord fades under the new one
            const uint8_t root = chord_[0], fifth = count_ > 2 ? chord_[2] : chord_[0], third = count_ > 1 ? chord_[1] : chord_[0];
            const uint32_t hold = chord_left_ + 2000 * kMs;
            Play(root, 62, hold, note);
            if(max_notes_ >= 2) Play(fifth, 56, hold, note);
            if(max_notes_ >= 3) Play(static_cast<uint8_t>(third + 12), 52, hold, note);
        } else chord_left_ = 8000 * kMs;
    }
    Held held_[kHeld] = {};
    Style style_ = Style::Drone;
    uint8_t chord_[4] = {}, count_ = 0, degree_ = 0, progression_ = 0, max_notes_ = 4, cluster_ = 0;
    int8_t walk_ = 2;
    uint32_t step_ = 0, chord_left_ = 0, next_ = 0, glint_ = 0;
    bool active_ = false;
};

} // namespace dream
} // namespace forge
