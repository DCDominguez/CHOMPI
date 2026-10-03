// The panel controller (menus, record gesture, keys, knobs) driven like the
// audio callback does, including development-only injected events, the LED
// composition and the development opcodes 0A/0B. Built with FORGE_TEST_HOOKS.
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>
#include "../core/panel_controller.h"
#include "../core/runtime.h"

using namespace forge;

namespace {
struct Sink : PanelSink {
    std::vector<MenuAction> presets; std::vector<forge::SampleJob> jobs; int flashes = 0, failures = 0;
    bool accept = true;
    bool PresetAction(const MenuAction& a, const Parameters&) override { presets.push_back(a); return accept; }
    bool SampleJob(const forge::SampleJob& j) override { jobs.push_back(j); return accept; }
    void Flash(bool ok) override { ++flashes; if(!ok) ++failures; }
};
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<int16_t> rec = std::vector<int16_t>(2 * 48000);
    SampleTable table; Engine engine; Recorder recorder; PanelController panel; Sink sink; PanelInput hw;
    Rig() {
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetSamples(&table);
        recorder.Init(rec.data(), 48000, &table.slots[kRamSlot], 48000.f);
    }
    // One audio block: panel, then 24 samples (recording from the line input).
    void Block(float line = 0.f) {
        panel.Block(hw, engine, recorder, sink);
        for(int i = 0; i < 24; ++i) {
            float L, R; engine.Process(line, line, L, R);
            if(recorder.Recording()) { float a, b; recorder.Input(panel.Source(), 0, line, line, L, R, a, b); recorder.Write(a, b); }
        }
        hw.turns[0] = hw.turns[1] = hw.turns[2] = hw.turns[3] = hw.turns[4] = hw.turns[5] = 0; hw.tone_press = false;
    }
    void Inject(PanelEvent::Kind kind, uint8_t id, int8_t value) { PanelEvent e; e.kind = kind; e.id = id; e.value = value; panel.Inject(e); Block(); }
    void Key(uint8_t id, bool down) { Inject(PanelEvent::Kind::Key, id, down ? 1 : 0); }
    void Tap(uint8_t id) { Key(id, true); Key(id, false); }
};
const uint8_t kWhite[15] = {15, 8, 9, 10, 11, 16, 17, 18, 19, 20, 24, 25, 26, 27, 28};

void KeysKnobsAndOverrides() {
    Rig rig;
    Parameters synth; synth.version = 3; synth.synth = true; assert(rig.engine.ApplyPatch(synth));
    rig.Block();                                                   // first block: jack (unplugged) -> mic
    assert(rig.panel.Source() == RecordSource::Mic);
    rig.Key(kWhite[0], true); assert(rig.engine.ActiveVoices() == 1);          // virtual KEY_1 plays MIDI 48
    // A real key held at the same time is unaffected by virtual releases.
    rig.hw.keys |= uint64_t(1) << kWhite[2]; rig.Block(); assert(rig.engine.ActiveVoices() == 2);
    rig.Key(kWhite[0], false); rig.Block();
    rig.hw.keys = 0; rig.Block();
    for(int i = 0; i < 2000; ++i) rig.Block();
    assert(rig.engine.ActiveVoices() == 0);
    // Knob 1 is hardware encoder SW4 (index 3): a virtual turn moves the mix.
    const float mix = rig.engine.GetParameters().mix;
    rig.Inject(PanelEvent::Kind::Turn, 3, 20); assert(std::fabs(rig.engine.GetParameters().mix - (mix + 20 / 127.f)) < 1e-6f);
    rig.hw.turns[3] = -10; rig.Block(); assert(std::fabs(rig.engine.GetParameters().mix - (mix + 10 / 127.f)) < 1e-6f);
    rig.Inject(PanelEvent::Kind::Turn, 5, 30); assert(rig.engine.GetParameters().level > 0.4f);  // SW6 level
    // SW5 press panics; a held note stops.
    rig.Key(kWhite[5], true); assert(rig.engine.ActiveVoices() == 1);
    rig.Inject(PanelEvent::Kind::Press, 4, 0); assert(rig.engine.ActiveVoices() == 0);
    rig.Key(kWhite[5], false);
    // Jack override: line in; back to hardware (unplugged) -> mic.
    rig.Inject(PanelEvent::Kind::Jack, 0, 1); assert(rig.panel.Source() == RecordSource::Line && rig.panel.Overridden());
    rig.Inject(PanelEvent::Kind::Jack, 0, -1); assert(rig.panel.Source() == RecordSource::Mic);
    // Release clears every override.
    rig.Key(7, true); rig.Inject(PanelEvent::Kind::Toggle, 0, 1); assert(rig.panel.Overridden());
    rig.Inject(PanelEvent::Kind::Release, 0, 0); assert(!rig.panel.Overridden());
}

void MenuAndRecordingThroughTheController() {
    Rig rig;
    // Toggle up + CHOMPI opens the menu; a white key recalls preset 1/4.
    rig.Inject(PanelEvent::Kind::Toggle, 0, 1);
    rig.Key(panel::kChompiKey, true); assert(rig.panel.MenuPacked() & 1u);
    rig.Tap(kWhite[3]);
    assert(rig.sink.presets.size() == 1 && rig.sink.presets[0].kind == MenuAction::Kind::Recall && rig.sink.presets[0].slot == 3);
    // Samples page: kit, bank b; a white key selects the kit for the live patch.
    rig.Tap(panel::kPage); rig.Tap(panel::kKit); rig.Tap(panel::kKit); rig.Tap(kWhite[0]);
    const Parameters& p = rig.engine.GetParameters();
    assert(p.Sampler() && p.sample_mode == 1 && p.sample_bank == 1);
    assert(((rig.panel.MenuPacked() >> 21) & 1u) && ((rig.panel.MenuPacked() >> 22) & 1u));
    rig.Key(panel::kChompiKey, false); assert(!(rig.panel.MenuPacked() & 1u));
    // Record: toggle down, hold CHOMPI with line input, release -> chromatic slot 15.
    rig.Inject(PanelEvent::Kind::Jack, 0, 1);
    rig.Inject(PanelEvent::Kind::Toggle, 0, 0);
    rig.Key(panel::kChompiKey, true); assert(rig.recorder.Recording());
    for(int i = 0; i < 200; ++i) rig.Block(0.1f);
    rig.Key(panel::kChompiKey, false);
    assert(!rig.recorder.Recording() && rig.recorder.Length() > 4000);
    assert(rig.engine.GetParameters().sample_mode == 0 && rig.engine.GetParameters().sample_slot == kRamSlot);
    assert(std::fabs(rig.recorder.Peak() - 0.3f) < 0.01f);                       // line x3
    // Save the take into chromatic a2 from the samples page (locks it).
    rig.Inject(PanelEvent::Kind::Toggle, 0, 1);
    rig.Key(panel::kChompiKey, true); rig.Tap(panel::kChromatic); rig.Tap(panel::kSave); rig.Tap(kWhite[1]);
    rig.Key(panel::kChompiKey, false); rig.Key(panel::kChompiKey, true);
    assert(rig.sink.jobs.size() == 1 && rig.sink.jobs[0].kind == SampleJob::Kind::Save && rig.sink.jobs[0].slot == 1
           && rig.sink.jobs[0].frames == rig.recorder.Length() && rig.recorder.Locked());
    // A full queue unlocks the take and flashes red.
    rig.recorder.Unlock(); rig.sink.accept = false;
    rig.Tap(panel::kSave); rig.Tap(kWhite[2]); rig.Key(panel::kChompiKey, false); rig.Key(panel::kChompiKey, true);
    assert(!rig.recorder.Locked() && rig.sink.failures == 1);
    // Recording refused while a save holds the take: red flash, gesture cancelled.
    rig.recorder.Lock(); rig.Key(panel::kChompiKey, false);
    rig.Inject(PanelEvent::Kind::Toggle, 0, 0); rig.Key(panel::kChompiKey, true);
    assert(!rig.recorder.Recording() && rig.sink.failures == 2);
}

void LedComposition() {
    LedView v; Rgb keys[25], chompi;
    ComposeLeds(v, keys, chompi); assert(chompi.b > 0.f && chompi.r == 0.f && keys[0].r == 0.f);   // idle: dim blue, menu closed
    v.recording_now = true; ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f);
    v.recording_now = false; v.flash = 0; ComposeLeds(v, keys, chompi); assert(chompi.r > 0.f && chompi.g == 0.f);
    v.flash = -1; v.saving = true; v.slow_blink = true; ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f && chompi.b > 0.f);
    v.slow_blink = false; ComposeLeds(v, keys, chompi); assert(chompi.r == 0.f && chompi.b == 0.f);
    PresetMenu menu; menu.Update(true, true); v.menu = menu.Packed(); v.preset_card = true; v.preset_occupancy = 1;
    ComposeLeds(v, keys, chompi); assert(keys[panel::SlotLed(0)].r > 0.f);                       // presets page drawn
}

void DevelopmentOpcodes() {
    auto make = [](uint8_t op, std::vector<uint8_t> data) {
        std::vector<uint8_t> m(7 + data.size() + 1); Header(m.data(), op, 42);
        std::copy(data.begin(), data.end(), m.begin() + 7); m.back() = Checksum(m.data(), m.size() - 1); return m;
    };
    Request r;
    auto m = make(0x0a, {0, 15, 65}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Panel
                                             && r.panel_kind == 0 && r.panel_id == 15 && r.panel_value == 1);
    m = make(0x0a, {1, 3, 64 - 20}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.panel_value == -20);
    m = make(0x0a, {3, 0, 63}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.panel_value == -1);
    for(auto bad : std::vector<std::vector<uint8_t>>{{0, 40, 65}, {0, 1, 66}, {1, 6, 65}, {2, 3, 64}, {3, 0, 66}, {5, 1, 64}, {6, 0, 64}}) {
        m = make(0x0a, bad); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
    }
    m = make(0x0b, {1}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.kind == RequestKind::Probe && r.page == 1);
    m = make(0x0b, {2}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.page == 2);
    m = make(0x0b, {8}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
    uint8_t out[kMaxReply];
    assert(EncodePanelAck(42, out) == 9 && out[4] == 0x47 && Checksum(out, 9) == 0);
    ProbeState s; s.menu = 0x7fffffffu; s.flags = 63; s.voices = 7; s.record_ms = 87000; s.live = 0x1e3; s.flash_count = 200; s.flash_ok = 1;
    assert(EncodeProbeState(42, s, out) == 24 && Checksum(out, 24) == 0 && out[4] == 0x46 && out[8] == 0);
    uint32_t menu = 0; for(unsigned i = 0; i < 5; ++i) menu |= uint32_t(out[9 + i]) << (7 * i);
    assert(menu == 0x7fffffffu && out[15] == 7 && (Read14(out + 16) | (out[18] << 14)) == 87000 && Read14(out + 19) == 0x1e3);
    uint8_t leds[26][3]; for(unsigned i = 0; i < 26; ++i) for(unsigned c = 0; c < 3; ++c) leds[i][c] = static_cast<uint8_t>(i + c);
    assert(EncodeProbeLeds(42, leds, out) == 88 && out[8] == 1 && out[9 + 3 * 25 + 2] == 27 && Checksum(out, 88) == 0);
    assert(88 <= kMaxReply);
}
} // namespace

int main() {
    KeysKnobsAndOverrides(); MenuAndRecordingThroughTheController(); LedComposition(); DevelopmentOpcodes();
    std::cout << "PASS: panel controller keys/knobs/overrides, menu + recording via injection, LED composition, dev opcodes\n";
}
