#pragma once
#include <cstdint>
#include "harmony.h"

// Harmony's secret modes (0.19.2, the owner's ideas): Chord Bloom, Chord Conversations, and the
// voicing math for Harmony Mischief (the bend itself lives in harmony::Player). Chosen on the harmony
// page by turning SW3; not saved in presets (back to off at power-on). Timing follows the tempo:
// Bloom adds a note every 1/16, Conversations answer one bar after the chord is let go. Fixed arrays,
// integer frame counts, no allocation; audio owner only (key events and Engine::Block). Original
// Forge code.
namespace forge {
namespace secret {

// SW3 on the harmony page steps through these (3 bits on the light flags: keep it at 8 or fewer).
enum class Mode : uint8_t { Off, BloomUp, BloomDown, BloomRandom, TalkEcho, TalkCall, TalkSparkle, TalkGroove };
constexpr unsigned kModes = 8;
inline bool Blooms(Mode m) { return m == Mode::BloomUp || m == Mode::BloomDown || m == Mode::BloomRandom; }
inline bool Talks(Mode m) { return m >= Mode::TalkEcho && m <= Mode::TalkGroove; }
// Conversations' motif families (the owner, 2026-10-12: groups of motifs that belong together).
enum class Family : uint8_t { Echo, Call, Sparkle, Groove };
constexpr unsigned kFamilies = 4, kMotifs = 3;
inline Family FamilyOf(Mode m) { return static_cast<Family>(Talks(m) ? static_cast<unsigned>(m) - static_cast<unsigned>(Mode::TalkEcho) : 0); }

struct Rand {                                   // xorshift32, as magic::Rng
    uint32_t s = 0x2545F491u;
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    unsigned Pick(unsigned n) { return n ? Next() % n : 0; }
};

// One 1/16 at `bpm` in frames (bpm 40-300; anything else counts as 120).
inline uint32_t Sixteenth(float sample_rate, float bpm) {
    if(!(bpm >= 40.f && bpm <= 300.f)) bpm = 120.f;
    return static_cast<uint32_t>(sample_rate * 15.f / bpm);
}

// Chord Bloom: a chord's notes arrive one at a time and stay, so it grows into the full chord (unlike
// an arpeggio, nothing stops until the key is let go). The first note sounds at once; the rest are
// queued `step` frames apart in the chosen order.
class Bloom {
public:
    static constexpr unsigned kQueue = harmony::kMaxNotes * 4;   // four chords growing at once
    // `notes` (n of them) start now or later; returns how many of them go into `now` (always 1 if n).
    FORGE_COLD unsigned Schedule(const uint8_t* notes, unsigned n, uint8_t velocity, uint8_t source, Mode mode, uint32_t step, Rand& r, uint8_t* now) {
        if(!n) return 0;
        uint8_t order[harmony::kMaxNotes * 2]; if(n > sizeof(order)) n = sizeof(order);
        for(unsigned i = 0; i < n; ++i) order[i] = notes[i];
        for(unsigned i = 1; i < n; ++i) for(unsigned j = i; j > 0 && order[j - 1] > order[j]; --j) { const uint8_t t = order[j]; order[j] = order[j - 1]; order[j - 1] = t; }
        if(mode == Mode::BloomDown) for(unsigned i = 0; i < n / 2; ++i) { const uint8_t t = order[i]; order[i] = order[n - 1 - i]; order[n - 1 - i] = t; }
        else if(mode == Mode::BloomRandom) for(unsigned i = n - 1; i > 0; --i) { const unsigned j = r.Pick(i + 1); const uint8_t t = order[i]; order[i] = order[j]; order[j] = t; }
        now[0] = order[0];
        for(unsigned i = 1; i < n; ++i) Queue(order[i], velocity, source, step * i);
        return 1;
    }
    // Key up (or a retrigger): notes of that chord not yet sounded never will.
    FORGE_COLD void Cancel(const uint8_t* notes, unsigned n, uint8_t source) {
        for(auto& p : queue_) if(p.used && p.source == source) for(unsigned i = 0; i < n; ++i) if(p.note == notes[i]) p.used = false;
    }
    template<typename F> void Advance(uint32_t frames, F play) {
        for(auto& p : queue_) if(p.used) {
            if(p.wait > frames) { p.wait -= frames; continue; }
            p.used = false; play(p.note, p.velocity, p.source);
        }
    }
    FORGE_COLD void Clear() { for(auto& p : queue_) p.used = false; }
    unsigned Pending() const { unsigned n = 0; for(const auto& p : queue_) n += p.used; return n; }
private:
    struct Slot { bool used = false; uint8_t note = 0, velocity = 0, source = 0; uint32_t wait = 0; };
    void Queue(uint8_t note, uint8_t velocity, uint8_t source, uint32_t wait) {
        for(auto& p : queue_) if(!p.used) { p = Slot{true, note, velocity, source, wait}; return; }   // full: that note just never blooms
    }
    Slot queue_[kQueue] = {};
};

// Chord Conversations: a chord is let go, a bar passes in silence, and Choowi answers with a short
// motif from the chosen family, built on that chord's tones in the key. Each family has three related
// motifs; the same one never plays twice in a row, except that about one answer in three develops the
// last motif on the new chord, so a run of answers hangs together. One note at a time (one voice);
// any key cuts it off.
//
// A motif step: `tone` picks a chord tone (0 = lowest, -1 = highest, -2 = the one below...; kRest = a
// rest), `octave` moves it by octaves, `scale` then moves it by steps of the key (neighbour and
// passing notes), `len` is in 1/16 notes at the tempo, `vel` the velocity.
struct MotifStep { int8_t tone, octave, scale; uint8_t len, vel; };
constexpr int8_t kRest = 99;
constexpr unsigned kMotifSteps = 6;
struct Motif { uint8_t count; MotifStep step[kMotifSteps]; };
// [family][motif]. Echo: the top notes come back, softer each time. Call: little melodies (a rising
// arpeggio to the octave, a falling run landing on the third, a turn around the third). Sparkle: high
// and quick (a grace note, a trill, broken octaves). Groove: low and rhythmic, with rests.
constexpr Motif kMotifTable[kFamilies][kMotifs] = {
    {{3, {{-1, 0, 0, 2, 80}, {-1, 0, 0, 2, 62}, {-1, 0, 0, 4, 46}}},
     {3, {{-1, 0, 0, 2, 80}, {-2, 0, 0, 2, 64}, {-1, 0, 0, 4, 48}}},
     {4, {{-1, 0, 0, 1, 82}, {-2, 0, 0, 1, 70}, {-3, 0, 0, 1, 60}, {-4, 0, 0, 4, 50}}}},
    {{4, {{0, 0, 0, 2, 84}, {1, 0, 0, 2, 84}, {2, 0, 0, 2, 86}, {0, 1, 0, 4, 72}}},
     {5, {{-1, 0, 0, 1, 84}, {-1, 0, -1, 1, 80}, {-1, 0, -2, 1, 78}, {-1, 0, -3, 1, 76}, {1, 0, 0, 4, 68}}},
     {5, {{1, 0, 0, 1, 82}, {1, 0, 1, 1, 78}, {1, 0, 0, 1, 80}, {1, 0, -1, 1, 76}, {0, 0, 0, 4, 66}}}},
    {{3, {{-1, 1, 1, 1, 64}, {-1, 1, 0, 3, 84}, {0, 2, 0, 4, 70}}},
     {5, {{-1, 1, 0, 1, 82}, {-1, 1, 1, 1, 74}, {-1, 1, 0, 1, 80}, {-1, 1, 1, 1, 72}, {-1, 1, 0, 4, 66}}},
     {5, {{0, 1, 0, 1, 80}, {0, 2, 0, 1, 72}, {2, 1, 0, 1, 80}, {2, 2, 0, 1, 72}, {1, 2, 0, 4, 64}}}},
    {{5, {{0, 0, 0, 1, 90}, {kRest, 0, 0, 1, 0}, {0, 0, 0, 1, 78}, {2, 0, 0, 2, 86}, {0, 1, 0, 3, 74}}},
     {5, {{0, -1, 0, 2, 92}, {2, -1, 0, 1, 80}, {0, 0, 0, 1, 84}, {kRest, 0, 0, 2, 0}, {0, -1, 0, 2, 76}}},
     {5, {{kRest, 0, 0, 1, 0}, {2, 0, 0, 2, 88}, {2, 0, 0, 1, 74}, {1, 0, 0, 2, 82}, {0, 0, 0, 4, 70}}}}};
class Conversation {
public:
    static constexpr unsigned kMaxPhrase = kMotifSteps;
    // The chord just ended: remember it and start the pause.
    FORGE_COLD void ChordEnded(const harmony::Voiced& v, const harmony::State& key, Family family, uint32_t sixteenth, Rand& r) {
        if(!v.count) return;
        Stop();
        Build(v, key, family, r);
        sixteenth_ = sixteenth ? sixteenth : 1;
        wait_ = 16 * sixteenth_;                      // one bar
        armed_ = count_ > 0;
    }
    // A key: no answer now, and a sounding answer note stops (returned through `stop`).
    template<typename F> void Interrupt(F stop) { if(sounding_) stop(sounding_); sounding_ = 0; armed_ = false; at_ = count_; }
    // `play(note, velocity)`: velocity 0 ends a note. A rest (note 0) sounds nothing for its length.
    template<typename F> void Advance(uint32_t frames, F play) {
        while(armed_ && frames) {
            if(wait_ > frames) { wait_ -= frames; return; }
            frames -= wait_;
            if(sounding_) { play(sounding_, 0); sounding_ = 0; }
            if(at_ >= count_) { armed_ = false; return; }
            if(phrase_[at_]) { sounding_ = phrase_[at_]; play(sounding_, velocity_[at_]); }
            wait_ = length_[at_] * sixteenth_; ++at_;
        }
    }
    void Stop() { armed_ = false; at_ = count_ = 0; }
    bool Speaking() const { return armed_; }
    unsigned Length() const { return count_; }
    uint8_t Note(unsigned i) const { return i < count_ ? phrase_[i] : 0; }   // 0 = a rest
    uint8_t Steps(unsigned i) const { return i < count_ ? length_[i] : 0; }
    uint8_t Velocity(unsigned i) const { return i < count_ ? velocity_[i] : 0; }
    uint8_t Sounding() const { return sounding_; }
    unsigned LastMotif() const { return last_motif_; }
private:
    FORGE_COLD void Build(const harmony::Voiced& v, const harmony::State& key, Family family, Rand& r) {
        uint8_t tones[harmony::kMaxNotes]; unsigned n = 0;
        for(unsigned i = 0; i < v.count && n < harmony::kMaxNotes; ++i) tones[n++] = v.note[i];
        for(unsigned i = 1; i < n; ++i) for(unsigned j = i; j > 0 && tones[j - 1] > tones[j]; --j) { const uint8_t t = tones[j]; tones[j] = tones[j - 1]; tones[j - 1] = t; }
        const unsigned f = static_cast<unsigned>(family) % kFamilies;
        unsigned motif;
        if(last_family_ == f && r.Pick(3) == 0) motif = last_motif_;                          // develop the last idea
        else if(last_family_ == f) motif = (last_motif_ + 1 + r.Pick(kMotifs - 1)) % kMotifs;   // a different one
        else motif = r.Pick(kMotifs);
        last_family_ = f; last_motif_ = motif;
        const int lift = tones[0] < 60 ? 12 : 0;                                               // answer above a low chord
        const Motif& m = kMotifTable[f][motif];
        count_ = 0; at_ = 0;
        for(unsigned i = 0; i < m.count && count_ < kMaxPhrase; ++i) {
            const MotifStep& st = m.step[i];
            phrase_[count_] = 0; length_[count_] = st.len; velocity_[count_] = st.vel;
            if(st.tone != kRest) {
                const int last = static_cast<int>(n) - 1;
                const int index = st.tone >= 0 ? (st.tone < last ? st.tone : last) : (last + 1 + st.tone >= 0 ? last + 1 + st.tone : 0);
                int note = ScaleStep(tones[index] + lift + 12 * st.octave, st.scale, key);
                while(note > harmony::kHighest) note -= 12;
                while(note < harmony::kLowest) note += 12;
                phrase_[count_] = static_cast<uint8_t>(note);
            }
            ++count_;
        }
    }
    // `steps` notes of the key up (+) or down (-) from `note`.
    static int ScaleStep(int note, int steps, const harmony::State& key) {
        const int dir = steps > 0 ? 1 : -1;
        for(int left = steps > 0 ? steps : -steps; left > 0; --left) {
            int guard = 0;
            do { note += dir; } while(!harmony::InKey(static_cast<unsigned>((note % 12 + 12) % 12), key) && ++guard < 12);
        }
        return note;
    }
    uint8_t phrase_[kMaxPhrase] = {}, length_[kMaxPhrase] = {}, velocity_[kMaxPhrase] = {};
    unsigned count_ = 0, at_ = 0, last_family_ = kFamilies, last_motif_ = 0;
    uint32_t wait_ = 0, sixteenth_ = 1;
    uint8_t sounding_ = 0;
    bool armed_ = false;
};

// Harmony Mischief: a related chord for the one held. Intervals above the root (0-11), the voiced
// notes and the root's pitch class in, the bent notes out (same count). Variants: 0 sus4, 1 sus2,
// 2 borrowed (third flips major / minor), 3 next inversion (lowest note up an octave), 4 chromatic
// neighbour (the whole chord a semitone up: a passing chord that leans back home).
constexpr unsigned kMischiefs = 5;
FORGE_COLD inline void Bend(const uint8_t* in, unsigned n, uint8_t root_pc, unsigned variant, uint8_t* out) {
    unsigned lowest = 0;
    for(unsigned i = 0; i < n; ++i) { out[i] = in[i]; if(in[i] < in[lowest]) lowest = i; }
    for(unsigned i = 0; i < n; ++i) {
        const int interval = ((in[i] - root_pc) % 12 + 12) % 12;
        int note = in[i];
        if(variant == 0 && (interval == 3 || interval == 4)) note += 5 - interval;
        else if(variant == 1 && (interval == 3 || interval == 4)) note -= interval - 2;
        else if(variant == 2 && (interval == 3 || interval == 4)) note += interval == 3 ? 1 : -1;
        else if(variant == 3 && i == lowest && note + 12 <= harmony::kHighest) note += 12;
        else if(variant == 4 && note + 1 <= harmony::kHighest) note += 1;
        out[i] = static_cast<uint8_t>(note);
    }
}

} // namespace secret
} // namespace forge
