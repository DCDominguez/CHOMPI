// Harmony mode, Phase 1 (core/harmony.h): chord identity for all 12 tonics x 9 modes x 7
// degrees, extensions and their drop rules, Static (function stays under the finger when
// the key changes) and Real (the key is the root) layouts, chromatic keys, Shift,
// inversions, voice leading, note ownership (no stuck notes) and range/limits.
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>
#include "../core/harmony.h"

using namespace forge::harmony;

namespace {
std::vector<unsigned> PitchClasses(const Chord& c, Extension e, unsigned limit = kMaxNotes) {
    uint8_t t[kMaxNotes]; const unsigned n = Tones(c, e, limit, t);
    std::vector<unsigned> pcs; for(unsigned i = 0; i < n; ++i) pcs.push_back((c.root + t[i]) % 12);
    return pcs;
}
std::vector<unsigned> Notes(const Voiced& v) { return std::vector<unsigned>(v.note, v.note + v.count); }
Function Degree(unsigned d) { Function f; f.kind = Kind::Diatonic; f.degree = static_cast<uint8_t>(d); return f; }
bool InScale(unsigned pc, unsigned tonic, Mode m) {
    for(unsigned d = 0; d < 7; ++d) if((tonic + kScale[static_cast<unsigned>(m)][d]) % 12 == pc) return true;
    return false;
}

void Identity() {
    // Every tonic, mode and degree: the root is the degree's note, every tone up to the
    // 13th is in the scale, and the stack is thirds (2..4 semitones apart, ascending).
    for(unsigned tonic = 0; tonic < 12; ++tonic) for(unsigned m = 0; m < kModes; ++m) for(unsigned d = 0; d < 7; ++d) {
        const Mode mode = static_cast<Mode>(m);
        const Chord c = Resolve(Degree(d), static_cast<uint8_t>(tonic), mode, false);
        assert(c.root == (tonic + kScale[m][d]) % 12 && c.tone[0] == 0);
        for(unsigned k = 1; k < 7; ++k) { const int step = c.tone[k] - c.tone[k - 1]; assert(step >= 2 && step <= 4); }
        for(unsigned k = 0; k < 7; ++k) assert(InScale((c.root + c.tone[k]) % 12, tonic, mode));
        for(unsigned e = 0; e < kExtensions; ++e) {
            uint8_t t[kMaxNotes]; const unsigned n = Tones(c, static_cast<Extension>(e), kMaxNotes, t);
            static const unsigned expect[kExtensions] = {3, 4, 5, 5, 5, 2};
            assert(n == expect[e] && t[0] == 0);
            for(unsigned i = 1; i < n; ++i) assert(t[i] > t[i - 1]);
        }
    }
    // Known chords (pitch classes C=0).
    auto pcs = [](unsigned tonic, Mode m, unsigned d, Extension e) { return PitchClasses(Resolve(Degree(d), static_cast<uint8_t>(tonic), m, false), e); };
    assert((pcs(0, Mode::Major, 3, Extension::Seventh) == std::vector<unsigned>{5, 9, 0, 4}));          // C major IV7 = F A C E
    assert((pcs(0, Mode::Major, 4, Extension::Seventh) == std::vector<unsigned>{7, 11, 2, 5}));         // V7 = G B D F
    assert((pcs(9, Mode::NaturalMinor, 0, Extension::Triad) == std::vector<unsigned>{9, 0, 4}));        // A minor i
    assert((pcs(9, Mode::HarmonicMinor, 4, Extension::Triad) == std::vector<unsigned>{4, 8, 11}));      // E G# B
    assert((pcs(11, Mode::Locrian, 0, Extension::Triad) == std::vector<unsigned>{11, 2, 5}));           // B diminished
    assert((pcs(5, Mode::Lydian, 1, Extension::Triad) == std::vector<unsigned>{7, 11, 2}));             // F Lydian II = G major
    assert((pcs(10, Mode::NaturalMinor, 3, Extension::Ninth) == std::vector<unsigned>{3, 6, 10, 1, 5}));  // A# minor iv9 = D#m9
    assert((pcs(0, Mode::Major, 0, Extension::Fifth) == std::vector<unsigned>{0, 7}));
    // Drop rules: 11th and 13th keep root, 3rd, 7th and the top extension; the 5th goes first.
    assert((pcs(0, Mode::Major, 1, Extension::Eleventh) == std::vector<unsigned>{2, 5, 0, 4, 7}));      // Dm11: D F C E G (no A)
    assert((pcs(0, Mode::Major, 4, Extension::Thirteenth) == std::vector<unsigned>{7, 11, 5, 0, 4}));   // G13: G B F C E
    const Chord g = Resolve(Degree(4), 0, Mode::Major, false);
    assert((PitchClasses(g, Extension::Ninth, 4) == std::vector<unsigned>{7, 11, 5, 9}));              // 4 voices: no 5th
    assert((PitchClasses(g, Extension::Seventh, 3) == std::vector<unsigned>{7, 11, 5}));
    assert((PitchClasses(g, Extension::Triad, 2) == std::vector<unsigned>{7, 11}) && PitchClasses(g, Extension::Triad, 0).empty());
}

void Layouts() {
    // Static: I vi IV V under the same keys (C3 A3 F3 G3) in every key: the roots follow the tonic.
    const uint8_t keys[4] = {48, 57, 53, 55}; const unsigned degrees[4] = {0, 5, 3, 4};
    for(unsigned tonic = 0; tonic < 12; ++tonic) for(unsigned i = 0; i < 4; ++i) {
        const Function f = StaticFunction(keys[i]);
        assert(f.kind == Kind::Diatonic && f.degree == degrees[i] && f.octave == 0);
        assert(Resolve(f, static_cast<uint8_t>(tonic), Mode::Major, false).root == (tonic + kScale[0][degrees[i]]) % 12);
    }
    assert(StaticFunction(60).degree == 0 && StaticFunction(60).octave == 1 && StaticFunction(71).degree == 6);
    assert(StaticFunction(72).kind == Kind::ShiftKey && StaticFunction(47).kind == Kind::None && StaticFunction(73).kind == Kind::None);
    // Black keys: C#3 V/ii (A7 in C), F#3 V/V (D7), A#3 bVII (Bb), C#4 iv (Fm), D#4 bIII (Eb), F#4 bVI (Ab), G#4 V/IV (C7), A#4 bII (Db).
    auto root_quality = [](uint8_t key, unsigned tonic, Mode m) { const Chord c = Resolve(StaticFunction(key), static_cast<uint8_t>(tonic), m, false); return std::make_pair(unsigned(c.root), PitchClasses(c, Extension::Seventh)); };
    assert((root_quality(49, 0, Mode::Major).second == std::vector<unsigned>{9, 1, 4, 7}));     // A7
    assert((root_quality(54, 0, Mode::Major).second == std::vector<unsigned>{2, 6, 9, 0}));     // D7
    assert((root_quality(58, 0, Mode::Major).second == std::vector<unsigned>{10, 2, 5, 8}));    // Bb7 (Bb from C minor)
    assert((root_quality(61, 0, Mode::Major).second == std::vector<unsigned>{5, 8, 0, 3}));     // Fm7
    assert(root_quality(63, 0, Mode::Major).first == 3 && root_quality(66, 0, Mode::Major).first == 8);
    assert((root_quality(68, 0, Mode::Major).second == std::vector<unsigned>{0, 4, 7, 10}));    // C7 = V/IV
    assert((root_quality(70, 0, Mode::Major).second == std::vector<unsigned>{1, 5, 8, 0}));     // Db maj7
    assert(root_quality(54, 4, Mode::Major).first == 6);                                         // V/V in E = F#
    // Real: the key is the root; in C major D = Dm, Eb = Eb (from C minor), Db = Db (bII).
    const Function d = RealFunction(62, 0, Mode::Major), eb = RealFunction(63, 0, Mode::Major), db = RealFunction(61, 0, Mode::Major);
    assert(d.kind == Kind::Diatonic && d.degree == 1 && eb.kind == Kind::Interchange && db.kind == Kind::Borrowed);
    assert((PitchClasses(Resolve(eb, 0, Mode::Major, false), Extension::Triad) == std::vector<unsigned>{3, 7, 10}));
    assert((PitchClasses(Resolve(db, 0, Mode::Major, false), Extension::Triad) == std::vector<unsigned>{1, 5, 8}));
    for(unsigned tonic = 0; tonic < 12; ++tonic) for(unsigned m = 0; m < kModes; ++m) for(uint8_t key = 0; key < 128; ++key) {
        const Function f = RealFunction(key, static_cast<uint8_t>(tonic), static_cast<Mode>(m));
        assert(f.kind != Kind::None && Resolve(f, static_cast<uint8_t>(tonic), static_cast<Mode>(m), false).root == key % 12);
    }
    // Shift: V7 -> tritone substitute, major -> sus4, minor -> dominant, diminished -> V7.
    auto shifted = [](unsigned d) { return PitchClasses(Resolve(Degree(d), 0, Mode::Major, true), Extension::Seventh); };
    const Chord sub = Resolve(Degree(4), 0, Mode::Major, true);                  // the V triad (G B D) is major, its 7th dominant
    assert(sub.shifted && sub.root == 1);                                        // G7 -> Db7
    assert((shifted(0) == std::vector<unsigned>{0, 5, 7, 10}));                  // C (major) -> Csus4 (7sus4 with a 7th)
    assert((PitchClasses(Resolve(Degree(0), 0, Mode::Major, true), Extension::Triad) == std::vector<unsigned>{0, 5, 7}));
    assert((shifted(1) == std::vector<unsigned>{2, 6, 9, 0}));                   // Dm -> D7 (V/V)
    assert((shifted(6) == std::vector<unsigned>{7, 11, 2, 5}));                  // Bdim -> G7
    const Function black = StaticFunction(54);                                   // V/V shifted -> its tritone substitute (Ab7)
    assert(Resolve(black, 0, Mode::Major, true).root == 8);
}

void Voicing() {
    const Chord c = Resolve(Degree(0), 0, Mode::Major, false);
    uint8_t t[kMaxNotes]; const uint8_t n = Tones(c, Extension::Triad, kMaxNotes, t);
    assert((Notes(Place(60, t, n, 0, false)) == std::vector<unsigned>{60, 64, 67}));
    assert((Notes(Place(60, t, n, 1, false)) == std::vector<unsigned>{64, 67, 72}));
    assert((Notes(Place(60, t, n, 2, false)) == std::vector<unsigned>{67, 72, 76}));
    assert((Notes(Place(60, t, n, 3, false)) == std::vector<unsigned>{60, 64, 67}));         // wraps
    assert((Notes(Place(60, t, n, 0, true)) == std::vector<unsigned>{60, 67, 76}));          // open: E up an octave
    // Range: always inside C2..C7 and 0..127, no duplicate notes.
    for(int root = -20; root < 150; ++root) for(unsigned inv = 0; inv < 4; ++inv) {
        const Voiced v = Place(root, t, n, inv, inv & 1);
        assert(v.count == 3);
        for(unsigned i = 0; i < v.count; ++i) { assert(v.note[i] >= kLowest && v.note[i] <= kHighest); if(i) assert(v.note[i] > v.note[i - 1]); }
    }
    // Voice leading: C -> F -> G -> C keeps common tones and moves by small steps.
    State s; Voiced prev = Place(60, t, n, 0, false);
    unsigned total = 0;
    for(unsigned d : {3u, 4u, 0u, 5u, 1u, 4u, 0u}) {
        const Chord next = Resolve(Degree(d), 0, Mode::Major, false);
        uint8_t nt[kMaxNotes]; const uint8_t nn = Tones(next, Extension::Triad, kMaxNotes, nt);
        const Voiced led = Lead(48 + next.root, nt, nn, false, prev);
        const Voiced block = Place(48 + next.root, nt, nn, 0, false);
        assert(Movement(prev, led) <= Movement(prev, block));
        std::set<unsigned> common; for(unsigned i = 0; i < prev.count; ++i) for(unsigned j = 0; j < led.count; ++j) if(prev.note[i] == led.note[j]) common.insert(prev.note[i]);
        std::set<unsigned> shared_pc; for(unsigned i = 0; i < prev.count; ++i) for(unsigned j = 0; j < nn; ++j) if(prev.note[i] % 12 == (next.root + nt[j]) % 12) shared_pc.insert(prev.note[i] % 12);
        assert(common.size() == shared_pc.size());                               // every shared pitch class held in place
        for(unsigned i = 0; i < led.count; ++i) assert(led.note[i] >= kLowest && led.note[i] <= kHighest);
        total += Movement(prev, led); prev = led;
    }
    assert(total < 60);
    // C -> F (led): C stays, E -> F, G -> A.
    const Chord f = Resolve(Degree(3), 0, Mode::Major, false);
    uint8_t ft[kMaxNotes]; const uint8_t fn = Tones(f, Extension::Triad, kMaxNotes, ft);
    assert((Notes(Lead(53, ft, fn, false, Place(60, t, n, 0, false))) == std::vector<unsigned>{60, 65, 69}));
    // Deterministic: the same input gives the same voicing.
    assert(Notes(Lead(53, ft, fn, false, Place(60, t, n, 0, false))) == Notes(Lead(53, ft, fn, false, Place(60, t, n, 0, false))));
}

// Note ownership: random key presses/releases on every source, with harmony settings
// changing in between; every note started is stopped exactly once, nothing stays on.
void Ownership() {
    Player p; p.state.enabled = true;
    int sounding[3][128] = {};
    uint8_t out[2 * kMaxNotes];
    auto down = [&](uint8_t key, uint8_t src) {
        const unsigned n = p.KeyDown(key, src, out), off = p.Released();
        for(unsigned i = 0; i < n; ++i) { int& s = sounding[src][out[i]]; if(i < off) { assert(s == 1); s = 0; } else { assert(s == 0); s = 1; } }
    };
    auto up = [&](uint8_t key, uint8_t src) {
        const unsigned n = p.KeyUp(key, src, out);
        for(unsigned i = 0; i < n; ++i) { assert(sounding[src][out[i]] == 1); sounding[src][out[i]] = 0; }
    };
    // Two keys sharing a note: the shared note stops only when both are up.
    down(48, 2); down(57, 2);                                    // C and Am share C and E
    up(48, 2);
    bool still = false; for(int n = 0; n < 128; ++n) still |= sounding[2][n] == 1;
    assert(still && p.Sounding(2) > 0);
    up(57, 2); assert(p.Sounding(2) == 0);
    for(auto& s : sounding[2]) assert(s == 0);
    // Shift (C5) never sounds and only bends the next chord.
    down(72, 2); assert(p.Shift() && p.Sounding(2) == 0); down(55, 2); assert(p.LastChord().shifted); up(72, 2); up(55, 2);
    assert(!p.Shift() && p.Sounding(2) == 0);
    std::srand(12345);
    bool held[3][128] = {};
    for(int step = 0; step < 200000; ++step) {
        const uint8_t key = static_cast<uint8_t>(36 + std::rand() % 50), src = static_cast<uint8_t>(std::rand() % 3);
        const int what = std::rand() % 12;
        if(what == 0) { p.state.tonic = static_cast<uint8_t>(std::rand() % 12); p.state.mode = static_cast<Mode>(std::rand() % kModes); }
        else if(what == 1) { p.state.extension = static_cast<Extension>(std::rand() % kExtensions); p.state.layout = static_cast<Layout>(std::rand() % 2); }
        else if(what == 2) { p.state.block = !p.state.block; p.state.open = std::rand() % 2; p.state.inversion = static_cast<uint8_t>(std::rand() % 4); p.state.max_notes = static_cast<uint8_t>(std::rand() % 6); }
        else if(held[src][key]) { up(key, src); held[src][key] = false; }
        else { down(key, src); held[src][key] = true; }
    }
    for(uint8_t src = 0; src < 3; ++src) for(uint8_t key = 0; key < 128; ++key) if(held[src][key]) up(key, src);
    for(auto& s : sounding) for(int x : s) assert(x == 0);
    for(uint8_t src = 0; src < 3; ++src) assert(p.Sounding(src) == 0);
    // Limits: an 11th key held, bad sources/keys ignored; Clear forgets everything.
    for(uint8_t k = 48; k < 60; ++k) down(k, 0);
    assert(p.KeyDown(60, 3, out) == 0 && p.KeyDown(200, 0, out) == 0);
    p.Clear(); assert(p.Sounding(0) == 0 && p.KeyUp(48, 0, out) == 0);
}
} // namespace

int main() {
    Identity(); Layouts(); Voicing(); Ownership();
    std::cout << "PASS: harmony identity (12 tonics x 9 modes x 7 degrees x 6 extensions), drop rules, Static/Real layouts,"
                 " chromatic keys, Shift, inversions/spread/range, voice leading, note ownership (200k random events)\n";
}
