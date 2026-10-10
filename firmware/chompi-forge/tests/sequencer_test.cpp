// Event recorder and projects (core/sequencer.h, core/sequence_store.h, firmware 0.15).
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../core/panel_controller.h"
#include "../core/runtime.h"
#include "../core/sequence_store.h"

using namespace forge;
using namespace forge::seq;

namespace {
constexpr float kRate = 48000.f;
constexpr unsigned kBlock = 24;

// Steps a recorder by whole ticks (the parts clock's count after each).
struct Clocked {
    Sequencer s; uint32_t clock = 0;
    std::vector<std::pair<uint32_t, Action>> out;
    void Ticks(unsigned n, bool running = true) {
        for(unsigned i = 0; i < n; ++i) {
            ++clock; Action a[16];
            const unsigned k = s.Advance(1, clock, running, a, 16);
            for(unsigned j = 0; j < k; ++j) out.push_back({clock, a[j]});
        }
    }
    std::vector<uint32_t> NoteOns(uint8_t note) const {
        std::vector<uint32_t> v; for(auto& e : out) if(e.second.kind == Note && e.second.a == note && e.second.b) v.push_back(e.first); return v;
    }
};

void RecordAndLoop() {
    Clocked c;
    c.Ticks(10);
    c.s.RecordKey(); assert(c.s.GetState() == Sequencer::State::Armed);
    c.s.RecordNote(60, 100);                               // armed, not recording yet: ignored
    c.Ticks(85); assert(c.s.GetState() == Sequencer::State::Armed);
    c.Ticks(1); assert(c.s.GetState() == Sequencer::State::Recording && c.s.Position() == 0);   // tick 96: a bar line
    c.s.RecordNote(60, 100); c.Ticks(12); c.s.RecordNote(60, 0);   // an eighth note on beat 1
    c.Ticks(36); c.s.RecordNote(64, 90);                             // tick 48: beat 3, held
    c.s.RecordParam(Parameter::Cutoff, .25f); c.s.RecordParam(Parameter::Cutoff, .5f);   // one value per tick: the last
    c.s.RecordParam(Parameter::Level, .9f);                          // volume: not recorded
    c.Ticks(10); c.s.RecordKey();                                     // close at the next bar line
    assert(c.s.GetState() == Sequencer::State::Recording);
    c.Ticks(37); assert(c.s.GetState() == Sequencer::State::Recording);
    c.Ticks(1); assert(c.s.GetState() == Sequencer::State::Playing && c.s.Length() == 96);
    // Held 64 got its end just before the loop end; 5 events: 60 on/off, cutoff, 64 on/off.
    assert(c.s.Count() == 5);
    const Event& last = c.s.Data().events[4]; assert(last.tick == 95 && last.a == 64 && last.b == 0);
    // Playback: two loops, each note at its recorded tick.
    c.out.clear(); const uint32_t start = c.clock;
    c.Ticks(192);
    const auto on60 = c.NoteOns(60), on64 = c.NoteOns(64);
    assert(on60.size() == 2 && on60[0] == start + 1 && on60[1] == start + 97);
    assert(on64.size() == 2 && on64[0] == start + 49);
    unsigned cutoffs = 0; for(auto& e : c.out) if(e.second.kind == Param) { assert(e.second.a == static_cast<uint8_t>(Parameter::Cutoff) && e.second.b == 8192); ++cutoffs; }
    assert(cutoffs == 2);
    // Overdub: a note added in the second half plays from the next pass.
    c.s.OverdubKey(); assert(c.s.Overdub() && c.s.Capturing());
    c.Ticks(70); c.s.RecordNote(67, 80); c.Ticks(5); c.s.RecordNote(67, 0);
    c.out.clear(); c.Ticks(96);
    assert(c.NoteOns(67).size() == 1 && c.s.Count() == 7);
    // Overdub ends with a key held: its end is written, playback never sticks.
    c.Ticks(10); c.s.RecordNote(72, 70); c.s.OverdubKey(); assert(!c.s.Overdub() && c.s.Count() == 9);
    c.out.clear(); c.Ticks(96);
    int held[128] = {}; for(auto& e : c.out) if(e.second.kind == Note) held[e.second.a] += e.second.b ? 1 : -1;
    for(int v : held) assert(v == 0 || v == 1);
    // Stop releases what sounds; play restarts from the first tick.
    c.Ticks(48); c.s.RecordKey(); assert(c.s.GetState() == Sequencer::State::Stopped);
    c.out.clear(); c.Ticks(2);
    for(auto& e : c.out) assert(e.second.kind == Note && e.second.b == 0);
    c.out.clear(); c.Ticks(50); assert(c.out.empty());
    c.s.RecordKey(); c.Ticks(1); assert(c.NoteOns(60).size() == 1);
    // A stopped clock pauses; panic stops and keeps the loop; clear empties it.
    const uint16_t pos = c.s.Position(); c.Ticks(5, false); assert(c.s.Position() == pos);
    c.s.Panic(); c.Ticks(1); assert(c.s.GetState() == Sequencer::State::Stopped && c.s.Count() == 9);
    c.s.Clear(); c.Ticks(1); assert(c.s.GetState() == Sequencer::State::Empty && c.s.Count() == 0 && c.s.Length() == 0);
    // An empty take leaves nothing; the eighth bar closes the take by itself.
    Clocked e; e.s.RecordKey(); e.Ticks(96); e.s.RecordKey(); e.Ticks(96); assert(e.s.GetState() == Sequencer::State::Empty);
    Clocked m; m.s.RecordKey(); m.Ticks(96); m.s.RecordNote(50, 60); m.Ticks(kMaxTicks + 10);
    assert(m.s.GetState() == Sequencer::State::Playing && m.s.Length() == kMaxTicks);
    // Panic while recording: the take closes on a whole bar.
    Clocked p; p.s.RecordKey(); p.Ticks(96); p.s.RecordNote(55, 60); p.Ticks(40); p.s.Panic();
    assert(p.s.GetState() == Sequencer::State::Stopped && p.s.Length() == 96);
    // Full: further events are dropped and counted.
    Clocked f; f.s.RecordKey(); f.Ticks(96);
    for(unsigned i = 0; i < kMaxEvents + 5; ++i) f.s.RecordNote(static_cast<uint8_t>(i % 2 ? 40 : 41), i % 4 < 2 ? 50 : 0);
    assert(f.s.Count() == kMaxEvents && f.s.Drops() >= 5);
    assert(!Recordable(Parameter::InputGain) && Recordable(Parameter::Speed) && Recordable(Parameter::Cutoff));
    auto balance = [](const Clocked& k) {
        int on[128] = {}; for(auto& e : k.out) if(e.second.kind == Note) on[e.second.a] += e.second.b ? 1 : -1;
        for(int v : on) if(v != 0) return false;
        return true;
    };
    // 0.15.1: 20 note-offs on one tick (16 actions a block): the rest go out next block.
    Clocked o; o.s.RecordKey(); o.Ticks(96);
    for(uint8_t i = 0; i < 20; ++i) { o.s.RecordNote(static_cast<uint8_t>(40 + i), 80); o.Ticks(1); }
    o.Ticks(10); for(uint8_t i = 0; i < 20; ++i) o.s.RecordNote(static_cast<uint8_t>(40 + i), 0);
    o.s.RecordKey(); while(o.s.GetState() == Sequencer::State::Recording) o.Ticks(1);
    o.out.clear(); o.Ticks(96); assert(balance(o) && o.NoteOns(59).size() == 1);
    // 0.15.1: a stopped clock (B4 / MIDI Stop) ends what the playback holds; it waits silent.
    Clocked h; h.s.RecordKey(); h.Ticks(96); h.s.RecordNote(52, 90); h.s.RecordKey();
    while(h.s.GetState() == Sequencer::State::Recording) h.Ticks(1);
    h.out.clear(); h.Ticks(10); assert(h.s.Sounding(52) && h.NoteOns(52).size() == 1);
    h.Ticks(1, false); assert(!h.s.Sounding(52) && balance(h));
    const size_t quiet = h.out.size(); h.Ticks(20, false); assert(h.out.size() == quiet);
    // 0.15.1: MIDI Start (Restart) ends the held note before the loop starts over.
    h.Ticks(90); assert(h.s.Sounding(52) && h.NoteOns(52).size() == 2); h.s.Restart(); h.Ticks(1);
    assert(h.s.Sounding(52) && h.NoteOns(52).size() == 3);                 // ended, then the first tick again
    int net = 0; for(auto& e : h.out) if(e.second.kind == Note) net += e.second.b ? 1 : -1; assert(net == 1);
}

void FileAndMailbox() {
    Clocked c; c.s.RecordKey(); c.Ticks(96);
    c.s.RecordNote(60, 100); c.s.RecordParam(Parameter::Resonance, .3f); c.Ticks(100); c.s.RecordNote(60, 0); c.s.RecordKey(); c.Ticks(100);
    static uint8_t file[kFileMax]; static Sequence back;
    const size_t n = EncodeFile(c.s.Data(), file);
    assert(n == kFileHeader + 6 * 3 + 2 && DecodeFile(file, n, back) && back.length == 192 && back.count == 3);
    for(unsigned i = 0; i < 3; ++i) assert(back.events[i].tick == c.s.Data().events[i].tick && back.events[i].b == c.s.Data().events[i].b);
    auto broken = [&](size_t at, uint8_t v) { std::vector<uint8_t> x(file, file + n); x[at] = v; return x; };
    assert(!DecodeFile(file, n - 1, back) && !DecodeFile(broken(0, 'X').data(), n, back) && !DecodeFile(broken(n - 1, file[n - 1] ^ 1).data(), n, back));
    auto resign = [&](std::vector<uint8_t> x) { const uint16_t crc = FileCrc(x.data(), x.size() - 2); x[x.size() - 2] = crc >> 8; x[x.size() - 1] = crc & 0xff; return x; };
    for(auto x : {resign(broken(4, 100)),                                      // length not whole bars
                  resign(broken(kFileHeader + 6 * 1, 150)),                   // ticks out of order (150 before 100)
                  resign(broken(kFileHeader + 6 * 1 + 3, static_cast<uint8_t>(Parameter::Level))),   // the volume is never recorded
                  resign(broken(kFileHeader + 4, 200))})                       // velocity > 127
        assert(!DecodeFile(x.data(), x.size(), back));
    // Mailbox: one handoff at a time.
    static Mailbox box; Sequencer other;
    assert(box.Export(c.s, 2, 5) && box.state == Mailbox::Exported && !box.Export(c.s, 2, 6) && !box.Claim());
    box.Done(); assert(box.Claim()); box.sequence.length = 96; box.sequence.count = 0; box.Loaded();
    assert(other.GetState() == Sequencer::State::Empty && box.Import(other) && box.state == Mailbox::Free);
    assert(!box.Import(other));
}

struct MemoryCard : Storage {
    std::map<std::string, std::vector<uint8_t>> files;
    bool Ready() override { return true; }
    bool Read(const char* path, uint8_t* buffer, size_t capacity, size_t& size) override {
        auto f = files.find(path); if(f == files.end() || f->second.size() >= capacity) return false;   // as FatFsStorage: a file filling the buffer is refused
        std::copy(f->second.begin(), f->second.end(), buffer); size = f->second.size(); return true;
    }
    bool Write(const char* path, const uint8_t* data, size_t size) override { files[path].assign(data, data + size); return true; }
    bool Remove(const char* path) override { files.erase(path); return true; }
};

void ProjectsOnTheCard() {
    MemoryCard card; static Mailbox box; static uint8_t buffer[kFileBuffer];
    Clocked c; c.s.RecordKey(); c.Ticks(96); c.s.RecordNote(62, 90); c.Ticks(30); c.s.RecordNote(62, 0); c.s.RecordKey(); c.Ticks(70);
    assert(SaveFile(card, box, 0, 0, buffer) == Error::Busy);                 // nothing exported: saved without a loop
    assert(box.Export(c.s, 1, 4) && SaveFile(card, box, 1, 4, buffer) == Error::None && card.files.count("FORGE/B2S05.FSQ"));
    assert(box.state == Mailbox::Free);
    Sequencer loaded; assert(LoadFile(card, box, 1, 4, buffer) && box.Import(loaded) && loaded.Count() == 2 && loaded.Length() == 96);
    assert(!LoadFile(card, box, 1, 5, buffer) && box.state == Mailbox::Free);   // no loop there: the current one stays
    assert(CopyFile(card, 1, 4, 7, 14, buffer) && card.files.count("FORGE/B8S15.FSQ"));
    assert(CopyFile(card, 3, 3, 7, 14, buffer) && !card.files.count("FORGE/B8S15.FSQ"));   // copying a slot without one
    card.files["FORGE/B2S06.FSQ"] = {1, 2, 3};
    assert(!LoadFile(card, box, 1, 5, buffer) && box.state == Mailbox::Free);   // a damaged file is ignored
    Sequencer empty; assert(box.Export(empty, 1, 4) && SaveFile(card, box, 1, 4, buffer) == Error::None && !card.files.count("FORGE/B2S05.FSQ"));
    assert(box.Export(c.s, 1, 4) && SaveFile(card, box, 1, 4, buffer, false) == Error::None && !card.files.count("FORGE/B2S05.FSQ"));
    assert(EraseFile(card, 1, 4));
    char path[20]; FilePath(0, 14, path); assert(std::string(path) == "FORGE/B1S15.FSQ");
    // 0.15.1: a full loop (kMaxEvents events: a kFileMax-byte file) saves, loads and copies.
    Clocked full; full.s.RecordKey(); full.Ticks(96);
    for(unsigned i = 0; i < kMaxEvents; ++i) { full.s.RecordNote(static_cast<uint8_t>(30 + i % 64), i % 128 < 64 ? 70 : 0); if(i % 4 == 3) full.Ticks(1); }
    full.s.RecordKey(); while(full.s.GetState() == Sequencer::State::Recording) full.Ticks(1);
    assert(full.s.Count() == kMaxEvents);
    assert(box.Export(full.s, 4, 4) && SaveFile(card, box, 4, 4, buffer) == Error::None && card.files["FORGE/B5S05.FSQ"].size() == kFileMax);
    Sequencer big; assert(LoadFile(card, box, 4, 4, buffer) && box.Import(big) && big.Count() == kMaxEvents);
    card.files["FORGE/B5S06.FSQ"] = {1};
    assert(CopyFile(card, 4, 4, 4, 5, buffer) && card.files["FORGE/B5S06.FSQ"].size() == kFileMax);
    // 0.15.1: a save that went without its loop (mailbox busy) removes the slot's older loop,
    // so a recall never pairs the new preset with it.
    assert(box.Claim()); assert(SaveFile(card, box, 4, 5, buffer) == Error::Busy && !card.files.count("FORGE/B5S06.FSQ")); box.Done();
    // 0.15.1: a host store never takes (or frees) a panel save's export, even of the same slot;
    // the panel's own save writes it afterwards.
    assert(box.Export(c.s, 4, 4, Mailbox::Panel));
    assert(SaveFile(card, box, 4, 4, buffer, true, Mailbox::Host) == Error::Busy && box.state == Mailbox::Exported
           && card.files["FORGE/B5S05.FSQ"].size() == kFileMax);                       // kept: the panel save writes it
    assert(SaveFile(card, box, 4, 4, buffer, true, Mailbox::Panel) == Error::None && box.state == Mailbox::Free
           && card.files["FORGE/B5S05.FSQ"].size() < kFileMax);
    assert(box.Export(c.s, 1, 1, Mailbox::Host) && SaveFile(card, box, 2, 2, buffer, true, Mailbox::Panel) == Error::Busy
           && box.state == Mailbox::Exported);                                         // another slot's export waits for its save
    assert(SaveFile(card, box, 1, 1, buffer, true, Mailbox::Host) == Error::None && box.state == Mailbox::Free);
}

struct Sink : PanelSink {
    std::vector<MenuAction> actions; bool full = false;
    bool PresetAction(const MenuAction& a, const Parameters&) override { if(full) return false; actions.push_back(a); return true; }
    bool SampleJob(const forge::SampleJob&) override { return true; }
    void Flash(bool) override {}
};
void EngineAndPanel() {
    std::vector<float> l(48002), r(48002), rv(Reverb::Required(48000));
    Engine e; assert(e.Init(kRate, l.data(), r.data(), l.size(), rv.data(), rv.size()));
    harmony::Player player; e.SetHarmony(&player);
    parts::Parts parts; parts.Init(kRate); e.SetParts(&parts);
    static Sequencer seq; static Mailbox box; e.SetSequencer(&seq, &box);
    Parameters synth; synth.version = 4; synth.synth = true; synth.voices = 7; synth.release = 0.f; synth.cutoff = .8f;
    assert(e.ApplyPatch(synth));
    std::vector<std::pair<uint32_t, MidiOut>> midi; uint32_t now = 0;
    auto run = [&](int blocks) {
        for(int b = 0; b < blocks; ++b) {
            e.Block(kBlock); now += kBlock;
            MidiOut m; while(parts.PopMidi(m)) if(m.status < 0xf0) midi.push_back({now, m});
            for(unsigned i = 0; i < kBlock; ++i) { float a, c; e.Process(0, 0, a, c); }
        }
    };
    // 120 BPM: a tick is 1000 samples; a bar 96000. Arm, wait for the bar, play two notes
    // (panel key and MIDI) and turn the cutoff, close after one bar.
    seq.RecordKey();
    while(seq.GetState() == Sequencer::State::Armed) run(1);
    e.Note(60, 100, 2); run(500); e.Note(60, 0, 2);
    e.Note(67, 90, 0); e.Apply({Parameter::Cutoff, .2f}); e.Apply({Parameter::Level, .1f}); run(500); e.Note(67, 0, 0);
    seq.RecordKey();
    while(seq.GetState() == Sequencer::State::Recording) run(1);
    assert(seq.GetState() == Sequencer::State::Playing && seq.Length() == 96 && seq.Count() == 5);
    // Playback: the notes come back at the same place in the bar (MIDI out on the keys' channel)
    // and the cutoff moves; the volume does not.
    e.Apply({Parameter::Cutoff, .9f}); e.Apply({Parameter::Level, .7f}); midi.clear();
    run(4000);                                                                        // one bar
    std::vector<uint8_t> ons; for(auto& x : midi) if(x.second.status == 0x90) ons.push_back(x.second.data1);
    assert((ons == std::vector<uint8_t>{60, 67}));
    assert(std::fabs(e.GetParameters().cutoff - .2f) < 1e-3f && std::fabs(e.GetParameters().level - .7f) < 1e-3f);
    // 0.15.1: a structural patch change silences the voices but the loop keeps playing.
    Parameters waves = synth; waves.waveform = synth.waveform == 0 ? 1 : 0; assert(e.ApplyPatch(waves));
    assert(seq.GetState() == Sequencer::State::Playing); midi.clear(); run(4000);
    ons.clear(); for(auto& x : midi) if(x.second.status == 0x90) ons.push_back(x.second.data1);
    assert((ons == std::vector<uint8_t>{60, 67}));
    assert(e.ApplyPatch(synth) && seq.GetState() == Sequencer::State::Playing);
    // Played through harmony and the arp like the keys: with the arp on, the recorded C4 cycles.
    parts.settings.pattern = parts::Pattern::Up; e.PartsChanged(); midi.clear(); run(4000);
    assert(parts.SetCount() >= 1);
    parts.settings.pattern = parts::Pattern::Off; e.PartsChanged(); e.Panic(); run(10);
    assert(seq.GetState() == Sequencer::State::Stopped && e.ActiveVoices() == 0 && seq.Count() == 5);
    // The panel: parts page F#4 plays it again, G#4 overdub, A#4 twice within 2 s clears.
    PanelController panel; Recorder recorder; Sink sink; PanelInput hw; hw.frames = 24; hw.toggle_up = true;
    std::vector<int16_t> rec(2 * 48000); SampleTable table; recorder.Init(rec.data(), 48000, &table.slots[kRamSlot], kRate);
    auto block = [&]() { panel.Block(hw, e, recorder, sink); e.Block(kBlock); for(auto& t : hw.turns) t = 0; };
    auto press = [&](uint8_t button) { hw.keys |= uint64_t(1) << button; block(); hw.keys &= ~(uint64_t(1) << button); block(); };
    auto key_of = [](uint8_t note) { for(uint8_t k = 0; k < panel::kButtons; ++k) if(panel::kKeyNotes[k] == note) return k; return uint8_t(255); };
    hw.keys = uint64_t(1) << panel::kChompiKey; block();
    hw.keys |= uint64_t(1) << panel::kFxBefore; for(int i = 0; i < 2001; ++i) block();
    hw.keys &= ~(uint64_t(1) << panel::kFxBefore); block();
    press(panel::kFxBefore); assert(panel.PartsPage());
    press(key_of(66)); assert(seq.GetState() == Sequencer::State::Playing);
    press(key_of(68)); assert(seq.Overdub());
    LedView v; v.menu = panel.MenuPacked(); v.parts = panel.PartsLights(); v.parts_clock = panel.PartsClock(); v.sequence = panel.SequenceLights();
    Rgb keys[25], chompi; ComposeLeds(v, keys, chompi);
    auto led = [&](uint8_t note) { const uint8_t k = key_of(note), slot = panel::KeyToSlot(k);
        return keys[slot != panel::kNoSlot ? panel::SlotLed(slot) : panel::BlackLed(k)]; };
    assert(led(66).g == 1.f && led(66).r == 0.f && led(68).r == 1.f && led(68).g > .9f);   // playing green, overdub yellow
    press(key_of(68)); press(key_of(70)); assert(seq.Count() == 5);                       // one press: armed, not cleared
    v.sequence = panel.SequenceLights(); ComposeLeds(v, keys, chompi); assert(led(70).r == 1.f);
    for(int i = 0; i < 4001; ++i) block();                                                  // > 2 s: the clear window closed
    press(key_of(70)); assert(seq.Count() == 5);
    press(key_of(70)); assert(seq.GetState() == Sequencer::State::Empty && seq.Count() == 0);
    // CHOMPI light in the menu position (menu closed): orange while recording.
    hw.keys = 0; block(); hw.toggle_up = false; block();
    assert(!panel.PartsPage() && !(panel.MenuPacked() & 1u));
    seq.RecordKey(); while(seq.GetState() == Sequencer::State::Armed) block();
    block();                                                                        // the lights follow a block later
    v = LedView{}; v.menu = panel.MenuPacked(); v.sequence = panel.SequenceLights(); v.parts_clock = panel.PartsClock();
    ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f && chompi.g > .5f && chompi.b < .3f);
    // A preset save on the panel hands the loop to the mailbox with it (a project).
    e.Note(62, 80, 2); block(); e.Note(62, 0, 2); seq.RecordKey(); while(seq.GetState() == Sequencer::State::Recording) block();
    assert(seq.GetState() == Sequencer::State::Playing && box.state == Mailbox::Free);
    assert(e.ExportSequence(2, 3) && box.state == Mailbox::Exported && box.bank == 2 && box.slot == 3 && box.sequence.count == 2);
    MemoryCard card; static uint8_t buffer[kFileBuffer];
    assert(SaveFile(card, box, 2, 3, buffer) == Error::None);
    seq.Clear(); assert(LoadFile(card, box, 2, 3, buffer));
    Request load; load.kind = RequestKind::SequenceLoad; Response unused;
    assert(!ExecuteRequest(load, e, unused) && seq.GetState() == Sequencer::State::Playing && seq.Count() == 2);
    // 0.15.1: a panel save whose action could not be queued holds no export (the mailbox
    // stays free for the next save); a queued one exports after queueing.
    auto slot_key = [](uint8_t slot) { for(uint8_t k = 0; k < panel::kButtons; ++k) if(panel::KeyToSlot(k) == slot) return k; return uint8_t(255); };
    auto save = [&]() {
        hw.toggle_up = true; hw.keys = uint64_t(1) << panel::kChompiKey; block();
        hw.keys |= uint64_t(1) << panel::kPage; for(int i = 0; i < 2001; ++i) block();   // KEY_22 held 1 s: presets page
        hw.keys &= ~(uint64_t(1) << panel::kPage); block();
        press(panel::kSave); hw.keys = 0; block();
        press(slot_key(9)); hw.keys = uint64_t(1) << panel::kChompiKey; block(); hw.keys = 0; block();
    };
    sink.actions.clear(); sink.full = true; save();
    assert(sink.actions.empty() && box.state == Mailbox::Free);
    sink.full = false; save();
    assert(sink.actions.size() == 1 && sink.actions[0].kind == MenuAction::Kind::Save && sink.actions[0].slot == 9
           && box.state == Mailbox::Exported && box.slot == 9 && box.owner == Mailbox::Panel);
    assert(SaveFile(card, box, sink.actions[0].bank, 9, buffer) == Error::None && box.state == Mailbox::Free);
    // 0.15.1: a host store exports only once its reply is queued (StoreQueued), as the host's.
    Request store; store.kind = RequestKind::Store; store.bank = 6; store.slot = 1; Response reply;
    assert(ExecuteRequest(store, e, reply) && box.state == Mailbox::Free);
    StoreQueued(reply, e); assert(box.state == Mailbox::Exported && box.owner == Mailbox::Host && box.bank == 6);
    assert(SaveFile(card, box, 6, 1, buffer, true, Mailbox::Host) == Error::None && card.files.count("FORGE/B7S02.FSQ"));
}
} // namespace

int main() {
    RecordAndLoop(); FileAndMailbox(); ProjectsOnTheCard(); EngineAndPanel();
    std::cout << "PASS: event recorder arm/bar start/close/8-bar limit/overdub/held-note ends/stop/play/panic/clear/full, "
                 "file format and rejections, mailbox, projects on the card (save/load/copy/erase/damaged), engine "
                 "(keys, MIDI, controls, volume excluded, arp, panic), parts page keys and lights, CHOMPI light, project save/recall\n";
}
