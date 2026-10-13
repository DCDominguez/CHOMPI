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
            assert(p.Valid() && p.version == 8 && p.synth && p.source == 0 && p.level == base.level);
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
    {   // 0.20: the FX section in a roll: about half with effects, moderate amounts, sometimes another order,
        // never side by side
        magic::Rng fr; unsigned with_fx = 0, reordered = 0;
        for(int i = 0; i < 600; ++i) {
            const Parameters p = magic::Roll(Base(7), fr);
            assert(!(p.fx >> 13 & 1u) && (p.fx >> 8 & 31u) < 24);
            for(unsigned m = 0; m < 4; ++m) assert((p.fx >> (2 * m) & 3u) < 3);
            assert(p.fx_colour <= .45f && p.fx_texture <= .5f && p.fx_motion <= .5f && p.fx_texture_character <= .6f);
            if((p.fx >> 2 & 3u) && p.fx_colour > 0.f) assert(p.fx_colour <= .35f);         // fold / crush lower
            with_fx += p.fx_colour > 0.f || p.fx_texture > 0.f || p.fx_motion > 0.f;
            reordered += (p.fx >> 8 & 31u) != 0;
        }
        assert(with_fx > 230 && with_fx < 370 && reordered > 15 && reordered < 90);
        // Mutate drifts the amounts and sometimes an effect; the order and side by side stay.
        Parameters q = magic::Roll(Base(7), fr); q.fx = static_cast<uint16_t>(q.fx | 5u << 8); q.fx_motion = .3f;
        for(int i = 0; i < 300; ++i) {
            q = magic::Mutate(q, fr);
            assert(q.Valid() && (q.fx >> 8 & 31u) == 5 && !(q.fx >> 13 & 1u) && q.fx_motion >= .02f && q.fx_motion <= .55f);
        }
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
        // -42 dB .. -3 dB; a mono roll with effects may sit 3 dB lower (a phaser or tremolo over its one note, 0.20).
        const bool mono_fx = p.voices == 1 && (p.fx_texture > 0.f || p.fx_motion > 0.f || p.fx_colour > 0.f);
        assert(rms > (mono_fx ? .0056f : .0079f) && rms < .708f && peak <= 1.f);
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
// 0.18 chomp to instrument: a roll over a sampler patch keeps the sample and rolls around it.
void ChompRollsKeepTheSample() {
    magic::Rng r; r.Stir(11);
    Parameters take = Base(7); take.source = 1; take.sample_mode = 0; take.sample_bank = 2; take.sample_slot = 14;
    take.sample_start = .1f; take.sample_end = .8f; take.sample_pitch = .55f;
    unsigned chords = 0, arps = 0;
    for(int i = 0; i < 300; ++i) {
        const Parameters p = magic::Roll(take, r);
        assert(p.Valid() && p.Sampler() && p.sample_slot == 14 && p.sample_bank == 2 && p.sample_start == .1f && p.sample_end == .8f && p.sample_pitch == .55f);
        chords += harmony::Unpack(p.harmony).enabled; arps += parts::Unpack(p.parts_arp, p.parts_clock).pattern != parts::Pattern::Off;
    }
    assert(chords > 140 && chords < 220 && arps > 110 && arps < 190);                 // ~60 % / ~50 %
    Parameters kit = take; kit.sample_mode = 1;
    for(int i = 0; i < 100; ++i) {
        const Parameters p = magic::Roll(kit, r);
        assert(p.Sampler() && p.sample_mode == 1 && !harmony::Unpack(p.harmony).enabled && parts::Unpack(p.parts_arp, p.parts_clock).pattern == parts::Pattern::Off);
    }
}

// 0.18 dream mode: starts after the idle time, plays in the key, any touch ends it with every
// note released, sleeps after its length until touched again.
void DreamPlaysInTheKeyAndStops() {
    magic::Dream d; d.SetTiming(4800, 48000 * 6);
    magic::Rng r; harmony::State key; key.tonic = 2; key.mode = harmony::Mode::Dorian;      // D dorian
    int sounding[128] = {}; unsigned ons = 0;
    auto note = [&](uint8_t n, uint8_t v) { if(v) { ++sounding[n]; ++ons; assert(v == magic::Dream::kVelocity); assert(harmony::InKey(n % 12, key)); } else { --sounding[n]; assert(sounding[n] >= 0); } };
    for(int i = 0; i < 99; ++i) d.Tick(48, true, true, key, r, note);
    assert(!d.Active() && ons == 0);                                                 // not yet: 4,752 frames
    for(int i = 0; i < 48000 * 3 / 48; ++i) d.Tick(48, true, true, key, r, note);
    assert(d.Active() && ons >= 4 && d.KeyMask() != 0);
    d.Tick(48, false, true, key, r, note);                                          // a touch
    assert(!d.Active()); for(int n = 0; n < 128; ++n) assert(sounding[n] == 0);
    ons = 0;
    for(int i = 0; i < 48000 * 12 / 48; ++i) d.Tick(48, true, true, key, r, note);   // idle again: dreams 6 s, then sleeps
    assert(!d.Active() && ons > 4); for(int n = 0; n < 128; ++n) assert(sounding[n] == 0);
    const unsigned slept = ons;
    for(int i = 0; i < 48000 * 10 / 48; ++i) d.Tick(48, true, true, key, r, note);
    assert(ons == slept);                                                            // asleep until touched
    for(int i = 0; i < 48000 * 2 / 48; ++i) d.Tick(48, true, false, key, r, note);   // not allowed (e.g. looper playing)
    assert(!d.Active());
}
} // namespace

int main() {
    RollsAreValidAndBounded(); RollsArePlayable(); MutationsDriftInsideTheirBounds(); ChimePlaysFourNotes();
    ChompRollsKeepTheSample(); DreamPlaysInTheKeyAndStops();
    std::cout << "PASS: surprise-me rolls valid / bounded / playable, mutations bounded, chime, chomp rolls, dream\n";
}
