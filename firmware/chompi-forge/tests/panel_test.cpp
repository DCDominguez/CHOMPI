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
        hw.turns[0] = hw.turns[1] = hw.turns[2] = hw.turns[3] = hw.turns[4] = hw.turns[5] = 0;
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
    // Knob 1 is hardware encoder SW4 (index 3): page 1 is TAPE's pitch, .003 per click.
    const float speed = rig.engine.GetPerformance().speed;
    rig.Inject(PanelEvent::Kind::Turn, 3, 20); assert(std::fabs(rig.engine.GetPerformance().speed - (speed + 20 * .003f)) < 1e-5f);
    rig.hw.turns[3] = -10; rig.Block(); assert(std::fabs(rig.engine.GetPerformance().speed - (speed + 10 * .003f)) < 1e-5f);
    rig.Inject(PanelEvent::Kind::Turn, 5, 10); assert(std::fabs(rig.engine.GetParameters().level - .55f) < 1e-5f);  // SW6: .03 per click
    // Panic: SW4 + SW3 held together 1 s; a held note stops; neither knob changes page.
    rig.Key(kWhite[5], true); assert(rig.engine.ActiveVoices() == 1);
    rig.Key(panel::kKnobEncoder[0], true); rig.Key(panel::kKnobEncoder[3], true);
    for(int i = 0; i < 1990; ++i) rig.Block();
    assert(rig.engine.ActiveVoices() == 1);
    for(int i = 0; i < 20; ++i) rig.Block();
    assert(rig.engine.ActiveVoices() == 0);
    rig.Key(panel::kKnobEncoder[0], false); rig.Key(panel::kKnobEncoder[3], false); assert(rig.panel.KnobPages() == 0);
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
    assert((rig.panel.MenuPacked() >> 21) & 1u);                                   // 0.10: TAPE's page first
    rig.Key(panel::kPage, true); for(int i = 0; i < 2000; ++i) rig.Block();         // hold KEY_22 1 s: Forge presets
    rig.Key(panel::kPage, false);
    assert(!((rig.panel.MenuPacked() >> 21) & 1u) && rig.engine.FxBeforeLoop());   // the hold did not move the effects
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
    // Count-in: 1.5 s of blinking first; letting go early records nothing.
    rig.Key(panel::kChompiKey, true); assert(!rig.recorder.Recording() && rig.panel.CountIn() == 1);
    for(int i = 0; i < 1000; ++i) rig.Block(0.1f);
    assert(!rig.recorder.Recording() && rig.panel.CountIn() == 3);
    rig.Key(panel::kChompiKey, false); assert(rig.panel.CountIn() == 0 && !rig.recorder.Recording() && rig.recorder.Length() == 0);
    rig.Key(panel::kChompiKey, true);
    for(int i = 0; i < 2999; ++i) rig.Block(0.1f);                            // 3,000 blocks of 24 = 1.5 s
    assert(!rig.recorder.Recording());
    rig.Block(0.1f); assert(rig.recorder.Recording() && rig.panel.CountIn() == 0);
    for(int i = 0; i < 200; ++i) rig.Block(0.1f);
    rig.Key(panel::kChompiKey, false);
    assert(!rig.recorder.Recording() && rig.recorder.Length() > 4000);
    assert(rig.engine.GetParameters().sample_mode == 0 && rig.engine.GetParameters().sample_slot == kRamSlot);
    assert(std::fabs(rig.recorder.Peak() - 0.225f) < 0.01f);                     // line x3 x input gain .75 (TAPE)
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
    for(int i = 0; i < 3000; ++i) rig.Block();
    assert(!rig.recorder.Recording() && rig.sink.failures == 2);
}

void LedComposition() {
    LedView v; Rgb keys[25], chompi;
    ComposeLeds(v, keys, chompi); assert(chompi.b == 0.f && chompi.r == 0.f && keys[0].r == 0.f);   // idle in the menu position: off (TAPE)
    v.recording_now = true; ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f);
    v.recording_now = false; v.flash = 0; ComposeLeds(v, keys, chompi); assert(chompi.r > 0.f && chompi.g == 0.f);
    v.flash = -1; v.saving = true; v.slow_blink = true; ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f && chompi.b > 0.f);
    v.slow_blink = false; ComposeLeds(v, keys, chompi); assert(chompi.r == 0.f && chompi.b == 0.f);
    // TAPE: record position = input meter (dim white at silence, green with signal); menu
    // position = off, purple while CHOMPI is held. Count-in: CHOMPI and white keys blink red.
    v.saving = false; v.record_position = true; ComposeLeds(v, keys, chompi); assert(chompi.r == .1f && chompi.g == .1f && chompi.b == .1f);
    v.input_level = .3f; ComposeLeds(v, keys, chompi); assert(chompi.g > chompi.r && chompi.g > .5f);
    v.record_position = false; ComposeLeds(v, keys, chompi); assert(chompi.r == 0.f && chompi.g == 0.f && chompi.b == 0.f);
    v.keys_down = uint64_t(1) << panel::kChompiKey; ComposeLeds(v, keys, chompi); assert(chompi.b == 1.f && chompi.r > .5f);
    v.keys_down = 0; v.record_position = true;
    v.count_in = 1; ComposeLeds(v, keys, chompi); assert(chompi.r == 1.f && keys[panel::SlotLed(0)].r == 1.f && keys[panel::SlotLed(14)].g == 0.f);
    v.count_in = 2; ComposeLeds(v, keys, chompi); assert(chompi.r == 0.f && keys[panel::SlotLed(0)].r == 0.f);
    v.count_in = 0; v.record_position = false; v.input_level = 0.f;
    // SW5's lights: playing at 1x lights the forward LED only; reverse lights LED 5; dimmed in the record position.
    Rgb rev, fwd; ComposeTransportLeds(true, 1.f, false, rev, fwd); assert(fwd.g > .5f && rev.r == 0.f && rev.g == 0.f);
    ComposeTransportLeds(true, -1.f, false, rev, fwd); assert(rev.g > .5f && fwd.g == 0.f);
    ComposeTransportLeds(true, 1.f, true, rev, fwd); assert(fwd.g < .75f);
    ComposeTransportLeds(false, 1.f, false, rev, fwd); assert(fwd.g == 0.f && rev.g == 0.f);
    PresetMenu menu; menu.Update(true, true); v.menu = menu.Packed(); v.preset_card = true; v.preset_occupancy = 1;
    ComposeLeds(v, keys, chompi); assert(keys[panel::SlotLed(0)].r > 0.f);                       // presets page drawn
    // Menu closed (TAPE NormalPage): held keys light white; nothing else without a sampler patch.
    LedView play; play.keys_down = (uint64_t(1) << 18) | (uint64_t(1) << 7);   // KEY_8 (C4, slot 7), KEY_16 (black)
    ComposeLeds(play, keys, chompi);
    for(unsigned i = 0; i < 25; ++i) {
        const bool lit = i == panel::SlotLed(7) || i == panel::BlackLed(7);
        assert(lit ? (keys[i].r == 1.f && keys[i].g == 1.f && keys[i].b == 1.f) : (keys[i].r == 0.f && keys[i].g == 0.f && keys[i].b == 0.f));
    }
    play.keys_down = uint64_t(1) << panel::kChompiKey;                 // CHOMPI, toggle, knobs: no key light
    ComposeLeds(play, keys, chompi); for(const Rgb& k : keys) assert(k.r == 0.f && k.g == 0.f && k.b == 0.f);
    // Kit bank b: its occupied slots dim in bank b's colour, the recording key dim pink; a held key is white.
    play.live = 1u | (1u << 1) | (1u << 2); play.kit_occupancy = 0b101; play.recording_present = true;
    play.keys_down = uint64_t(1) << 8;                                 // KEY_2 = slot 1 (empty)
    ComposeLeds(play, keys, chompi);
    const Rgb b = SampleBankColour(1);
    assert(std::fabs(keys[panel::SlotLed(0)].r - b.r * .25f) < 1e-6f && std::fabs(keys[panel::SlotLed(2)].g - b.g * .25f) < 1e-6f);
    assert(keys[panel::SlotLed(1)].r == 1.f && keys[panel::SlotLed(3)].r == 0.f);
    assert(keys[panel::SlotLed(kRamSlot)].r > 0.f && keys[panel::SlotLed(kRamSlot)].b > keys[panel::SlotLed(kRamSlot)].g);
    // Chromatic bank a: C3, C4, C5 marked; playing the recording marks them pink.
    play.live = 1u; play.keys_down = 0; ComposeLeds(play, keys, chompi);
    for(uint8_t s = 0; s < 15; ++s) assert((keys[panel::SlotLed(s)].r > 0.f) == (s == 0 || s == 7 || s == 14));
    play.live = 1u | (uint32_t(kRamSlot) << 5); ComposeLeds(play, keys, chompi);
    assert(keys[panel::SlotLed(7)].b > keys[panel::SlotLed(7)].g);
    // The controller publishes what is held.
    Rig rig; rig.hw.keys = uint64_t(1) << 18; rig.Block(); assert(rig.panel.KeysDown() == uint32_t(1) << 18);
    rig.hw.keys = 0; rig.Block(); assert(rig.panel.KeysDown() == 0);
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
    m = make(0x0b, {8}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.page == 8);   // harmony (0.13)
    m = make(0x0b, {9}); assert(DecodeRequest(m.data(), m.size(), r) == Error::None && r.page == 9);   // parts (0.14)
    m = make(0x0b, {10}); assert(DecodeRequest(m.data(), m.size(), r) == Error::Patch);
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
    // SW5: a plain turn is always cutoff; push and turn is the loop speed (.012 per click, TAPE);
    // a click without turning puts it back to 1x.
    const float cutoff = rig.engine.GetParameters().cutoff;
    rig.hw.turns[panel::kToneEncoder] = 10; rig.Block();
    assert(std::fabs(rig.engine.GetParameters().cutoff - (cutoff + .1f)) < 1e-5f);
    for(int i = 0; i < 2000; ++i) rig.Block();
    assert(std::fabs(looper.Speed() - 1.f) < 1e-4f);
    rig.hw.tone_down = true; rig.hw.turns[panel::kToneEncoder] = 10; rig.Block();
    rig.hw.tone_down = false; rig.Block();
    for(int i = 0; i < 2000; ++i) rig.Block();
    assert(std::fabs(looper.Speed() - 1.12f) < 1e-3f && std::fabs(rig.engine.GetParameters().cutoff - (cutoff + .1f)) < 1e-5f);
    rig.Inject(PanelEvent::Kind::Press, 4, 0); rig.Block(); rig.Block();
    for(int i = 0; i < 2000; ++i) rig.Block();
    assert(std::fabs(looper.Speed() - 1.f) < 1e-3f && looper.GetState() == Looper::State::Playing);
    // Menu: PLAY / LOOP set the feedback, KEY_20 / KEY_21 place the effects; the loop keeps playing.
    rig.hw.toggle_up = true; rig.Key(panel::kChompiKey, true);
    rig.Tap(panel::kPlayKey); rig.Tap(panel::kPlayKey);
    assert(std::fabs(looper.Feedback() - 0.8f) < 1e-6f && looper.GetState() == Looper::State::Playing);
    rig.Tap(panel::kPage); assert(!rig.engine.FxBeforeLoop());                    // TAPE's page: KEY_22 = effects after
    rig.Tap(panel::kFxBefore); assert(rig.engine.FxBeforeLoop());                 // KEY_21 = before
    rig.Key(panel::kPage, true); for(int i = 0; i < 2000; ++i) rig.Block(); rig.Key(panel::kPage, false);   // Forge presets page
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

// Samples page: COPY, then LOOP as the source, a white key, CHOMPI: a locked
// save-from-loop job; while locked, LOOP cannot overdub.
void LooperSaveGesture() {
    Rig rig;
    std::vector<int16_t> memory(2 * 48000 * 4); Looper looper; looper.Init(memory.data(), 48000 * 4, 48000.f);
    rig.engine.SetLooper(&looper);
    rig.Block();
    rig.Tap(panel::kLoopKey); for(int i = 0; i < 400; ++i) rig.Block(0.2f); rig.Tap(panel::kPlayKey);
    assert(looper.HasLoop() && !looper.Writing());
    rig.hw.toggle_up = true; rig.Key(panel::kChompiKey, true);
    rig.Tap(panel::kPage); rig.Tap(panel::kCopy); rig.Key(panel::kChompiKey, false);
    const float feedback = looper.Feedback();
    rig.Tap(panel::kLoopKey);                                            // the loop is the source, not feedback +10 %
    assert(rig.panel.Menu().LoopSource() && looper.Feedback() == feedback && (rig.panel.MenuPacked() >> 31));
    rig.Tap(kWhite[4]); rig.Tap(panel::kChompiKey);                      // destination kit/chromatic slot 5, confirm
    assert(rig.sink.jobs.size() == 1);
    const forge::SampleJob& job = rig.sink.jobs[0];
    assert(job.kind == SampleJob::Kind::Save && job.from_loop && job.slot == 4 && job.frames == looper.Length() && job.gain == 1.f);
    assert(looper.Locked() && !rig.panel.Menu().LoopSource());
    rig.hw.toggle_up = false; rig.Block();
    rig.Tap(panel::kLoopKey); assert(!looper.Overdubbing());             // locked: no overdub
    looper.Clear(); assert(looper.HasLoop());                            // ...and no clear
    looper.Unlock(); rig.Tap(panel::kLoopKey); assert(looper.Overdubbing());
    // An empty looper cannot be saved: the job is refused with a red flash.
    Rig empty; Looper idle; std::vector<int16_t> m(2 * 4800); idle.Init(m.data(), 4800, 48000.f); empty.engine.SetLooper(&idle);
    empty.Block(); empty.hw.toggle_up = true; empty.Key(panel::kChompiKey, true);
    empty.Tap(panel::kPage); empty.Tap(panel::kCopy); empty.Key(panel::kChompiKey, false);
    empty.Tap(panel::kLoopKey); empty.Tap(kWhite[4]); empty.Tap(panel::kChompiKey);
    assert(empty.sink.jobs.empty() && empty.sink.failures == 1);
}

// Knob pages (docs/forge/KNOBS.md, core/knob_layout.h): TAPE's pages first, Forge's
// extra controls after, the patch's own knob last; a press steps the page on release,
// a 1.5 s hold resets the control; TAPE's step sizes.
void KnobPages() {
    Rig rig;
    Parameters synth; synth.version = 3; synth.synth = true; synth.cutoff = 0.5f; assert(rig.engine.ApplyPatch(synth));
    rig.Block();
    auto turn = [&](unsigned knob, int amount) { rig.Inject(PanelEvent::Kind::Turn, panel::kKnobEncoder[knob], static_cast<int8_t>(amount)); };
    auto press = [&](unsigned knob) { rig.Tap(panel::kKnobEncoder[knob]); };
    auto page = [&](unsigned knob) { return (rig.panel.KnobPages() >> (3 * knob)) & 7u; };
    const Parameters& p = rig.engine.GetParameters(); const Performance& perf = rig.engine.GetPerformance();
    auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
    // SW4: pitch (.003), gain (.01), resonance, filter envelope; 4 pages.
    turn(0, 10); assert(near(perf.speed, .83f + .03f) && rig.panel.KnobPages() == 0);
    rig.Key(panel::kKnobEncoder[0], true); assert(page(0) == 0);              // press: nothing yet
    rig.Key(panel::kKnobEncoder[0], false); assert(page(0) == 1);             // release: next page (TAPE)
    turn(0, 10); assert(near(perf.voice_gain, .704f + .1f));
    press(0); turn(0, 20); assert(near(p.resonance, .2f));
    press(0); turn(0, -10); assert(near(p.filter_amount, .4f));
    press(0); assert(page(0) == 0);                                           // wraps
    // SW1 (synth): attack, decay, LFO rate; .03 per click.
    turn(1, 10); assert(near(p.attack, 0.0045f + .3f));
    press(1); turn(1, 10); assert(near(p.decay, 0.1f + .3f));
    press(1); turn(1, 10); assert(near(p.lfo_rate, 0.5f + .3f));
    press(1); assert(page(1) == 0);
    // SW2 (synth): release, sustain, LFO filter, osc 2 detune.
    turn(2, 5); assert(near(p.release, 0.08f + .15f));
    press(2); turn(2, -5); assert(near(p.sustain, .6f - .15f));
    press(2); turn(2, 5); assert(near(p.lfo_filter, .15f));
    press(2); turn(2, -5); assert(near(p.osc2_detune, .5f - .15f));
    press(2); assert(page(2) == 0);
    // SW3: space (delay + reverb), saturation, DJ filter.
    turn(3, 10); assert(near(p.feedback, .25f + .3f) && near(p.mix, .5f * .55f) && near(p.reverb_mix, .55f));
    press(3); turn(3, 10); assert(near(perf.saturation, .3f));
    press(3); turn(3, -10); assert(near(perf.dj_filter, .2f));
    press(3); assert(page(3) == 0);
    // Hold 1.5 s without turning: back to the patch's value (performance: TAPE's default); no page change.
    press(3); press(3); assert(page(3) == 2);
    rig.Key(panel::kKnobEncoder[3], true);
    for(int i = 0; i < 2990; ++i) rig.Block();
    assert(near(perf.dj_filter, .2f));
    for(int i = 0; i < 20; ++i) rig.Block();
    assert(near(perf.dj_filter, .5f) && (rig.panel.KnobState() >> 19) & 1u);
    rig.Key(panel::kKnobEncoder[3], false); assert(page(3) == 2);
    press(3); assert(page(3) == 0);
    rig.Key(panel::kKnobEncoder[1], true); for(int i = 0; i < 3100; ++i) rig.Block(); rig.Key(panel::kKnobEncoder[1], false);
    assert(near(p.attack, 0.0045f) && page(1) == 0);                          // page 1 of SW1: attack back to the patch's
    // Sampler: SW1/SW2 page 1 are TAPE's start/end (.009 per click); SW2 page 4 = loop crossfade.
    Parameters sampler; sampler.version = 4; sampler.synth = true; sampler.source = 1; sampler.sample_xfade = 0.04f;
    assert(rig.engine.ApplyPatch(sampler));
    turn(1, 10); assert(near(p.sample_start, .09f));
    turn(2, -10); assert(near(p.sample_end, 1.f - .09f));
    for(int i = 0; i < 3; ++i) press(2);
    turn(2, 1); assert(near(p.sample_xfade, .04f + .03f));
    press(2); assert(page(2) == 0);
    // v5 assignment: one extra last page with the patch's control; CC 20 follows it.
    Parameters assigned = synth; assigned.version = 5; assigned.knobs[0] = static_cast<uint8_t>(Parameter::Attack) + 1;
    assert(rig.engine.ApplyPatch(assigned));
    for(int i = 0; i < 4; ++i) press(0);
    assert(page(0) == 4 && ((rig.panel.KnobState() >> 12) & 1u));
    turn(0, 1); assert(near(p.attack, 0.0045f + .01f));
    press(0); assert(page(0) == 0);
    Command cc; assert(DecodeCC(0, 20, 127, cc) && rig.engine.Apply(cc) && near(p.attack, 1.f));
    // A patch with fewer pages brings a knob back to page 1.
    for(int i = 0; i < 4; ++i) press(0);
    assert(page(0) == 4 && rig.engine.ApplyPatch(synth)); rig.Block(); assert(page(0) == 0);
    // LEDs: TAPE value colours; patch page dim white; reset flash white; off in the record position.
    Rgb leds[4];
    ComposeKnobLeds(0u | 255u << 8, 0u, false, leds);
    assert(leds[0].b > .9f && leds[0].r == 0.f);                              // pitch at 0: med blue (reverse, slow)
    assert(leds[1].r == 1.f && leds[1].g < .7f);                              // start at 1: orange
    ComposeKnobLeds(0u, 4u | 1u << 12, false, leds); assert(leds[0].r > 0.f && leds[0].r == leds[0].g && leds[0].g == leds[0].b);
    ComposeKnobLeds(0u, 1u << 17, false, leds); assert(leds[1].r == 1.f && leds[1].g == 1.f && leds[1].b == 1.f);
    ComposeKnobLeds(0u, 0u, true, leds); for(auto& l : leds) assert(l.r == 0.f && l.g == 0.f && l.b == 0.f);
    for(int i = 0; i < 200; ++i) rig.Block();
}
// TAPE's menu knob layer (MenuPage.h): while the TAPE menu page is open the knobs do
// TAPE's shift functions and their presses reset or toggle; pages never step.
void MenuKnobLayer() {
    Rig rig;
    Parameters sampler; sampler.version = 4; sampler.synth = true; sampler.source = 1; sampler.sample_end = .5f;
    assert(rig.engine.ApplyPatch(sampler));
    rig.Inject(PanelEvent::Kind::Toggle, 0, 1); rig.Key(panel::kChompiKey, true);
    assert((rig.panel.MenuPacked() & 1u) && ((rig.panel.MenuPacked() >> 21) & 1u) && (rig.panel.MenuLights() & 1u));
    const Parameters& p = rig.engine.GetParameters(); const Performance& perf = rig.engine.GetPerformance();
    auto turn = [&](unsigned enc, int amount) { rig.Inject(PanelEvent::Kind::Turn, enc, static_cast<int8_t>(amount)); };
    // SW4: quantised pitch, one step per 4 clicks (1x -> 1.5x -> 2x); press: back to 1x.
    turn(panel::kKnobEncoder[0], 3); assert(std::fabs(TapeSpeedRatio(perf.speed) - 1.f) < .01f);
    turn(panel::kKnobEncoder[0], 1); assert(std::fabs(TapeSpeedRatio(perf.speed) - 1.5f) < .01f);
    turn(panel::kKnobEncoder[0], 4); assert(std::fabs(TapeSpeedRatio(perf.speed) - 2.f) < .01f);
    rig.Tap(panel::kKnobEncoder[0]); assert(std::fabs(perf.speed - .83f) < 1e-6f && rig.panel.KnobPages() == 0);
    // SW1 / SW2 page 1 (sampler): move the start-end window together, .03 per click.
    turn(panel::kKnobEncoder[1], 2); assert(std::fabs(p.sample_start - .06f) < 1e-5f && std::fabs(p.sample_end - .56f) < 1e-5f);
    turn(panel::kKnobEncoder[2], -5); assert(p.sample_start == 0.f && std::fabs(p.sample_end - .5f) < 1e-5f);   // stops at 0
    // SW1 press: auto-loop on/off; SW2 press: sustain (hold) on/off; the lights follow.
    const bool loop = p.sample_loop, hold = p.sample_gate;
    rig.Tap(panel::kKnobEncoder[1]); rig.Tap(panel::kKnobEncoder[2]);
    assert(p.sample_loop == !loop && p.sample_gate == !hold && rig.panel.KnobPages() == 0);
    assert(((rig.panel.MenuLights() >> 1) & 1u) == p.sample_loop && ((rig.panel.MenuLights() >> 2) & 1u) == p.sample_gate);
    // SW3 page 1: delay time (and reverb size); SW3 press resets every effect.
    const float time = p.time; turn(panel::kKnobEncoder[3], 5); assert(std::fabs(p.time - (time + .15f)) < 1e-5f);
    rig.engine.Apply({Parameter::Saturation, .7f});
    rig.Tap(panel::kKnobEncoder[3]); assert(std::fabs(p.time - time) < 1e-5f && perf.saturation == 0.f);
    // SW6: compressor; press: next monitor position (the volume page does not change).
    turn(panel::kVolumeEncoder, 10); assert(std::fabs(perf.compressor - .3f) < 1e-5f);
    assert(rig.panel.Monitor() == panel::MonitorMode::Headphones);
    rig.Tap(panel::kVolumePress); assert(rig.panel.Monitor() == panel::MonitorMode::Both && !rig.panel.VolumePage());
    assert(((rig.panel.MenuLights() >> 3) & 3u) == 1);
    rig.Tap(panel::kVolumePress); rig.Tap(panel::kVolumePress); assert(rig.panel.Monitor() == panel::MonitorMode::Headphones);
    Rgb rings[4], volume; ComposeMenuKnobLeds(rig.panel.MenuLights(), rings, volume);
    assert((rings[1].r == 1.f) == p.sample_loop && (rings[2].r == 1.f) == p.sample_gate && volume.r == 1.f && volume.b < .3f);   // orange: headphones
    // Closing the menu gives the knobs back; a hold inside the menu never resets.
    rig.Key(panel::kChompiKey, false); assert(!(rig.panel.MenuLights() & 1u));
    turn(panel::kKnobEncoder[0], 10); assert(std::fabs(perf.speed - (.83f + .03f)) < 1e-5f);
    for(int i = 0; i < 200; ++i) rig.Block();
}
// TAPE's options.json applied: record latch, which pitch mode is quantised, split delay,
// monitor position, MIDI out channel; and TAPE's MIDI out from the panel.
void OptionsAndMidiOut() {
    Rig rig;
    Parameters synth; synth.version = 3; synth.synth = true; assert(rig.engine.ApplyPatch(synth));
    Options o; o.record_latch = true; o.quantise_menu = false; o.monitor = 1; o.midi_out = 2;
    rig.panel.SetOptions(o); rig.engine.SetSplitDelay(true);
    assert(rig.panel.Monitor() == panel::MonitorMode::Both);
    auto drain = [&]() { std::vector<MidiOut> v; MidiOut m; while(rig.panel.PopMidi(m)) v.push_back(m); return v; };
    rig.Block(); drain();
    // Keys: note on/off at velocity 127 on channel 3.
    rig.Key(kWhite[7], true); rig.Key(kWhite[7], false);
    auto m = drain();
    assert(m.size() == 2 && m[0].status == 0x92 && m[0].data1 == 60 && m[0].data2 == 127 && m[1].status == 0x82 && m[1].data1 == 60);
    // Knob turns send TAPE's CC for the page; a preset change sends nothing.
    rig.Inject(PanelEvent::Kind::Turn, panel::kKnobEncoder[1], 3); m = drain();
    assert(m.size() == 1 && m[0].status == 0xb2 && m[0].data1 == 21);
    rig.Tap(panel::kKnobEncoder[1]); drain(); rig.Inject(PanelEvent::Kind::Turn, panel::kKnobEncoder[1], 3); m = drain();
    assert(m.size() == 1 && m[0].data1 == 29);                                      // page 2: CC 29 (TAPE cc_map)
    assert(rig.engine.ApplyPatch(synth)); rig.Block(); assert(drain().empty());
    // Quantise option false: the normal page's pitch steps in fifths/octaves (4 clicks a step).
    rig.Inject(PanelEvent::Kind::Turn, panel::kKnobEncoder[0], 4);
    assert(std::fabs(TapeSpeedRatio(rig.engine.GetPerformance().speed) - 1.5f) < .01f);
    // TAPE's quantised steps (fifths/fourths up to 2x, down to 1/16x, then reverse) and the pitch curve's inverse.
    assert(QuantisedSpeedStep(1.f, 1) == 1.5f && QuantisedSpeedStep(1.5f, 1) == 2.f && QuantisedSpeedStep(2.f, 1) == 2.f);
    assert(QuantisedSpeedStep(1.f, -1) == .75f && QuantisedSpeedStep(.0625f, -1) == -.0625f && QuantisedSpeedStep(-.0625f, 1) == .0625f);
    assert(QuantisedSpeedStep(-1.f, 1) == -.75f && QuantisedSpeedStep(-2.f, -1) == -2.f);
    for(float r : {-2.f, -1.f, -.3f, .02f, .5f, 1.f, 1.5f, 2.f}) assert(std::fabs(TapeSpeedRatio(TapeSpeedKnob(r)) - r) < 1e-3f);
    // Split delay: SW3 page 1 left of centre = delay only, right = reverb only.
    rig.engine.Apply({Parameter::Space, .25f});
    assert(std::fabs(rig.engine.GetParameters().feedback - .5f) < 1e-5f && rig.engine.GetParameters().reverb_mix == 0.f);
    rig.engine.Apply({Parameter::Space, .75f});
    assert(rig.engine.GetParameters().feedback == 0.f && std::fabs(rig.engine.GetParameters().reverb_mix - .5f) < 1e-5f);
    // Record latch: press starts (after the count-in), release keeps recording, a second press stops.
    rig.Inject(PanelEvent::Kind::Toggle, 0, 0); rig.Inject(PanelEvent::Kind::Jack, 0, 1); drain();
    rig.Key(panel::kChompiKey, true); rig.Key(panel::kChompiKey, false);
    for(int i = 0; i < 3001; ++i) rig.Block(0.1f);
    assert(rig.recorder.Recording());
    m = drain(); assert(m.size() == 2 && m[0].status == 0xb2 && m[0].data1 == 21 && m[0].data2 == 127 && m[1].data2 == 0);   // CHOMPI CC 21
    rig.Key(panel::kChompiKey, true); assert(!rig.recorder.Recording());
    rig.Key(panel::kChompiKey, false); for(int i = 0; i < 4000; ++i) rig.Block(); assert(!rig.recorder.Recording());
}
// TAPE: SW6 (volume) held 2 s shows the battery on its light while held; a short press does not.
void BatteryHold() {
    Rig rig; rig.Block();
    const unsigned two_seconds = 2 * 48000 / 24;
    rig.hw.keys = uint64_t(1) << panel::kVolumePress;
    for(unsigned i = 0; i + 1 < two_seconds; ++i) { rig.Block(); assert(!rig.panel.BatteryView()); }
    rig.Block(); assert(rig.panel.BatteryView());
    for(int i = 0; i < 100; ++i) rig.Block();
    assert(rig.panel.BatteryView());
    rig.hw.keys = 0; rig.Block(); assert(!rig.panel.BatteryView());
    rig.hw.keys = uint64_t(1) << panel::kVolumePress; for(int i = 0; i < 1000; ++i) rig.Block();
    assert(!rig.panel.BatteryView());                                       // 0.5 s: no
    rig.hw.keys = 0; rig.Block(); assert(rig.engine.ActiveVoices() == 0);   // and it never plays a note
    // TAPE: a short press switches SW6 to input gain (.03 per click; .75 at power-on) and back.
    assert(rig.panel.VolumePage());
    rig.Inject(PanelEvent::Kind::Turn, 5, -5); assert(std::fabs(rig.engine.GetPerformance().input_gain - .6f) < 1e-5f);
    rig.Tap(panel::kVolumePress); assert(!rig.panel.VolumePage());
    rig.hw.keys = uint64_t(1) << panel::kVolumePress; for(int i = 0; i < 4100; ++i) rig.Block();
    rig.hw.keys = 0; rig.Block(); assert(!rig.panel.VolumePage());          // after the battery view: no switch
}

int main() {
    KnobPages(); BatteryHold(); MenuKnobLayer(); OptionsAndMidiOut();
    KeysKnobsAndOverrides(); MenuAndRecordingThroughTheController(); LedComposition(); DevelopmentOpcodes(); LooperThroughThePanel(); LooperVoiceCap(); LooperSaveGesture();
    std::cout << "PASS: panel controller keys/knobs/overrides, menu + recording via injection, LED composition, dev opcodes, looper via panel/MIDI, knob pages/LEDs/v5 assignment, SW6 battery hold\n";
}
