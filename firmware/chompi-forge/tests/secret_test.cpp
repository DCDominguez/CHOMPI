// Harmony's secret modes (core/secret.h, 0.19.2): Chord Bloom (order, timing on the tempo, notes
// accumulate, key-up cancels the rest), Chord Conversations (a bar after the chord ends, one voice,
// notes from the chord and the key, a key cuts it off), Harmony Mischief (every variant bends the held
// chord and comes back home, no stuck notes), and the panel (SW3 on the harmony page picks the mode,
// holding a chord turns SW1 + SW3 into Mischief, letting go brings the chord home).
#include <algorithm>
#include <cassert>
#include <iostream>
#include <set>
#include <vector>
#include "../core/panel_controller.h"

using namespace forge;
using namespace forge::harmony;

namespace {
struct Sink final : PanelSink {
    bool PresetAction(const MenuAction&, const Parameters&) override { return true; }
    bool SampleJob(const forge::SampleJob&) override { return true; }
    void Flash(bool) override {}
};
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<int16_t> rec = std::vector<int16_t>(2 * 48000);
    SampleTable table; Engine engine; Player player; Recorder recorder; PanelController panel; Sink sink; PanelInput hw;
    Rig() {
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetSamples(&table); engine.SetHarmony(&player);
        recorder.Init(rec.data(), 48000, &table.slots[kRamSlot], 48000.f);
        Parameters synth; synth.version = 4; synth.synth = true; synth.voices = 7; synth.release = 0.f; synth.sustain = 1.f;
        assert(engine.ApplyPatch(synth));
        player.state.enabled = true; player.state.extension = Extension::Seventh;   // Cmaj7 on C4
        hw.frames = 24; hw.toggle_up = true;   // the play / menu position
    }
    // One 24-frame block, as forge_main: the panel, the engine's block tick, then the samples.
    void Block() {
        panel.Block(hw, engine, recorder, sink); engine.Block(24);
        for(int i = 0; i < 24; ++i) { float a, b; engine.Process(0, 0, a, b); }
        for(auto& t : hw.turns) t = 0;
    }
    void Run(uint32_t frames) { for(uint32_t i = 0; i < frames / 24; ++i) Block(); }
};

void BloomUnit() {
    secret::Rand r; secret::Bloom b; uint8_t first;
    const uint8_t chord[4] = {64, 60, 71, 67};
    std::vector<uint8_t> heard; auto take = [&](uint8_t n, uint8_t, uint8_t) { heard.push_back(n); };
    assert(b.Schedule(chord, 4, 100, 2, secret::Mode::BloomUp, 1000, r, &first) == 1 && first == 60 && b.Pending() == 3);
    b.Advance(999, take); assert(heard.empty());
    b.Advance(1, take); assert((heard == std::vector<uint8_t>{64}));
    b.Advance(2000, take); assert((heard == std::vector<uint8_t>{64, 67, 71}) && b.Pending() == 0);   // up, one per step
    heard.clear(); b.Schedule(chord, 4, 100, 2, secret::Mode::BloomDown, 10, r, &first);
    b.Advance(100, take); assert(first == 71 && (heard == std::vector<uint8_t>{67, 64, 60}));          // down
    heard.clear(); b.Schedule(chord, 4, 100, 2, secret::Mode::BloomRandom, 10, r, &first);
    b.Advance(100, take); heard.push_back(first);
    assert((std::set<uint8_t>(heard.begin(), heard.end()) == std::set<uint8_t>{60, 64, 67, 71}));   // random: every note once
    b.Schedule(chord, 4, 100, 2, secret::Mode::BloomUp, 10, r, &first);
    const uint8_t gone[2] = {67, 71}; b.Cancel(gone, 2, 2); assert(b.Pending() == 1);                 // key up: those never bloom
    b.Cancel(chord, 4, 3); assert(b.Pending() == 1);                                                 // another source's chord is not touched
    assert(secret::Sixteenth(48000.f, 120.f) == 6000 && secret::Sixteenth(48000.f, 0.f) == 6000);
}

void BloomInEngine() {
    Rig rig; rig.engine.SetSecret(secret::Mode::BloomUp);
    const uint32_t step = secret::Sixteenth(48000.f, 120.f);                     // no parts: 120 BPM
    rig.engine.Note(60, 100, 2); rig.Run(240);
    assert(rig.engine.ActiveVoices() == 1);                                        // the root first
    rig.Run(step); assert(rig.engine.ActiveVoices() == 2);
    rig.Run(2 * step); assert(rig.engine.ActiveVoices() == 4);                    // grown into Cmaj7, all still sounding
    rig.Run(4 * step); assert(rig.engine.ActiveVoices() == 4 && rig.engine.Blooming().Pending() == 0);
    rig.engine.Note(60, 0, 2); rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    // Let go mid-bloom: the rest never comes.
    rig.engine.Note(60, 100, 2); rig.Run(step + 240); assert(rig.engine.ActiveVoices() == 2);
    rig.engine.Note(60, 0, 2); assert(rig.engine.Blooming().Pending() == 0);
    rig.Run(4 * step); assert(rig.engine.ActiveVoices() == 0);
    // Off: the chord arrives at once.
    rig.engine.SetSecret(secret::Mode::Off); rig.engine.Note(60, 100, 2); rig.Run(240); assert(rig.engine.ActiveVoices() == 4);
    rig.engine.Note(60, 0, 2);
}

void Conversations() {
    Rig rig; rig.engine.SetSecret(secret::Mode::TalkCall);
    const uint32_t step = secret::Sixteenth(48000.f, 120.f);
    rig.engine.Note(60, 100, 2); rig.Run(4800); rig.engine.Note(60, 0, 2);
    secret::Conversation& talk = rig.engine.Talk();
    assert(talk.Speaking() && talk.Length() >= 3 && talk.Length() <= secret::Conversation::kMaxPhrase);
    for(unsigned i = 0; i < talk.Length(); ++i) {                                // every note is the chord's or the key's
        if(!talk.Note(i)) continue;                                                // a rest
        const unsigned pc = talk.Note(i) % 12;
        assert(InKey(pc, rig.player.state) || pc == 0 || pc == 4 || pc == 7 || pc == 11);
        assert(talk.Note(i) >= kLowest && talk.Note(i) <= kHighest);
    }
    rig.Run(14 * step); assert(talk.Sounding() == 0);                              // a bar of silence first
    rig.Run(3 * step); assert(talk.Sounding() != 0);                               // then Choowi answers
    unsigned most = 0; for(int i = 0; i < 20 * static_cast<int>(step) / 24; ++i) { rig.Block(); most = std::max(most, static_cast<unsigned>(talk.Sounding() != 0)); }
    assert(most == 1 && !talk.Speaking());                                         // one note at a time, then done
    rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    // A key cuts the answer off at once.
    rig.engine.Note(62, 100, 2); rig.Run(480); rig.engine.Note(62, 0, 2);
    rig.Run(17 * step); assert(talk.Sounding() != 0);
    rig.engine.Note(64, 100, 2); assert(talk.Sounding() == 0 && !talk.Speaking());
    rig.engine.Note(64, 0, 2);
    rig.engine.Panic(); assert(!talk.Speaking());
}

// The motif families: every note is the chord's or the key's, Groove rests, Sparkle sits high and
// Groove low, and an answer repeats the last motif only as a "develop" (about one in three).
void MotifFamilies() {
    secret::Rand r; State key; key.tonic = 0; key.mode = Mode::Major;
    Voiced cmaj7; cmaj7.count = 4; cmaj7.note[0] = 60; cmaj7.note[1] = 64; cmaj7.note[2] = 67; cmaj7.note[3] = 71;
    double mean[secret::kFamilies] = {};
    bool rested[secret::kFamilies] = {};
    for(unsigned f = 0; f < secret::kFamilies; ++f) {
        secret::Conversation talk; unsigned repeats = 0, previous = 99, total = 0, notes = 0;
        for(int i = 0; i < 300; ++i) {
            talk.ChordEnded(cmaj7, key, static_cast<secret::Family>(f), 6000, r);
            assert(talk.Length() >= 3 && talk.Length() <= secret::Conversation::kMaxPhrase);
            for(unsigned k = 0; k < talk.Length(); ++k) {
                assert(talk.Steps(k) >= 1 && talk.Steps(k) <= 4);
                if(!talk.Note(k)) { rested[f] = true; continue; }
                assert(InKey(talk.Note(k) % 12, key) && talk.Note(k) >= kLowest && talk.Note(k) <= kHighest);
                assert(talk.Velocity(k) >= 40 && talk.Velocity(k) <= 100);
                total += talk.Note(k); ++notes;
            }
            if(talk.LastMotif() == previous) ++repeats;
            previous = talk.LastMotif();
        }
        mean[f] = double(total) / notes;
        assert(repeats > 60 && repeats < 140);                                     // ~100 of 300: develop, not chance repeats
    }
    assert(rested[static_cast<unsigned>(secret::Family::Groove)] && !rested[static_cast<unsigned>(secret::Family::Echo)]);
    assert(mean[static_cast<unsigned>(secret::Family::Sparkle)] > mean[static_cast<unsigned>(secret::Family::Echo)] + 6);
    assert(mean[static_cast<unsigned>(secret::Family::Groove)] < mean[static_cast<unsigned>(secret::Family::Call)]);
    // Echo answers with the chord's top note, Call's first motif climbs to the octave.
    secret::Conversation echo; echo.ChordEnded(cmaj7, key, secret::Family::Echo, 6000, r); assert(echo.Note(0) == 71);
    assert(secret::FamilyOf(secret::Mode::TalkSparkle) == secret::Family::Sparkle && !secret::Talks(secret::Mode::BloomUp));
}

void Mischief() {
    for(unsigned variant = 0; variant < secret::kMischiefs; ++variant) {
        Rig rig;
        rig.engine.Note(60, 100, 2); rig.Run(240);
        uint8_t own[kMaxNotes], src; const unsigned n = rig.player.Newest(own, &src);
        assert(n == 4 && rig.engine.CanBend());
        assert(rig.engine.HarmonyBend(variant) && rig.player.Bent());
        uint8_t bent[kMaxNotes]; rig.player.Newest(bent, &src);                    // Newest still reports the chord's own notes
        assert(std::equal(own, own + n, bent));
        rig.Run(240); assert(rig.engine.ActiveVoices() >= 3);
        rig.engine.HarmonyUnbend(); assert(!rig.player.Bent());
        rig.Run(9600); assert(rig.engine.ActiveVoices() == 4);                     // home: the original chord
        rig.engine.Note(60, 0, 2); rig.Run(9600);
        assert(rig.engine.ActiveVoices() == 0 && rig.player.Sounding(2) == 0);
        // Let go of the key while bent: everything stops; bringing it home afterwards does nothing.
        rig.engine.Note(60, 100, 2); rig.engine.HarmonyBend(variant); rig.engine.Note(60, 0, 2);
        assert(rig.player.Sounding(2) == 0); rig.engine.HarmonyUnbend(); assert(rig.player.Sounding(2) == 0);
        rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    }
    // The bends themselves: sus4, sus2, borrowed third, next inversion, a semitone up.
    const uint8_t c[4] = {60, 64, 67, 71}; uint8_t o[4];
    secret::Bend(c, 4, 0, 0, o); assert(o[1] == 65 && o[0] == 60 && o[2] == 67);
    secret::Bend(c, 4, 0, 1, o); assert(o[1] == 62);
    secret::Bend(c, 4, 0, 2, o); assert(o[1] == 63);
    secret::Bend(c, 4, 0, 3, o); assert(o[0] == 72 && o[1] == 64);
    secret::Bend(c, 4, 0, 4, o); assert(o[0] == 61 && o[3] == 72);
    // No chord held, or harmony off: nothing to bend.
    Rig idle; assert(!idle.engine.CanBend() && !idle.engine.HarmonyBend(0));
}

void Panel() {
    Rig rig;
    // The harmony page: menu open (TAPE's page), hold KEY_21 1 s.
    rig.hw.keys = uint64_t(1) << panel::kChompiKey; rig.Block();
    rig.hw.keys |= uint64_t(1) << panel::kFxBefore; for(int i = 0; i < 2001; ++i) rig.Block();
    rig.hw.keys &= ~(uint64_t(1) << panel::kFxBefore); rig.Block();
    assert(rig.panel.HarmonyPage() && rig.engine.Secret() == secret::Mode::Off);
    const secret::Mode order[] = {secret::Mode::BloomUp, secret::Mode::BloomDown, secret::Mode::BloomRandom, secret::Mode::TalkEcho,
                                  secret::Mode::TalkCall, secret::Mode::TalkSparkle, secret::Mode::TalkGroove, secret::Mode::TalkGroove};
    for(secret::Mode m : order) {                                                  // SW3: three clicks per step, stops at the end
        rig.hw.turns[panel::kKnobEncoder[3]] = 3; rig.Block(); assert(rig.engine.Secret() == m);
        assert(((rig.panel.KnobFlags() >> 19) & 7u) == static_cast<unsigned>(m) && ((rig.panel.KnobFlags() >> 7) & 1u));
    }
    Rgb rings[4]; ComposeHarmonyKnobLeds(Pack(rig.player.state), rings, rig.panel.KnobFlags());
    assert(rings[3].r == knobs::StepColour(7).r && rings[3].g == knobs::StepColour(7).g);   // Groove: the 8th step colour (blue)
    rig.hw.turns[panel::kKnobEncoder[3]] = -24; rig.Block(); assert(rig.engine.Secret() == secret::Mode::Off);
    // Close the menu and hold a chord (C4).
    rig.hw.keys = 0; for(int i = 0; i < 10; ++i) rig.Block();
    rig.engine.Note(60, 100, 2); rig.Run(240);
    const Parameters before = rig.engine.GetParameters();
    rig.hw.keys = (uint64_t(1) << panel::kKnobEncoder[1]) | (uint64_t(1) << panel::kKnobEncoder[3]);
    for(int i = 0; i < 2010; ++i) rig.Block();                                     // SW1 + SW3 held 1 s with a chord: Mischief
    assert(rig.player.Bent() && (rig.panel.MagicLights() & 3u) == 3u);
    assert(rig.engine.GetParameters().cutoff == before.cutoff);                     // the sound was not rolled
    rig.hw.keys = 0; rig.Block(); rig.Block(); assert(!rig.player.Bent());          // let go: home (knob releases land a block later)
    rig.engine.Note(60, 0, 2); rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    // Without a chord the same gesture still rolls a new sound.
    rig.hw.keys = (uint64_t(1) << panel::kKnobEncoder[1]) | (uint64_t(1) << panel::kKnobEncoder[3]);
    for(int i = 0; i < 2010; ++i) rig.Block();
    assert((rig.panel.MagicLights() & 3u) == 1u);
    rig.hw.keys = 0; rig.Block();
}
} // namespace

// 0.19.2 Dream (toggled): the sound picks the style; notes stay in the key and within the voices;
// chords move; SW4 + SW1 starts and stops it, a key or panic stops it; no nap while it plays.
void DreamShow() {
    Parameters pad; pad.version = 4; pad.synth = true; pad.voices = 4; pad.attack = .3f; pad.sustain = .8f; pad.release = .5f;
    Parameters pluck = pad; pluck.attack = .0045f; pluck.sustain = 0.f; pluck.release = .08f;
    Parameters kit = pad; kit.source = 1; kit.sample_mode = 1;
    Parameters loop = pluck; loop.source = 1; loop.sample_loop = true;
    assert(dream::StyleFor(pad) == dream::Style::Drone && dream::StyleFor(pluck) == dream::Style::Shimmer);
    assert(dream::StyleFor(kit) == dream::Style::Patter && dream::StyleFor(loop) == dream::Style::Drone);
    State key; key.tonic = 2; key.mode = Mode::Dorian;                              // D dorian
    for(const Parameters* p : {&pad, &pluck, &kit}) {
        magic::Rng r; dream::Show show; show.Start(*p, r);
        std::set<unsigned> colours; unsigned on = 0, sounding = 0, most = 0;
        auto note = [&](uint8_t n, uint8_t v) {
            if(v) { ++on; ++sounding; if(p != &kit) assert(InKey(n % 12, key)); assert(n >= 36 && n <= 108 && v <= 100); }
            else if(sounding) --sounding;
            most = std::max(most, sounding);
        };
        for(int i = 0; i < 60 * 2000; ++i) { show.Tick(24, key, r, note); if(i % 2000 == 0) colours.insert(show.Colour()); }   // 60 s
        assert(on >= 8 && most <= p->voices && colours.size() >= 3);
        show.Stop(note); assert(!show.Active() && sounding == 0 && show.KeyMask() == 0);
    }
    // The panel: SW4 + SW1 held 1 s starts it, held again stops it; a key stops it; panic stops it.
    Rig rig; assert(rig.engine.ApplyPatch(pad)); rig.player.state.enabled = false;
    auto combo = [&]() { rig.hw.keys = (uint64_t(1) << panel::kKnobEncoder[0]) | (uint64_t(1) << panel::kKnobEncoder[1]);
                         for(int i = 0; i < 2010; ++i) { rig.Block(); }
                         rig.hw.keys = 0; rig.Block(); rig.Block(); };
    combo(); assert(rig.panel.DreamPlaying() && ((rig.panel.DreamLights() >> 26) & 1u));
    unsigned voices = 0; for(int i = 0; i < 6000; ++i) { rig.Block(); voices = std::max(voices, rig.engine.ActiveVoices()); }
    assert(voices >= 2 && rig.panel.DreamLights() & 0x1FFFFFFu);                    // a 3-note drone chord, keys lit
    combo(); assert(!rig.panel.DreamPlaying()); rig.Run(48000 * 6); assert(rig.engine.ActiveVoices() == 0);
    combo(); assert(rig.panel.DreamPlaying());
    rig.hw.keys = uint64_t(1) << 15; rig.Block(); rig.hw.keys = 0; rig.Block();      // a white key
    assert(!rig.panel.DreamPlaying());
    rig.Run(48000 * 6); assert(rig.engine.ActiveVoices() == 0);
    combo(); assert(rig.panel.DreamPlaying());
    rig.hw.keys = (uint64_t(1) << panel::kKnobEncoder[0]) | (uint64_t(1) << panel::kKnobEncoder[3]);
    for(int i = 0; i < 2010; ++i) rig.Block();                                     // SW4 + SW3: panic
    rig.hw.keys = 0; rig.Block(); assert(!rig.panel.DreamPlaying() && rig.engine.ActiveVoices() == 0);
}

// 0.19.2: the start-up chime always sounds. Waking up as the pass-through (no instrument: no
// remembered preset) or on a sample that has not loaded, it plays on the built-in bell; on a synth
// sound it plays through the sound's own voices as before.
void ChimeAlwaysSounds() {
    auto chime = [](const Parameters& p, unsigned& voices) {
        Rig rig; assert(rig.engine.ApplyPatch(p));
        rig.panel.RequestChime();
        float peak = 0.f; voices = 0;
        for(int b = 0; b < 3000; ++b) {                                             // 1.5 s
            rig.panel.Block(rig.hw, rig.engine, rig.recorder, rig.sink); rig.engine.Block(24);
            for(int i = 0; i < 24; ++i) { float a, c; rig.engine.Process(0, 0, a, c); peak = std::max(peak, std::fabs(a)); }
            voices = std::max(voices, rig.engine.ActiveVoices());
        }
        return peak;
    };
    unsigned voices;
    Parameters through; through.version = 1; through.synth = false; through.mix = 0.f; through.level = 1.f;
    assert(chime(through, voices) > 0.02f && voices == 0);                          // the bell, not the synth
    Parameters unloaded; unloaded.version = 4; unloaded.synth = true; unloaded.source = 1; unloaded.sample_mode = 0; unloaded.level = 1.f; unloaded.mix = 0.f;
    assert(chime(unloaded, voices) > 0.02f && voices == 0);                         // empty sample slot: the bell
    Parameters synth; synth.version = 4; synth.synth = true; synth.voices = 4; synth.level = 1.f; synth.mix = 0.f;
    assert(chime(synth, voices) > 0.02f && voices >= 2);                            // the sound's own voices
}

// 0.20 split playing: the secret modes stay on the chord side (C3-B3); melody keys (C4-C5) just play.
void SplitChordSideOnly() {
    const uint32_t step = secret::Sixteenth(48000.f, 120.f);
    {   Rig rig; rig.engine.SetSecret(secret::Mode::BloomUp); rig.engine.SetSplit(true);
        rig.engine.Note(48, 100, 2); rig.Run(240); assert(rig.engine.ActiveVoices() == 1);   // the chord blooms
        const unsigned pending = rig.engine.Blooming().Pending(); assert(pending > 0);
        rig.engine.Note(64, 100, 2); rig.Run(240);                                     // the melody note sounds at once
        assert(rig.engine.ActiveVoices() == 2 && rig.engine.Blooming().Pending() == pending);
        rig.engine.Note(64, 0, 2); rig.engine.Note(48, 0, 2); rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    }
    {   Rig rig; rig.engine.SetSplit(true);                                            // Mischief bends chords only
        rig.engine.Note(64, 100, 2); rig.Run(240); assert(!rig.engine.CanBend() && !rig.engine.HarmonyBend(0));
        rig.engine.Note(48, 100, 2); rig.Run(240); assert(rig.engine.CanBend() && rig.engine.HarmonyBend(0));
        rig.engine.HarmonyUnbend(); rig.engine.Note(48, 0, 2); rig.Run(240); assert(rig.engine.ActiveVoices() == 1);   // the melody note still sounds
        rig.engine.Note(64, 0, 2); rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    }
    {   Rig rig; rig.engine.SetSecret(secret::Mode::TalkCall); rig.engine.SetSplit(true);
        secret::Conversation& talk = rig.engine.Talk();
        rig.engine.Note(64, 100, 2); rig.Run(480); rig.engine.Note(64, 0, 2); assert(!talk.Speaking());   // a melody note: no answer
        rig.engine.Note(48, 100, 2); rig.Run(4800); rig.engine.Note(48, 0, 2); assert(talk.Speaking());   // a chord: Choowi answers
        rig.Run(17 * step); assert(talk.Sounding() != 0);
        rig.engine.Note(67, 100, 2); assert(talk.Speaking());                         // a melody plays along with the answer
        rig.engine.Note(67, 0, 2);
        rig.engine.Note(50, 100, 2); assert(!talk.Speaking());                        // a chord key cuts it off
        rig.engine.Note(50, 0, 2); rig.engine.Panic();
    }
}

int main() {
    SplitChordSideOnly();
    BloomUnit(); BloomInEngine(); Conversations(); MotifFamilies(); Mischief(); Panel(); DreamShow(); ChimeAlwaysSounds();
    std::cout << "PASS: secret modes: bloom order/timing/cancel, conversations (bar pause, one voice, in key, cut off, 4 motif families), "
                 "mischief (5 bends, home, no stuck notes), panel (SW3 picks, SW1+SW3 bends with a chord, rolls without), Dream (styles, in key, voices, SW4+SW1, key / panic stop), the chime's bell fallback, split (chord side only)\n";
    return 0;
}
