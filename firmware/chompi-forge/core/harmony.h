#pragma once
#include <cstdint>
#include "parameters.h"

// Harmony mode (docs/forge/HARMONY_BRIEF.md, Phase 1, firmware 0.13). A key resolves
// to a musical function, the function to a chord built from the mode's own scale by
// stacked thirds, the chord to voiced MIDI notes (inversion, spread, or the voicing
// nearest the previous chord). Output is ordinary notes for the synth/sampler and
// MIDI out; nothing here touches audio. Integer only, fixed arrays, no allocation;
// it runs on key-down/up only (audio owner, between samples). Original Forge code
// from general music theory: no third-party source.
namespace forge {
namespace harmony {

enum class Mode : uint8_t { Major, NaturalMinor, HarmonicMinor, MelodicMinor, Dorian, Phrygian, Lydian, Mixolydian, Locrian };
constexpr unsigned kModes = 9;
// Semitones of scale degrees 1-7 above the tonic.
constexpr uint8_t kScale[kModes][7] = {
    {0, 2, 4, 5, 7, 9, 11}, {0, 2, 3, 5, 7, 8, 10}, {0, 2, 3, 5, 7, 8, 11}, {0, 2, 3, 5, 7, 9, 11},
    {0, 2, 3, 5, 7, 9, 10}, {0, 1, 3, 5, 7, 8, 10}, {0, 2, 4, 6, 7, 9, 11}, {0, 2, 4, 5, 7, 9, 10},
    {0, 1, 3, 5, 6, 8, 10}};
// Zero is the default everywhere (State is zero-initialised in .bss): Triad first.
enum class Extension : uint8_t { Triad, Seventh, Ninth, Eleventh, Thirteenth, Fifth };
constexpr unsigned kExtensions = 6;
enum class Layout : uint8_t { Static, Real };
constexpr unsigned kMaxNotes = 5;          // per chord: fits CHOMPI's voices with room for a 9th
constexpr uint8_t kLowest = 36, kHighest = 96;   // where voiced notes may go (C2..C7)

struct State {
    bool enabled = false;
    uint8_t tonic = 0;                     // pitch class, 0 = C
    Mode mode = Mode::Major;
    Extension extension = Extension::Triad;
    uint8_t inversion = 0;                 // 0-3, used when voice leading is off
    bool open = false;                     // spread: the second voice up an octave
    bool block = false;                    // voice leading off (block chords as set)
    Layout layout = Layout::Static;
    uint8_t max_notes = 0;                 // 0 = kMaxNotes; else fewer (the patch's voices)
};

// What a key means. Kind: Diatonic (degree 0-6 of the mode), Interchange (degree 0-6
// of the parallel mode: major keys borrow from natural minor, minor keys from major),
// Secondary (V7 of a degree), Borrowed (a major chord on a chromatic root, e.g. bII);
// Shift is applied on top.
enum class Kind : uint8_t { None, Diatonic, Secondary, Borrowed, ShiftKey, Interchange };
// The parallel mode modal interchange borrows from.
inline Mode Parallel(Mode m) { return kScale[static_cast<unsigned>(m) % kModes][2] == 4 ? Mode::NaturalMinor : Mode::Major; }
enum class Quality : uint8_t { Scale, Major, Minor, Dominant, Sus4 };
struct Function {
    Kind kind = Kind::None;
    uint8_t degree = 0;                    // Diatonic: the degree; Secondary: its target degree
    uint8_t root = 0;                      // semitones above the tonic (Borrowed, Secondary)
    Quality quality = Quality::Scale;
    uint8_t octave = 0;                    // 0 lower register, 1 upper (Static); Real: unused
};
// A chord's identity: root pitch class and a stack of seven tones (root, 3rd, 5th, 7th,
// 9th, 11th, 13th) in semitones above the root.
struct Chord { uint8_t root = 0; uint8_t tone[7] = {}; uint8_t degree = 0; Kind kind = Kind::None; Quality quality = Quality::Scale; bool shifted = false; };
struct Voiced { uint8_t count = 0; uint8_t note[kMaxNotes] = {}; };

// Static layout: the 25 keys C3..C5 (MIDI 48..72). White C3..B4 = degrees I..vii twice,
// C5 = Shift; black keys = chromatic functions (lower octave secondary dominants and
// bVII; upper octave borrowed chords and V/IV).
constexpr uint8_t kShiftKey = 72;
inline Function StaticFunction(uint8_t note) {
    Function f;
    if(note < 48 || note > 72) return f;
    if(note == kShiftKey) { f.kind = Kind::ShiftKey; return f; }
    static const int8_t white[12] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6};
    static const int8_t black[12] = {-1, 0, -1, 1, -1, -1, 2, -1, 3, -1, 4, -1};
    const unsigned pc = note % 12, octave = (note - 48) / 12;
    f.octave = static_cast<uint8_t>(octave);
    if(white[pc] >= 0) { f.kind = Kind::Diatonic; f.degree = static_cast<uint8_t>(white[pc]); return f; }
    // Lower: V/ii V/iii V/V V/vi, parallel vii (in major: bVII). Upper: parallel iv,
    // iii, vi (in major: iv bIII bVI), V/IV, bII (Neapolitan).
    static const uint8_t lower_target[4] = {1, 2, 4, 5}, upper_degree[3] = {3, 2, 5};
    const unsigned b = static_cast<unsigned>(black[pc]);
    if(octave == 0 && b < 4) { f.kind = Kind::Secondary; f.degree = lower_target[b]; f.quality = Quality::Dominant; }
    else if(octave == 0) { f.kind = Kind::Interchange; f.degree = 6; }
    else if(b < 3) { f.kind = Kind::Interchange; f.degree = upper_degree[b]; }
    else if(b == 3) { f.kind = Kind::Secondary; f.degree = 3; f.quality = Quality::Dominant; }
    else { f.kind = Kind::Borrowed; f.root = 1; f.quality = Quality::Major; }
    return f;
}
// Real layout: the key is the root. In the scale: that degree's chord. Outside it: the
// parallel mode's chord on that root (modal interchange); else a dominant if it falls
// a fifth to a degree with a perfect fifth (V7/x); else a major chord (e.g. bII).
FORGE_COLD inline Function RealFunction(uint8_t note, uint8_t tonic, Mode mode) {
    Function f;
    if(note > 127) return f;
    const uint8_t* scale = kScale[static_cast<unsigned>(mode) % kModes];
    const unsigned pc = (note + 12 - tonic % 12) % 12;
    for(unsigned d = 0; d < 7; ++d) if(scale[d] == pc) { f.kind = Kind::Diatonic; f.degree = static_cast<uint8_t>(d); return f; }
    const uint8_t* parallel = kScale[static_cast<unsigned>(Parallel(mode))];
    for(unsigned d = 0; d < 7; ++d) if(parallel[d] == pc) { f.kind = Kind::Interchange; f.degree = static_cast<uint8_t>(d); return f; }
    const unsigned target = (pc + 5) % 12;
    for(unsigned d = 0; d < 7; ++d) if(scale[d] == target) {
        const unsigned fifth = (scale[(d + 4) % 7] + 12 - scale[d]) % 12;
        if(fifth == 7) { f.kind = Kind::Secondary; f.degree = static_cast<uint8_t>(d); f.quality = Quality::Dominant; return f; }
    }
    f.kind = Kind::Borrowed; f.root = static_cast<uint8_t>(pc); f.quality = Quality::Major;
    return f;
}

inline void QualityStack(Quality q, uint8_t* tone) {
    static const uint8_t stacks[4][7] = {{0, 4, 7, 11, 14, 18, 21}, {0, 3, 7, 10, 14, 17, 21},
                                         {0, 4, 7, 10, 14, 17, 21}, {0, 5, 7, 10, 14, 17, 21}};
    const unsigned i = q == Quality::Major ? 0 : q == Quality::Minor ? 1 : q == Quality::Dominant ? 2 : 3;
    for(unsigned k = 0; k < 7; ++k) tone[k] = stacks[i][k];
}
// The chord a function names in a key, with Shift applied (context-dependent):
// a dominant -> its tritone substitute; major -> sus4; minor -> dominant on the same
// root (ii -> II7 = V/V); diminished -> the key's V7 (vii is V7 without its root).
FORGE_COLD inline Chord Resolve(const Function& f, uint8_t tonic, Mode mode, bool shift) {
    Chord c; c.kind = f.kind;
    const uint8_t* scale = kScale[static_cast<unsigned>(mode) % kModes];
    tonic %= 12;
    if(f.kind == Kind::Diatonic || f.kind == Kind::Interchange) {
        if(f.kind == Kind::Interchange) scale = kScale[static_cast<unsigned>(Parallel(mode))];
        const unsigned d = f.degree % 7;
        c.degree = static_cast<uint8_t>(d); c.root = static_cast<uint8_t>((tonic + scale[d]) % 12);
        for(unsigned k = 0; k < 7; ++k) {
            const unsigned i = d + 2 * k;
            c.tone[k] = static_cast<uint8_t>(scale[i % 7] + 12 * (i / 7) - scale[d]);
        }
    } else if(f.kind == Kind::Secondary) {
        c.degree = f.degree % 7; c.quality = Quality::Dominant;
        c.root = static_cast<uint8_t>((tonic + scale[c.degree] + 7) % 12);
        QualityStack(Quality::Dominant, c.tone);
    } else if(f.kind == Kind::Borrowed) {
        c.quality = f.quality; c.root = static_cast<uint8_t>((tonic + f.root) % 12);
        QualityStack(f.quality, c.tone);
    } else return c;
    if(!shift) return c;
    c.shifted = true;
    const bool major3 = c.tone[1] == 4, minor3 = c.tone[1] == 3, perfect5 = c.tone[2] == 7, minor7 = c.tone[3] == 10;
    if(major3 && perfect5 && minor7) {                        // dominant: tritone substitute
        c.root = static_cast<uint8_t>((c.root + 6) % 12); c.quality = Quality::Dominant; QualityStack(Quality::Dominant, c.tone);
    } else if(major3) { c.quality = Quality::Sus4; QualityStack(Quality::Sus4, c.tone); }
    else if(minor3 && perfect5) { c.quality = Quality::Dominant; QualityStack(Quality::Dominant, c.tone); }
    else if(minor3) {                                        // diminished: the key's V7
        c.root = static_cast<uint8_t>((tonic + scale[4]) % 12); c.quality = Quality::Dominant; QualityStack(Quality::Dominant, c.tone);
    } else { c.quality = Quality::Sus4; QualityStack(Quality::Sus4, c.tone); }   // a power chord: sus4
    return c;
}
// The chord's tones for an extension, at most `limit`, ascending semitones above the
// root. Dropped first: the 5th, then the middle extensions from the top down (keeps root, 3rd,
// 7th, the top extension and the 9th, which carry the chord's sound).
// 0.19.2 fixes (the owner's harmony review, 2026-10-13):
// - A dominant (secondary dominants, Shift's dominants and tritone substitutes) keeps its 7th
//   even at triad size: without it V/IV was the same chord as I, and in minor V/III and V/VI
//   doubled diatonic chords.
// - A natural 11th over a major 3rd rubs a minor 9th against it: 11th chords drop the 3rd instead
//   (the suspended 11th sound), 13th chords drop the 11th. A minor 9th is kept only on dominants.
FORGE_COLD inline uint8_t Tones(const Chord& c, Extension e, unsigned limit, uint8_t* out) {
    if(limit < 1) return 0;
    if(limit > kMaxNotes) limit = kMaxNotes;
    if(e == Extension::Fifth) { out[0] = 0; if(limit < 2) return 1; out[1] = c.tone[2]; return 2; }
    unsigned top = static_cast<unsigned>(e) + 2;                       // Triad: k = 0..2, ..., 13th: 0..6
    if(c.quality == Quality::Dominant && top < 3) top = 3;
    const bool major3 = c.tone[1] == 4, dominant = major3 && c.tone[3] == 10;
    bool skip[7] = {};
    if(major3 && c.tone[5] == 17 && top >= 5) { if(e == Extension::Eleventh) skip[1] = true; else skip[5] = true; }
    if(c.tone[4] == 13 && !dominant) skip[4] = true;
    uint8_t order[7]; unsigned n = 0;
    auto add = [&](unsigned k) { if(k > top || skip[k]) return; for(unsigned i = 0; i < n; ++i) if(order[i] == k) return; order[n++] = static_cast<uint8_t>(k); };
    add(0); add(1); add(3); add(top);
    for(unsigned k = 4; k <= top; ++k) add(k);
    add(2);
    bool use[7] = {};
    for(unsigned i = 0; i < n && i < limit; ++i) use[order[i]] = true;
    uint8_t count = 0;
    for(unsigned k = 0; k < 7; ++k) if(use[k]) out[count++] = c.tone[k];   // tone[] ascends with k
    return count;
}

// Root position near `root_note` (the root's MIDI note), inversion r (lowest r notes up
// an octave), optional open spread; kept inside kLowest..kHighest.
FORGE_COLD inline Voiced Place(int root_note, const uint8_t* tones, uint8_t count, unsigned inversion, bool open) {
    Voiced v;
    int n[kMaxNotes];
    for(unsigned i = 0; i < count; ++i) n[i] = root_note + tones[i];
    for(unsigned r = 0; r < inversion % (count ? count : 1); ++r) {    // rotate: lowest up an octave
        int lowest = 0; for(unsigned i = 1; i < count; ++i) if(n[i] < n[lowest]) lowest = static_cast<int>(i);
        n[lowest] += 12;
    }
    if(open && count >= 3) {                                           // the second-lowest up an octave
        int a = -1, b = -1;
        for(unsigned i = 0; i < count; ++i) { if(a < 0 || n[i] < n[a]) { b = a; a = static_cast<int>(i); } else if(b < 0 || n[i] < n[b]) b = static_cast<int>(i); }
        n[b] += 12;
    }
    int lo = 127, hi = 0;
    for(unsigned i = 0; i < count; ++i) { if(n[i] < lo) lo = n[i]; if(n[i] > hi) hi = n[i]; }
    while(lo < kLowest && hi + 12 <= kHighest) { lo += 12; hi += 12; for(unsigned i = 0; i < count; ++i) n[i] += 12; }
    while(hi > kHighest && lo - 12 >= kLowest) { lo -= 12; hi -= 12; for(unsigned i = 0; i < count; ++i) n[i] -= 12; }
    for(unsigned i = 0; i < count; ++i) {                               // ascending, unique, 0..127
        const int x = n[i] < 0 ? 0 : n[i] > 127 ? 127 : n[i];
        unsigned at = v.count;
        bool duplicate = false;
        for(unsigned j = 0; j < v.count; ++j) if(v.note[j] == x) duplicate = true;
        if(duplicate) continue;
        while(at > 0 && v.note[at - 1] > x) { v.note[at] = v.note[at - 1]; --at; }
        v.note[at] = static_cast<uint8_t>(x); ++v.count;
    }
    return v;
}
// Movement from the previous chord: each new note to its nearest previous note, plus
// each previous note to its nearest new note (so a chord that drops a voice is not free).
inline unsigned Movement(const Voiced& from, const Voiced& to) {
    auto nearest = [](uint8_t x, const Voiced& v) { unsigned best = 255; for(unsigned i = 0; i < v.count; ++i) { const unsigned d = x > v.note[i] ? x - v.note[i] : v.note[i] - x; if(d < best) best = d; } return best; };
    unsigned cost = 0;
    for(unsigned i = 0; i < to.count; ++i) cost += nearest(to.note[i], from);
    for(unsigned i = 0; i < from.count; ++i) cost += nearest(from.note[i], to);
    return cost;
}
// Voice leading: of every inversion at the root's octave and one either side, the
// voicing that moves least from `previous` (ties: the earlier candidate, so the
// result is deterministic). Common tones cost nothing, so they are kept.
// 0.19.2 fix (the owner, 2026-10-13: "only 1 octave across the whole keyboard"): a voicing's lowest
// note must stay in the pressed key's register, `base` (that octave's C) from a 4th below to its top.
// Player also moves `previous` into that register first, so a change of register plays the same smooth
// shape an octave up or down instead of being pulled back next to the last chord. Root position at the
// root's own octave is always allowed.
FORGE_COLD inline Voiced Lead(int root_note, const uint8_t* tones, uint8_t count, bool open, const Voiced& previous, int base) {
    Voiced best; unsigned best_cost = ~0u;
    for(int shift = 0; shift < 3; ++shift) {
        const int octave = shift == 0 ? 0 : shift == 1 ? -12 : 12;
        for(unsigned r = 0; r < count; ++r) {
            const Voiced v = Place(root_note + octave, tones, count, r, open);
            const bool home = shift == 0 && r == 0;
            if(!home && v.count && (v.note[0] < base - 5 || v.note[0] > base + 11)) continue;
            const unsigned cost = Movement(previous, v);
            if(cost < best_cost) { best_cost = cost; best = v; }
        }
    }
    return best;
}

// The state in one word (LEDs, Inspector, patch v6): bits 0-3 tonic, 4-7 mode, 8-10
// extension, 11-12 inversion, 13 open, 14 block (voice leading off), 15 Real layout,
// 16 enabled, 17 Shift held (Pack only).
inline uint32_t Pack(const State& s, bool shift = false) {
    return (s.tonic % 12u) | (static_cast<uint32_t>(s.mode) % kModes) << 4 | (static_cast<uint32_t>(s.extension) % kExtensions) << 8
         | (s.inversion & 3u) << 11 | (s.open ? 1u << 13 : 0u) | (s.block ? 1u << 14 : 0u)
         | (s.layout == Layout::Real ? 1u << 15 : 0u) | (s.enabled ? 1u << 16 : 0u) | (shift ? 1u << 17 : 0u);
}
inline State Unpack(uint32_t w) {
    State s;
    s.tonic = static_cast<uint8_t>((w & 15u) % 12u); s.mode = static_cast<Mode>(((w >> 4) & 15u) % kModes);
    s.extension = static_cast<Extension>(((w >> 8) & 7u) % kExtensions); s.inversion = static_cast<uint8_t>((w >> 11) & 3u);
    s.open = (w >> 13) & 1u; s.block = (w >> 14) & 1u; s.layout = (w >> 15) & 1u ? Layout::Real : Layout::Static;
    s.enabled = (w >> 16) & 1u;
    return s;
}
// Is pitch class `pc` in the key (tonic + mode)?
inline bool InKey(unsigned pc, const State& s) {
    for(uint8_t step : kScale[static_cast<unsigned>(s.mode) % kModes]) if((s.tonic + step) % 12 == pc % 12) return true;
    return false;
}
inline bool InKeyNote(unsigned note, const State& s) { return InKey(note % 12, s); }
// Split playing (0.20 secret): a melody note moved into the key's scale, to the nearest scale note (a tie goes down).
inline uint8_t SnapToScale(uint8_t note, const State& s) {
    for(int d = 0; d <= 6; ++d) {
        if(note >= d && InKeyNote(note - d, s)) return static_cast<uint8_t>(note - d);
        if(note + d <= 127 && InKeyNote(note + d, s)) return static_cast<uint8_t>(note + d);
    }
    return note;
}

// Keys to chords and who owns which note: each held key keeps the notes it started,
// a per-pitch count lets overlapping chords share notes, so releasing a key (or
// turning harmony off, changing key or mode while holding) never strands a note.
// Zero-initialised (lives in .bss). Audio owner only.
class Player {
public:
    static constexpr unsigned kHeld = 10, kSources = 4;   // + the event recorder (0.15)
    State state;
    // Key down: fills `out` with the notes to start (those not already sounding).
    FORGE_COLD unsigned KeyDown(uint8_t key, uint8_t source, uint8_t* out) {
        if(source >= kSources || key > 127) return 0;
        const Function f = state.layout == Layout::Static ? StaticFunction(key) : RealFunction(key, state.tonic, state.mode);
        if(f.kind == Kind::ShiftKey) { shift_ = true; return 0; }
        if(f.kind == Kind::None) return 0;
        Held* h = Find(key, source);
        if(!h) h = Free();
        if(!h) return 0;                                                // ten chords held: ignore more
        const Chord c = Resolve(f, state.tonic, state.mode, shift_);
        uint8_t tones[kMaxNotes];
        const unsigned limit = state.max_notes ? state.max_notes : kMaxNotes;
        const uint8_t count = Tones(c, state.extension, limit, tones);
        // The root's note: Static, the key's register (C3 / C4 octave); Real, the key.
        int root_note = 48 + 12 * f.octave + c.root;
        if(state.layout == Layout::Real) {
            root_note = key - key % 12 + c.root;
            if(root_note > key + 6) root_note -= 12;                    // a Shift that moves the root: the nearer octave
        }
        const int base = state.layout == Layout::Real ? key - key % 12 : 48 + 12 * f.octave;   // the key's register
        Voiced from = last_;                                            // the last chord, moved into this register
        if(from.count && base != last_base_) {
            const int move = base - last_base_;
            unsigned k = 0;
            for(unsigned i = 0; i < from.count; ++i) { const int x = from.note[i] + move; if(x >= 0 && x <= 127) from.note[k++] = static_cast<uint8_t>(x); }
            from.count = static_cast<uint8_t>(k);
        }
        const Voiced v = !state.block && from.count ? Lead(root_note, tones, count, state.open, from, base)
                                                     : Place(root_note, tones, count, state.inversion, state.open);
        unsigned n = 0;
        if(h->used) n = Release(*h, out);                               // retrigger: let go of the old notes first
        h->used = true; h->key = key; h->source = source; h->count = v.count;
        unsigned started = 0;
        for(unsigned i = 0; i < v.count; ++i) {
            h->note[i] = v.note[i];
            if(refs_[source][v.note[i]]++ == 0) out_on_[started++] = v.note[i];
        }
        for(unsigned i = 0; i < started; ++i) out[n + i] = out_on_[i];
        last_ = v; last_chord_ = c; last_base_ = base; ++changes_;
        newest_ = static_cast<uint8_t>(h - held_);
        released_ = static_cast<uint8_t>(n);
        return n + started;
    }
    // Harmony Mischief (0.19.2): the newest held chord sounds `notes` (same count) instead of its own
    // until Unbend. `out` gets the notes to stop first (Released() of them), then those to start; the
    // per-pitch counts stay right, so releasing the key during a bend still stops everything.
    FORGE_COLD unsigned Bend(const uint8_t* notes, uint8_t* out) {
        if(bent_) { Held& b = held_[bent_index_]; if(b.used && b.key == bent_key_ && b.source == bent_source_ && bent_index_ != newest_) return released_ = 0; }
        Held& h = held_[newest_];
        if(!h.used) return released_ = 0;
        if(!bent_ || bent_index_ != newest_) { for(unsigned i = 0; i < h.count; ++i) own_[i] = h.note[i]; bent_ = true; bent_index_ = newest_; bent_key_ = h.key; bent_source_ = h.source; }
        return Swap(h, notes, out);
    }
    FORGE_COLD unsigned Unbend(uint8_t* out) {
        Held& h = held_[bent_index_];
        const bool same = bent_ && h.used && h.key == bent_key_ && h.source == bent_source_;
        bent_ = false;
        return same ? Swap(h, own_, out) : (released_ = 0);
    }
    bool Bent() const { return bent_; }
    uint8_t BentSource() const { return bent_source_; }
    // The newest held chord's notes and source (what Mischief bends); 0 when none is held.
    FORGE_COLD unsigned Newest(uint8_t* notes, uint8_t* source) const {
        const Held& h = held_[newest_];
        if(!h.used) return 0;
        const uint8_t* from = bent_ && bent_index_ == newest_ ? own_ : h.note;
        for(unsigned i = 0; i < h.count; ++i) notes[i] = from[i];
        *source = h.source; return h.count;
    }
    // After KeyDown: how many of `out`'s first notes were releases (a retrigger).
    unsigned Released() const { return released_; }
    // Key up: fills `out` with the notes to stop (no other key holds them).
    FORGE_COLD unsigned KeyUp(uint8_t key, uint8_t source, uint8_t* out) {
        if(state.layout == Layout::Static && key == kShiftKey) shift_ = false;
        Held* h = Find(key, source);
        return h ? Release(*h, out) : 0;
    }
    // Keys harmony mode takes: Static, the keybed C3..C5; Real, every key.
    bool Maps(uint8_t key) const { return state.layout == Layout::Real ? key < 128 : key >= 48 && key <= 72; }
    // Keys whose key-up harmony must see: a held chord, and the Static Shift key while Shift is
    // on (it holds no chord, so without this its release never reached KeyUp and Shift stuck).
    bool Holds(uint8_t key, uint8_t source) const {
        if(shift_ && state.layout == Layout::Static && key == kShiftKey) return true;
        return const_cast<Player*>(this)->Find(key, source) != nullptr;
    }
    // Panic, a structural patch change: forget everything (the synth is silenced).
    void Clear() { for(auto& h : held_) h = Held{}; for(auto& s : refs_) for(auto& r : s) r = 0; shift_ = false; last_ = Voiced{}; last_base_ = 48; bent_ = false; }
    const Voiced& LastVoiced() const { return last_; }
    const Chord& LastChord() const { return last_chord_; }
    bool Shift() const { return shift_; }
    uint32_t Changes() const { return changes_; }
    unsigned Sounding(uint8_t source) const { unsigned n = 0; if(source < kSources) for(uint8_t r : refs_[source]) n += r != 0; return n; }
private:
    struct Held { bool used = false; uint8_t key = 0, source = 0, count = 0, note[kMaxNotes] = {}; };
    Held* Find(uint8_t key, uint8_t source) { for(auto& h : held_) if(h.used && h.key == key && h.source == source) return &h; return nullptr; }
    Held* Free() { for(auto& h : held_) if(!h.used) return &h; return nullptr; }
    FORGE_COLD unsigned Swap(Held& h, const uint8_t* notes, uint8_t* out) {
        unsigned off = 0, on = 0;
        for(unsigned i = 0; i < h.count; ++i) { uint8_t& r = refs_[h.source][h.note[i]]; if(r && --r == 0) out[off++] = h.note[i]; }
        for(unsigned i = 0; i < h.count; ++i) { h.note[i] = notes[i]; if(refs_[h.source][notes[i]]++ == 0) out_on_[on++] = notes[i]; }
        for(unsigned i = 0; i < on; ++i) out[off + i] = out_on_[i];
        released_ = static_cast<uint8_t>(off);
        return off + on;
    }
    unsigned Release(Held& h, uint8_t* out) {
        unsigned n = 0;
        for(unsigned i = 0; i < h.count; ++i) {
            uint8_t& r = refs_[h.source][h.note[i]];
            if(r && --r == 0) out[n++] = h.note[i];
        }
        h = Held{};
        return n;
    }
    Held held_[kHeld] = {};
    uint8_t refs_[kSources][128] = {};
    uint8_t out_on_[kMaxNotes] = {};
    Voiced last_ = {};
    int last_base_ = 48;                       // the register (C of its octave) of the last chord
    Chord last_chord_ = {};
    bool shift_ = false;
    uint8_t released_ = 0;
    uint32_t changes_ = 0;
    uint8_t newest_ = 0, bent_index_ = 0, own_[kMaxNotes] = {}, bent_key_ = 0, bent_source_ = 0;   // Mischief (0.19.2)
    bool bent_ = false;
};
} // namespace harmony
} // namespace forge
