// The magic batch (core/magic.h, 0.17): surprise-me rolls stay valid, inside their bounds
// and playable (rendered through the engine: never silent, never harsh), mutations drift
// without leaving their bounds or touching level / harmony / parts, the chime's notes.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include "../core/engine.h"
#include "../core/magic.h"

using namespace forge;

namespace {
Parameters Base(uint8_t version) {
    Parameters p; p.version = version; p.synth = version > 1; p.level = .5f;
    if(version >= 6) { harmony::State h; h.tonic = 9; p.harmony = harmony::Pack(h); }
    if(version >= 7) { parts::Settings s; s.bpm = 96; p.parts_arp = parts::PackArp(s); p.parts_clock = parts::PackClock(s); }
    return p;
}

void RollsAreValidAndBounded() {
    magic::Rng r;
    for(uint8_t version : {uint8_t(1), uint8_t(5), uint8_t(7)}) {
        const Parameters base = Base(version);
        unsigned harmony_on = 0, arp_on = 0, mono = 0;
        for(int i = 0; i < 300; ++i) {
            const Parameters p = magic::Roll(base, r);
            assert(p.Valid() && p.version == 7 && p.synth && p.source == 0 && p.level == base.level);
            assert(p.cutoff >= .45f && p.cutoff <= .9f && p.feedback <= .53f && p.attack <= .151f);
            assert(!(p.cutoff > .75f && p.resonance > .35f));
            assert(p.voices == 1 || p.voices >= 4);
            const harmony::State h = harmony::Unpack(p.harmony);
            if(version >= 6) assert(h.tonic == 9);                                   // the player's key
            const parts::Settings s = parts::Unpack(p.parts_arp, p.parts_clock);
            assert(!s.latch && s.bass == parts::Bass::Off);
            if(version >= 7) assert(s.bpm == 96);                                    // the player's tempo
            harmony_on += h.enabled; arp_on += s.pattern != parts::Pattern::Off; mono += p.voices == 1;
        }
        assert(harmony_on > 30 && harmony_on < 130 && arp_on > 20 && arp_on < 110 && mono > 20 && mono < 110);
    }
    magic::Rng a, b; a.Stir(42); b.Stir(42);
    const Parameters x = magic::Roll(Base(7), a), y = magic::Roll(Base(7), b);
    assert(x.cutoff == y.cutoff && x.waveform == y.waveform);                       // same seed, same roll
}

// Every roll is heard: a C major chord for 0.6 s through the engine (delay and reverb on),
// RMS between -42 dB (a mono roll sounds one of the three notes) and -3 dB, nothing above full scale, everything finite.
void RollsArePlayable() {
    std::vector<float> l(48002), r(48002), rv(Reverb::Required(48000));
    Engine engine; assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
    magic::Rng rng; rng.Stir(7);
    float quietest = 1.f, loudest = 0.f;
    for(int i = 0; i < 150; ++i) {
        const Parameters p = magic::Roll(Base(7), rng);
        assert(engine.ApplyPatch(p));
        for(uint8_t n : {uint8_t(60), uint8_t(64), uint8_t(67)}) engine.Note(n, 100, 2);
        double sum = 0; float peak = 0.f;
        for(int k = 0; k < 28800; ++k) {
            float L, R; engine.Process(0.f, 0.f, L, R);
            assert(std::isfinite(L) && std::isfinite(R));
            sum += L * L + R * R; peak = std::max(peak, std::max(std::fabs(L), std::fabs(R)));
        }
        for(uint8_t n : {uint8_t(60), uint8_t(64), uint8_t(67)}) engine.Note(n, 0, 2);
        const float rms = static_cast<float>(std::sqrt(sum / (2 * 28800)));
        quietest = std::min(quietest, rms); loudest = std::max(loudest, rms);
        assert(rms > .0079f && rms < .708f && peak <= 1.f);                        // -42 dB .. -3 dB
        engine.Panic(); for(int k = 0; k < 4800; ++k) { float L, R; engine.Process(0.f, 0.f, L, R); }
    }
    std::cout << "rolls: RMS " << 20 * std::log10(quietest) << " .. " << 20 * std::log10(loudest) << " dB\n";
}

void MutationsDriftInsideTheirBounds() {
    magic::Rng r; r.Stir(3);
    Parameters p = magic::Roll(Base(7), r);
    const Parameters start = p;
    bool moved = false;
    for(int i = 0; i < 1000; ++i) {
        const Parameters q = magic::Mutate(p, r);
        assert(q.Valid() && q.version == start.version && q.level == start.level);
        assert(q.harmony == start.harmony && q.parts_arp == start.parts_arp && q.parts_clock == start.parts_clock);
        assert(q.waveform == start.waveform && q.voices == start.voices && q.source == start.source);
        assert(std::fabs(q.cutoff - p.cutoff) <= .0601f && q.feedback <= .6f && q.resonance <= .65f);
        moved = moved || q.cutoff != p.cutoff;
        p = q;
    }
    assert(moved);
    Parameters bright = Base(7); bright.cutoff = .99f; bright.feedback = .8f;      // set outside the bounds by the player
    for(int i = 0; i < 200; ++i) { bright = magic::Mutate(bright, r); assert(bright.cutoff <= .99f && bright.feedback <= .8f); }
    Parameters v1 = Base(1); v1 = magic::Mutate(v1, r); assert(v1.Valid() && v1.version == 1 && !v1.synth);
}

void ChimePlaysFourNotes() {
    magic::Chime chime; assert(!chime.Active() && chime.Progress() == 0.f);
    chime.Start();
    std::vector<std::pair<uint8_t, uint8_t>> events; float last = -1.f;
    while(chime.Active()) {
        assert(chime.Progress() > last); last = chime.Progress();
        chime.Tick(48, [&](uint8_t n, uint8_t v) { events.push_back({n, v}); });
    }
    assert(events.size() == 8);
    unsigned on = 0; for(auto& e : events) on += e.second > 0;
    assert(on == 4 && events.front().first == 60 && events.front().second == magic::Chime::kVelocity);
    assert(events.back().first == 72 && events.back().second == 0);
    chime.Tick(48, [&](uint8_t, uint8_t) { assert(false); });                     // finished: nothing more
}
} // namespace

int main() {
    RollsAreValidAndBounded(); RollsArePlayable(); MutationsDriftInsideTheirBounds(); ChimePlaysFourNotes();
    std::cout << "PASS: surprise-me rolls valid / bounded / playable, mutations bounded, chime\n";
}
