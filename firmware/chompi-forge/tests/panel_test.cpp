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
        hw.frames = 24;
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
    m = make(0x0b, {0}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);   // retired state page
    uint8_t out[kMaxReply];
    assert(EncodePanelAck(42, out) == 9 && out[4] == 0x47 && Checksum(out, 9) == 0);
    uint8_t leds[26][3]; for(unsigned i = 0; i < 26; ++i) for(unsigned c = 0; c < 3; ++c) leds[i][c] = static_cast<uint8_t>(i + c);
    assert(EncodeProbeLeds(42, leds, out) == 88 && out[8] == 1 && out[9 + 3 * 25 + 2] == 27 && Checksum(out, 88) == 0);
    assert(88 <= kMaxReply);
}
} // namespace

void LooperThroughThePanel() {
    Rig rig;
    std::vector<int16_t> memory(2 * 48000 * 4); Looper looper; looper.Init(memory.data(), 48000 * 4, 48000.f);
    rig.engine.SetLooper(&looper);
    rig.Block();
    // Armed by PLAY + LOOP on an empty looper; a keybed note starts the take.
    rig.hw.keys = (uint64_t(1) << panel::kPlayKey) | (uint64_t(1) << panel::kLoopKey); rig.Block();
    rig.hw.keys = 0; rig.Block();
    assert(looper.GetState() == Looper::State::Armed);
    Parameters synth; synth.version = 3; synth.synth = true; assert(rig.engine.ApplyPatch(synth));
    rig.Tap(kWhite[7]); assert(looper.GetState() == Looper::State::FirstTake);
    for(int i = 0; i < 1000; ++i) rig.Block();
    rig.Tap(panel::kPlayKey);                                        // PLAY ends the take: plain playback
    assert(looper.GetState() == Looper::State::Playing && !looper.Overdubbing() && looper.Length() > 20000);
    // SW5 with a loop: transport, not cutoff / panic.
    const float cutoff = rig.engine.GetParameters().cutoff;
    rig.hw.turns[panel::kToneEncoder] = 10; rig.Block();
    assert(looper.Speed() > 1.2f && rig.engine.GetParameters().cutoff == cutoff);
    rig.hw.tone_press = true; rig.Block();
    assert(looper.Speed() == 1.f && looper.GetState() == Looper::State::Playing);
    // Menu: PLAY / LOOP set the feedback, KEY_20 / KEY_21 place the effects; the loop keeps playing.
    rig.hw.toggle_up = true; rig.Key(panel::kChompiKey, true);
    rig.Tap(panel::kPlayKey); rig.Tap(panel::kPlayKey);
    assert(std::fabs(looper.Feedback() - 0.8f) < 1e-6f && looper.GetState() == Looper::State::Playing);
    rig.Tap(panel::kFxAfter); assert(!rig.engine.FxBeforeLoop());
    LedView v; v.menu = rig.panel.MenuPacked(); v.looper = PackLooper(looper, rig.engine.FxBeforeLoop());
    Rgb keys[25], chompi; ComposeLeds(v, keys, chompi);
    assert(keys[panel::BlackLed(panel::kFxAfter)].r > keys[panel::BlackLed(panel::kFxBefore)].r);
    rig.Tap(panel::kFxBefore); assert(rig.engine.FxBeforeLoop());
    rig.Key(panel::kChompiKey, false); rig.hw.toggle_up = false; rig.Block();
    // A patch change keeps the loop playing; panic pauses it (kept).
    Parameters other = synth; other.waveform = 2; assert(rig.engine.ApplyPatch(other));
    assert(looper.GetState() == Looper::State::Playing);
    rig.engine.Panic(); assert(looper.GetState() == Looper::State::Paused && looper.HasLoop());
    // MIDI: CC 26 press + release resumes; CC 24 sets the speed while a loop exists.
    MidiFramer parser; MidiFrame frame; Request request; Response response;
    auto cc = [&](uint8_t number, uint8_t value) {
        for(uint8_t b : {uint8_t(0xb0), number, value}) parser.Feed(b, frame);
        assert(TranslateChannel(frame, 0, request) == Ingress::Control); ExecuteRequest(request, rig.engine, response);
        rig.Block();
    };
    cc(26, 127); cc(26, 0); assert(looper.GetState() == Looper::State::Playing);
    cc(24, 127); assert(looper.Speed() == 2.f && rig.engine.GetParameters().cutoff == cutoff);
    cc(27, 127); cc(27, 0); assert(looper.Overdubbing());                       // CC 27 = LOOP: overdub
    Rgb play, loop; ComposeLooperLeds(PackLooper(looper, true), false, play, loop);
    assert(loop.r > 0.f && loop.g > 0.f && loop.b == 0.f);                       // yellow while overdubbing
    // Hold PLAY + LOOP 2 s: cleared; SW5 is the cutoff / panic again.
    rig.hw.keys = (uint64_t(1) << panel::kPlayKey) | (uint64_t(1) << panel::kLoopKey);
    for(int i = 0; i < 4100; ++i) rig.Block();
    rig.hw.keys = 0; rig.Block();
    assert(looper.Empty());
    rig.hw.turns[panel::kToneEncoder] = -10; rig.Block(); assert(rig.engine.GetParameters().cutoff < cutoff);
    ComposeLooperLeds(PackLooper(looper, true), false, play, loop);
    assert(play.r == 0.f && play.g == 0.f && loop.r == 0.f && loop.g == 0.f);   // empty: dark
}

// While the looper writes, sampler voices are capped (CPU budget): a held 7th
// voice is released, not cut, and new notes stay within the cap.
void LooperVoiceCap() {
    Rig rig;
    std::vector<int16_t> memory(2 * 48000 * 4); Looper looper; looper.Init(memory.data(), 48000 * 4, 48000.f);
    rig.engine.SetLooper(&looper);
    rig.recorder.Start(); for(int i = 0; i < 24000; ++i) rig.recorder.Write(0.3f, 0.3f); rig.recorder.Stop();
    Parameters p; p.version = 4; p.synth = true; p.voices = 7; p.sample_gate = true; p.sample_loop = true;
    p = SelectSample(p, 0, 0, kRamSlot); assert(rig.engine.ApplyPatch(p));
    for(uint8_t n = 0; n < 7; ++n) rig.engine.Note(static_cast<uint8_t>(48 + 2 * n), 100, 1);
    rig.Block(); assert(rig.engine.ActiveVoices() == 7);
    rig.Tap(panel::kLoopKey); assert(looper.GetState() == Looper::State::FirstTake && looper.Writing());
    rig.Block(); assert(rig.engine.ActiveVoices() == 7);                // the 7th releases (still sounding)
    for(int i = 0; i < 1000; ++i) rig.Block();                          // 0.5 s: the release has ended
    assert(rig.engine.ActiveVoices() == 6 && looper.Writing());
    rig.engine.Note(70, 100, 1); rig.Block(); assert(rig.engine.ActiveVoices() == 6);   // steals within the cap
    rig.Tap(panel::kPlayKey); assert(!looper.Writing());               // plain playback: all 7 again
    rig.engine.Note(71, 100, 1); rig.Block(); assert(rig.engine.ActiveVoices() == 7);
}

int main() {
    KeysKnobsAndOverrides(); MenuAndRecordingThroughTheController(); LedComposition(); DevelopmentOpcodes(); LooperThroughThePanel(); LooperVoiceCap();
    std::cout << "PASS: panel controller keys/knobs/overrides, menu + recording via injection, LED composition, dev opcodes, looper via panel/MIDI\n";
}
