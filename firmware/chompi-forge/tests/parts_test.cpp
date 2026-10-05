// Clock, arpeggiator and bass (core/parts.h, firmware 0.14).
#include <cassert>
#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <iostream>
#include <set>
#include <vector>
#include "../core/parts.h"
#include "../core/panel_controller.h"

using namespace forge;
using namespace forge::parts;

namespace {
constexpr float kRate = 48000.f;
constexpr unsigned kBlock = 24;

struct Note { uint32_t at; uint8_t note, velocity, source; };
struct Rig {
    Parts parts;
    uint32_t now = 0;
    std::vector<Note> synth;               // synth events in order
    std::vector<MidiOut> midi;
    explicit Rig(Settings s = Settings{}) { parts.settings = s; parts.Init(kRate); parts.Changed(); }
    void Run(float seconds) {
        const uint32_t blocks = static_cast<uint32_t>(seconds * kRate / kBlock);
        for(uint32_t b = 0; b < blocks; ++b) {
            Event out[Parts::kEvents];
            parts.Advance(kBlock);
            const unsigned n = parts.Take(out, Parts::kEvents);
            for(unsigned i = 0; i < n; ++i) synth.push_back({now, out[i].note, out[i].velocity, out[i].source});
            MidiOut m; while(parts.PopMidi(m)) midi.push_back(m);
            now += kBlock;
        }
    }
    std::vector<uint8_t> Ons() const { std::vector<uint8_t> v; for(auto& e : synth) if(e.velocity) v.push_back(e.note); return v; }
    unsigned Count(uint8_t status) const { unsigned n = 0; for(auto& m : midi) n += m.status == status; return n; }
    void Keys(std::initializer_list<uint8_t> notes, int root = -1, uint8_t fifth = 7) {
        std::vector<uint8_t> v(notes); parts.KeyDown(v.data(), static_cast<unsigned>(v.size()), root, fifth, 100, 2);
    }
    void Release(std::initializer_list<uint8_t> notes) { std::vector<uint8_t> v(notes); parts.KeyUp(v.data(), static_cast<unsigned>(v.size())); }
};
Settings Arp(Pattern p, Rate r = Rate::Sixteenth) { Settings s; s.pattern = p; s.rate = r; s.clock_out = false; return s; }

void Packing() {
    Settings s; s.pattern = Pattern::Random; s.rate = Rate::ThirtySecond; s.octaves = 4; s.gate = 20; s.latch = false;
    s.bass = Bass::Octave; s.bass_rate = BassRate::Eighth; s.bass_octave = 2; s.bpm = 300; s.seed = 2047; s.clock_out = false;
    assert(ValidWords(PackArp(s), PackClock(s)) && Unpack(PackArp(s), PackClock(s)) == s);
    assert(PackArp(Settings{}) == (1u << 3 | 9u << 8 | 1u << 13 | 2u << 17 | 1u << 19));
    assert(Unpack(PackArp(Settings{}), PackClock(Settings{})) == Settings{});
    assert(!ValidWords(6, PackClock(s)) && !ValidWords(6u << 3, PackClock(s)) && !ValidWords(20u << 8, PackClock(s))
           && !ValidWords(5u << 14, PackClock(s)) && !ValidWords(3u << 19, PackClock(s)) && !ValidWords(1u << 21, PackClock(s)));
    Settings slow = s; slow.bpm = 39; assert(!ValidWords(PackArp(s), PackClock(slow)));
    slow.bpm = 301; assert(!ValidWords(PackArp(s), PackClock(slow)));
}

void ClockTempoTapAndMidi() {
    Clock c; c.Init(kRate); c.SetTempo(120);
    unsigned ticks = 0; for(int b = 0; b < 2000; ++b) ticks += c.Advance(kBlock);           // 1 s
    assert(ticks == 48 && c.Ticks() == 48 && !c.External() && c.Running());
    c.SetTempo(10); assert(c.Tempo() == kMinBpm); c.SetTempo(999); assert(c.Tempo() == kMaxBpm);
    assert(c.Advance(48000 * 10) == 8);                                                     // bounded per block
    // Tap: four taps 0.5 s apart = 120 BPM; 0.4 s = 150; a pause starts over.
    Clock t; t.Init(kRate); t.SetTempo(90);
    assert(!t.Tap(0)); assert(t.Tap(24000) && t.Tempo() == 120); assert(t.Tap(48000) && t.Tempo() == 120);
    assert(!t.Tap(48000 + 200000)); assert(t.Tap(48000 + 200000 + 19200) && t.Tempo() == 150);
    assert(!t.Tap(48000 + 200000 + 19200 + 2000));                                          // too quick
    // MIDI clock at 100 BPM (1200 samples a tick): followed within a beat; stop / start; gone after 0.5 s.
    Clock m; m.Init(kRate); m.SetTempo(120);
    uint32_t next = 0, now = 0; unsigned got = 0;
    for(int b = 0; b < 4000; ++b) {
        while(now >= next) { m.Midi(Clock::Tick); next += 1200; }
        got += m.Advance(kBlock); now += kBlock;
    }
    assert(m.External() && std::fabs(m.Bpm() - 100.f) < 1.5f && got >= 80 && got <= 81);
    m.Midi(Clock::Stop); m.Midi(Clock::Tick); assert(m.Advance(kBlock) == 0 && !m.Running());
    m.Midi(Clock::Start); assert(m.Ticks() == 0 && m.Running());
    m.Midi(Clock::Tick); assert(m.Advance(kBlock) == 1 && m.Ticks() == 1);
    for(int b = 0; b < 1100; ++b) m.Advance(kBlock);                                        // 0.55 s silence
    assert(!m.External() && m.Running() && m.Bpm() == 120.f);
}

void ArpPatterns() {
    const std::vector<std::pair<Pattern, std::vector<uint8_t>>> cases = {
        {Pattern::Up, {60, 64, 67, 60, 64, 67, 60}}, {Pattern::Down, {67, 64, 60, 67, 64, 60, 67}},
        {Pattern::UpDown, {60, 64, 67, 64, 60, 64, 67}}, {Pattern::Order, {67, 60, 64, 67, 60, 64, 67}}};
    for(const auto& c : cases) {
        Rig rig(Arp(c.first)); rig.Keys({67, 60, 64});
        rig.Run(6 * 0.125f + 0.06f);                         // first at once, then every 1/16 (125 ms at 120)
        std::vector<uint8_t> ons = rig.Ons(); ons.resize(std::min<size_t>(ons.size(), 7));
        assert(ons == c.second);
    }
    // Octaves: Up over two octaves.
    Settings two = Arp(Pattern::Up); two.octaves = 2;
    Rig o(two); o.Keys({60, 64}); o.Run(4 * 0.125f + 0.06f);
    assert((o.Ons() == std::vector<uint8_t>{60, 64, 72, 76, 60}));
    // Random: the same seed and keys give the same notes; another seed differs; all from the set.
    auto random = [](uint16_t seed) { Settings s = Arp(Pattern::Random); s.seed = seed; Rig r(s); r.Keys({60, 62, 64, 65, 67}); r.Run(4.f); return r.Ons(); };
    const auto a = random(7), b = random(7), c = random(8);
    assert(a == b && a != c && a.size() >= 32);
    for(uint8_t n : a) assert(n == 60 || n == 62 || n == 64 || n == 65 || n == 67);
    // Rate: 1/4 at 120 = 2 steps a second; 1/8 triplet = 6.
    Rig q(Arp(Pattern::Up, Rate::Quarter)); q.Keys({60}); q.Run(2.01f); assert(q.Ons().size() == 5);
    Rig t(Arp(Pattern::Up, Rate::EighthTriplet)); t.Keys({60}); t.Run(1.01f); assert(t.Ons().size() == 7);
}

void GateLatchAndOwnership() {
    // Gate 50 % of a 1/16 (125 ms): each note ends ~62 ms after it starts.
    Rig g(Arp(Pattern::Up)); g.Keys({60, 64}); g.Run(0.5f);
    for(size_t i = 0; i + 1 < g.synth.size(); i += 2) {
        assert(g.synth[i].velocity && !g.synth[i + 1].velocity && g.synth[i].note == g.synth[i + 1].note);
        const uint32_t len = g.synth[i + 1].at - g.synth[i].at; assert(len >= 2952 && len <= 3048);
    }
    // Latch (default): released keys keep cycling; the next key after release replaces the set.
    Rig l(Arp(Pattern::Up)); l.Keys({60, 64}); l.Release({60}); l.Release({64});
    assert(l.parts.Latched() && l.parts.SetCount() == 2);
    l.Run(0.5f); assert(l.Ons().size() >= 4);
    l.Keys({70}); l.synth.clear(); l.Run(0.5f);
    for(uint8_t n : l.Ons()) assert(n == 70);
    // Unlatched: the arp follows the held keys and stops when none are held.
    Settings free = Arp(Pattern::Up); free.latch = false;
    Rig u(free); u.Keys({60, 64}); u.Run(0.3f); u.Release({64}); u.synth.clear(); u.Run(0.3f);
    for(uint8_t n : u.Ons()) assert(n == 60);
    u.Release({60}); u.Run(0.3f);
    assert(u.parts.SetCount() == 0 && u.parts.ArpNote() == 0 && !u.synth.back().velocity);
    // Panic: every owned note stops on the synth and MIDI.
    Settings both = Arp(Pattern::Up, Rate::Quarter); both.gate = 20; both.bass = Bass::Root; both.bass_rate = BassRate::Chord;
    Rig p(both); p.Keys({60, 64, 67}, 0); p.Run(0.1f);
    assert(p.parts.ArpNote() == 60 && p.parts.BassNote() == 36);
    p.parts.Clear(); p.Run(0.05f);
    int sounding[128] = {}; for(auto& e : p.synth) sounding[e.note] += e.velocity ? 1 : -1;
    for(int v : sounding) assert(v == 0);
    // Arp and bass on the same pitch: one synth note, ended only when both are done.
    Settings same = Arp(Pattern::Up, Rate::Quarter); same.bass = Bass::Root; same.bass_rate = BassRate::Chord; same.bass_octave = 2;
    Rig s(same); s.Keys({48}, 0); s.Run(0.1f);
    unsigned ons48 = 0; for(auto& e : s.synth) ons48 += e.note == 48 && e.velocity; assert(ons48 == 1);
    s.Run(0.4f);                                        // the arp note ends at its gate; the bass holds
    assert(s.parts.BassNote() == 48 && s.synth.back().velocity);
}

void BassModesAndMidi() {
    Settings b; b.bass = Bass::Alternate; b.bass_rate = BassRate::Quarter; b.bass_octave = 1; b.clock_out = true;
    Rig r(b); r.Keys({64, 67, 71}, 4, 7);                     // E minor, root E
    r.Run(2.01f);
    std::vector<uint8_t> bass; for(auto& m : r.midi) if(m.status == 0x91) bass.push_back(m.data1);
    assert((bass == std::vector<uint8_t>{40, 47, 40, 47, 40}));
    assert(r.Count(0xfa) == 1 && r.Count(0xf8) >= 95 && r.Count(0xf8) <= 97);          // 24 PPQN at 120
    for(auto& m : r.midi) assert(m.status != 0x90);           // no arp: channel 1 silent
    b.bass = Bass::Fifth; Rig f(b); f.Keys({59, 62, 65}, 11, 6);   // B diminished: the chord's own fifth
    f.Run(0.1f); assert(f.Count(0x91) == 2 && f.midi[1].data1 == 47 && f.midi[2].data1 == 53);
    b.bass = Bass::Octave; b.bass_octave = 0; Rig o(b); o.Keys({60}); o.Run(0.51f);
    std::vector<uint8_t> oct; for(auto& m : o.midi) if(m.status == 0x91) oct.push_back(m.data1);
    assert((oct == std::vector<uint8_t>{24, 36}));
    // Chord rate: one note per chord change, held until the next.
    b.bass = Bass::Root; b.bass_rate = BassRate::Chord; b.bass_octave = 1; Rig c(b);
    c.Keys({60}); c.Run(1.f); c.Release({60}); c.Keys({65}); c.Run(1.f);
    std::vector<uint8_t> roots; for(auto& m : c.midi) if(m.status == 0x91) roots.push_back(m.data1);
    assert((roots == std::vector<uint8_t>{36, 41}));
    // Stopping the parts stops the clock out.
    c.parts.settings.bass = Bass::Off; c.parts.Changed(); c.Run(0.1f);
    assert(c.Count(0xfc) == 1 && c.parts.BassNote() == 0);
    // Following MIDI clock: nothing sent; MIDI Start restarts the phrase.
    Settings a = Arp(Pattern::Up, Rate::Quarter); a.clock_out = true;
    Rig e(a); e.Keys({60, 64}); e.Run(0.01f);
    e.midi.clear(); e.synth.clear();
    e.parts.ClockMessage(Clock::Start);
    for(int i = 0; i < 48; ++i) { e.parts.ClockMessage(Clock::Tick); e.Run(0.0208f); }
    assert(e.parts.GetClock().External() && e.Count(0xf8) == 0);
    assert((e.Ons() == std::vector<uint8_t>{60, 64, 60}));
}
struct Sink : forge::PanelSink {
    bool PresetAction(const forge::MenuAction&, const forge::Parameters&) override { return true; }
    bool SampleJob(const forge::SampleJob&) override { return true; }
    void Flash(bool) override {}
};
void EngineAndPanel() {
    std::vector<float> l(48002), r(48002), rv(forge::Reverb::Required(48000));
    Engine e; assert(e.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
    harmony::Player player; e.SetHarmony(&player);
    Parts parts; parts.Init(kRate); e.SetParts(&parts);
    Parameters synth; synth.version = 4; synth.synth = true; synth.voices = 7; synth.release = 0.f;
    assert(e.ApplyPatch(synth));
    std::vector<uint8_t> arp_notes;
    auto run = [&](int blocks) {
        for(int b = 0; b < blocks; ++b) {
            e.Block(kBlock);
            if(parts.ArpNote() && (arp_notes.empty() || arp_notes.back() != parts.ArpNote())) arp_notes.push_back(parts.ArpNote());
            for(unsigned i = 0; i < kBlock; ++i) { float a, c; e.Process(0, 0, a, c); }
        }
    };
    auto midi = [&]() { std::vector<MidiOut> v; MidiOut m; while(parts.PopMidi(m)) if(m.status < 0xf0) v.push_back(m); return v; };
    // Arp on: a key goes to the arp (nothing returned for the panel's MIDI out); one voice at a time.
    parts.settings.pattern = Pattern::Up; parts.settings.rate = Rate::Sixteenth; e.PartsChanged();
    assert(e.ArpOn() && e.Note(60, 100, 2) == 0 && e.Note(64, 100, 2) == 0);
    run(2000);                                                                    // 1 s
    assert(e.ActiveVoices() <= 2 && arp_notes.size() >= 7);
    for(uint8_t n : arp_notes) assert(n == 60 || n == 64);
    // Latch: released keys keep playing; panic stops everything and sends MIDI note-offs.
    e.Note(60, 0, 2); e.Note(64, 0, 2); arp_notes.clear(); run(1000); assert(arp_notes.size() >= 3 && parts.Latched());
    midi(); e.Panic(); run(10);
    assert(e.ActiveVoices() == 0 && parts.SetCount() == 0 && parts.ArpNote() == 0);
    // Harmony + arp: C4 (Static) = the I chord, arpeggiated one note at a time.
    player.state.enabled = true; arp_notes.clear();
    assert(e.Note(60, 100, 2) == 0 && parts.SetCount() == 3);
    run(2000);
    std::set<unsigned> pcs; for(uint8_t n : arp_notes) pcs.insert(n % 12);
    assert((pcs == std::set<unsigned>{0, 4, 7}) && e.ActiveVoices() <= 1);
    e.Note(60, 0, 2); e.Panic(); run(10);
    // Bass only: the chord sounds directly, the bass adds its root (C2) on MIDI channel + 1.
    parts.settings.pattern = Pattern::Off; parts.settings.bass = Bass::Root; parts.settings.bass_rate = BassRate::Chord; e.PartsChanged();
    uint8_t played[16]; unsigned released = 0; midi();                          // (the panic's note-offs)
    assert(e.Note(60, 100, 2, played, &released) == 3); run(10);
    assert(e.ActiveVoices() == 4 && parts.BassNote() == 36);
    auto out = midi(); assert(out.size() == 1 && out[0].status == 0x91 && out[0].data1 == 36);
    assert(e.Note(60, 0, 2) == 3); run(400);
    assert(e.ActiveVoices() == 0 && parts.BassNote() == 0);
    // A note held before the arp is switched on still ends at its key-up (no stuck note).
    player.state.enabled = false; parts.settings.bass = Bass::Off; e.PartsChanged();
    assert(e.Note(67, 100, 1) == 1); run(5); assert(e.ActiveVoices() == 1);
    parts.settings.pattern = Pattern::Up; e.PartsChanged();
    assert(e.Note(67, 0, 1) == 0); run(400); assert(e.ActiveVoices() == 0);
    // Kit patches keep their pads: the parts stay out of the way.
    Parameters kit = synth; kit.source = 1; kit.sample_mode = 1; assert(e.ApplyPatch(kit) && !e.PartsOn());
    assert(e.ApplyPatch(synth)); e.Panic(); run(5);
    // The panel: TAPE's menu page, hold KEY_21 1 s = harmony page, KEY_21 = parts page.
    forge::PanelController panel; forge::Recorder recorder; Sink sink; forge::PanelInput hw; hw.frames = 24; hw.toggle_up = true;
    std::vector<int16_t> rec(2 * 48000); forge::SampleTable table; recorder.Init(rec.data(), 48000, &table.slots[forge::kRamSlot], 48000.f);
    auto block = [&]() { panel.Block(hw, e, recorder, sink); e.Block(kBlock); for(auto& t : hw.turns) t = 0; };
    auto press = [&](uint8_t button) { hw.keys |= uint64_t(1) << button; block(); hw.keys &= ~(uint64_t(1) << button); block(); };
    auto key_of = [](uint8_t note) { for(uint8_t k = 0; k < forge::panel::kButtons; ++k) if(forge::panel::kKeyNotes[k] == note) return k; return uint8_t(255); };
    parts.settings = Settings{}; e.PartsChanged(); player.state = harmony::State{};
    hw.keys = uint64_t(1) << forge::panel::kChompiKey; block();
    hw.keys |= uint64_t(1) << forge::panel::kFxBefore; for(int i = 0; i < 2001; ++i) block();
    hw.keys &= ~(uint64_t(1) << forge::panel::kFxBefore); block();
    assert(panel.HarmonyPage());
    const uint8_t tonic = player.state.tonic;
    press(forge::panel::kFxBefore); assert(panel.PartsPage() && ((panel.MenuPacked() >> 1) & 7u) == 6u);
    press(key_of(53)); press(key_of(65)); press(key_of(56)); press(key_of(59));   // F3 up-down, F4 1/16, G#3 alternate, B3 latch off
    press(forge::panel::kKnobEncoder[3]);                                            // SW3 press: bass octave C2 -> C3
    assert(parts.settings.pattern == Pattern::UpDown && parts.settings.rate == Rate::Sixteenth && parts.settings.bass == Bass::Alternate
           && parts.settings.bass_octave == 2 && !parts.settings.latch && e.ActiveVoices() == 0);
    hw.turns[forge::panel::kKnobEncoder[0]] = 5; block(); assert(parts.settings.bpm == 125 && parts.GetClock().Tempo() == 125);
    hw.turns[forge::panel::kKnobEncoder[1]] = 3; block(); assert(parts.settings.octaves == 2);
    hw.turns[forge::panel::kKnobEncoder[2]] = -3; block(); assert(parts.settings.gate == 7);
    hw.turns[forge::panel::kKnobEncoder[3]] = 3; block(); assert(parts.settings.bass_rate == BassRate::Eighth);
    // Tap tempo on C5: three taps 0.4 s apart = 150 BPM.
    for(int t = 0; t < 3; ++t) { press(key_of(72)); for(int i = 0; i < 798; ++i) block(); }
    assert(parts.settings.bpm == 150 && parts.GetClock().Tempo() == 150);
    // B4 stops and restarts the clock.
    press(key_of(71)); assert(!parts.GetClock().Running()); press(key_of(71)); assert(parts.GetClock().Running());
    // Lights: the chosen keys lit in their colours.
    forge::LedView v; v.menu = panel.MenuPacked(); v.parts = panel.PartsLights(); v.parts_clock = panel.PartsClock();
    forge::Rgb keys[25], chompi; forge::ComposeLeds(v, keys, chompi);
    auto led = [&](uint8_t note) {
        const uint8_t k = key_of(note), slot = forge::panel::KeyToSlot(k);
        return keys[slot != forge::panel::kNoSlot ? forge::panel::SlotLed(slot) : forge::panel::BlackLed(k)];
    };
    assert(led(53).b == 1.f && led(65).g == 1.f && led(56).r == 1.f && led(59).r < .1f && led(71).g == 1.f);
    assert(led(66).r < .1f && led(68).r < .1f && led(70).r == 0.f);                  // recorder empty: F#4 / G#4 dim, A#4 dark
    assert(led(48).b == 0.f && led(60).g == 0.f);
    forge::Rgb rings[4]; forge::ComposePartsKnobLeds(v.parts, v.parts_clock, rings);
    assert(rings[1].g == 1.f && rings[1].r == 0.f && rings[3].r == 1.f && rings[3].g > .9f);   // 2 octaves green, 1/8 yellow
    // KEY_21 back to the harmony page (the tonic unchanged), KEY_22 back to TAPE's page.
    press(forge::panel::kFxBefore); assert(panel.HarmonyPage() && player.state.tonic == tonic);
    press(forge::panel::kPage); assert(!panel.HarmonyPage() && !panel.PartsPage() && (panel.MenuPacked() >> 21) & 1u);
}
} // namespace

int main() {
    Packing(); ClockTempoTapAndMidi(); ArpPatterns(); GateLatchAndOwnership(); BassModesAndMidi(); EngineAndPanel();
    std::cout << "PASS: parts packing, clock (tempo, tap, MIDI follow/stop/start/timeout), arp patterns/octaves/"
                 "rates/seeded random, gate, latch, panic, shared-pitch ownership, bass modes/rates, MIDI out and clock out, engine (arp, harmony + arp, bass with chords, latch, panic, kit), parts page (keys, knobs, tap, run/stop, lights)\n";
}
