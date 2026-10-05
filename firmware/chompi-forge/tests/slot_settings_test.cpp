// TAPE's per-slot sample settings (core/slot_settings.h, firmware 0.12): presets.json in
// TAPE's own layout, saving as a knob turns, chromatic slots restored on selection, kit
// pads with their own pitch/gain/pan/window/envelope, heard in the rendered audio, and
// the menu's copy/erase reaching playing pads.
#include <cassert>
#include <cmath>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
#include "../core/engine.h"
#include "../core/sampler_runtime.h"
#include "../core/slot_settings.h"

using namespace forge;

namespace {
constexpr uint32_t kFrames = 48000;
bool Near(float a, float b, float e = .0011f) { return std::fabs(a - b) <= e; }

// TAPE's PresetManager::WriteWholeFile, re-done independently from TAPE's source text.
std::string TapeLayout(const SlotSettings& s) {
    std::string out = "[";
    for(unsigned m = 0; m < 2; ++m) {
        out += "[";
        for(unsigned b = 0; b < 5; ++b) {
            out += "[";
            for(unsigned k = 0; k < 14; ++k) {
                const SlotValues v = s.Get(m, b, k);
                const float f[9] = {v.pitch, v.start, v.end, v.attack, v.release, v.loop ? 1.f : 0.f, v.sustain ? 1.f : 0.f, v.gain, v.pan};
                out += "[";
                for(float x : f) out += std::to_string(static_cast<int>(std::lround(x * 1000))) + ",";
                out += s.Valid(m, b, k) ? "true]," : "false],";
            }
            out.pop_back(); out += "],";
        }
        out.pop_back(); out += "],";
    }
    return out + "2]";
}

void FileFormat() {
    SlotSettings s;
    char text[SlotSettings::kFileMax];
    // All defaults: TAPE's layout, and every slot invalid.
    size_t n = s.Write(text, sizeof text);
    assert(n > 0 && text[n] == 0 && std::string(text) == TapeLayout(s));
    assert(std::strncmp(text, "[[[[830,0,1000,0,0,1000,1000,704,500,false],", 44) == 0);
    assert(!s.Dirty());
    // Values survive a round trip; only file slots mark the file dirty.
    SlotValues v; v.pitch = .5f; v.start = .25f; v.end = .75f; v.attack = .1f; v.release = .9f; v.loop = false; v.sustain = false; v.gain = 1.f; v.pan = 0.f;
    s.Set(1, 4, 13, v);
    assert(s.Dirty() && s.TakeDirty() && !s.Dirty());
    s.Set(0, 0, kRamSlot, v); assert(!s.Dirty() && s.Valid(0, 3, kRamSlot));   // the recording: memory only
    n = s.Write(text, sizeof text);
    assert(n > 0 && std::string(text) == TapeLayout(s));
    SlotSettings r; assert(r.Parse(text, n));
    assert(r.Valid(1, 4, 13) && !r.Valid(1, 4, 12) && !r.Valid(0, 0, 0) && !r.Valid(0, 0, kRamSlot));
    const SlotValues got = r.Get(1, 4, 13);
    assert(Near(got.pitch, .5f) && Near(got.start, .25f) && Near(got.end, .75f) && Near(got.attack, .1f) && Near(got.release, .9f));
    assert(!got.loop && !got.sustain && Near(got.gain, 1.f) && Near(got.pan, 0.f));
    // Largest possible file (every value 1000, every slot valid) fits TAPE's 8 KB buffer.
    SlotSettings full; SlotValues big; big.pitch = big.start = big.end = big.attack = big.release = big.gain = big.pan = 1.f;
    for(unsigned m = 0; m < 2; ++m) for(unsigned b = 0; b < 5; ++b) for(unsigned k = 0; k < 14; ++k) full.Set(m, b, k, big);
    n = full.Write(text, sizeof text);
    assert(n > 7000 && n < SlotSettings::kFileMax);
    assert(full.Write(text, 100) == 0);                                   // too small: refused, not truncated
    // TAPE version 1 (7 values, no gain/pan), spaces, floats and junk are tolerated.
    const char v1[] = " [ [ [ [500, 100, 900, 0, 250, 0, 1, true] ] ], [] ] ";
    SlotSettings old; assert(old.Parse(v1, sizeof v1));
    const SlotValues o = old.Get(0, 0, 0);
    assert(old.Valid(0, 0, 0) && Near(o.pitch, .5f) && Near(o.start, .1f) && Near(o.end, .9f) && Near(o.release, .25f));
    assert(!o.loop && o.sustain && Near(o.gain, .704f) && Near(o.pan, .5f));
    const char odd[] = "[[[[830.7,-5,2000,0,0,1000,1000,704,500,true],[1,2,3],[1,2,3,4,5,6,7,8,9,10,11,true]]]]";
    SlotSettings j; assert(j.Parse(odd, sizeof odd));
    assert(j.Valid(0, 0, 0) && Near(j.Get(0, 0, 0).pitch, .83f) && j.Get(0, 0, 0).start == 0.f && j.Get(0, 0, 0).end == 1.f);
    assert(!j.Valid(0, 0, 1) && !j.Valid(0, 0, 2));                      // wrong counts: skipped
    SlotSettings none; assert(!none.Parse("not json", 8) && !none.Parse("", 0));
    // Menu: Save copies the recording's settings, Copy a slot's, Erase invalidates.
    SlotSettings m; m.Set(0, 0, kRamSlot, v); m.TakeDirty();
    m.Copy(0, 0, kRamSlot, 0, 2, 5); assert(m.Valid(0, 2, 5) && Near(m.Get(0, 2, 5).start, .25f) && m.TakeDirty());
    m.Copy(0, 2, 5, 1, 0, 0); assert(m.Valid(1, 0, 0));
    m.Invalidate(0, 2, 5); assert(!m.Valid(0, 2, 5) && m.Dirty());
}

struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<int16_t> sine = std::vector<int16_t>(2 * kFrames);
    SampleTable table; SlotSettings slots; Engine engine;
    Rig() {
        for(uint32_t i = 0; i < kFrames; ++i) sine[2 * i] = sine[2 * i + 1] = static_cast<int16_t>(12000 * std::sin(6.2831853 * 441.0 * i / 48000.0));
        for(auto& s : table.slots) { s.data = sine.data(); s.frames = kFrames; s.channels = 2; s.rate_ratio = 1.f; s.gain = 1.f; s.loaded.store(kFrames); }
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetSamples(&table); engine.SetSlotSettings(&slots);
    }
    // Hold a note for `frames`; returns left and right energy and the left channel's zero crossings.
    void Play(uint8_t note, uint32_t frames, double& el, double& er, unsigned& crossings) {
        engine.Note(note, 127, 0);
        el = er = 0; crossings = 0; float last = 0;
        for(uint32_t i = 0; i < frames; ++i) {
            float a, b; engine.Process(0, 0, a, b);
            if(i > 2400) { el += a * a; er += b * b; if((a > 0) != (last > 0)) ++crossings; }
            last = a;
        }
        engine.Note(note, 0, 0);
        for(int i = 0; i < 48000; ++i) { float a, b; engine.Process(0, 0, a, b); }
    }
};

Parameters Sampler(uint8_t mode, uint8_t slot) {
    Parameters p = SelectSample(Parameters{}, mode, 0, slot);
    p.mix = 0.f; p.level = 1.f; p.reverb_mix = 0.f;
    return p;
}

void Chromatic() {
    Rig rig; Engine& e = rig.engine;
    assert(e.ApplyPatch(Sampler(0, 3), SlotPolicy::Select));
    assert(!rig.slots.Valid(0, 0, 3) && Near(e.Value(Parameter::Speed), .83f));   // unsaved slot: TAPE's defaults
    // Turning a slot control saves the whole set straight away (TAPE DumpValuePresets).
    assert(e.Apply({Parameter::SampleStart, .3f}) && e.Apply({Parameter::Speed, .9f}));
    assert(rig.slots.Valid(0, 0, 3) && Near(rig.slots.Get(0, 0, 3).start, .3f) && Near(rig.slots.Get(0, 0, 3).pitch, .9f));
    assert(rig.slots.Dirty());
    e.ToggleSampleLoop(); assert(rig.slots.Get(0, 0, 3).loop == e.GetParameters().sample_loop);
    // Another slot starts from TAPE's defaults; coming back restores slot 4's settings.
    assert(e.ApplyPatch(SelectSample(e.GetParameters(), 0, 0, 4), SlotPolicy::Select));
    assert(Near(e.Value(Parameter::SampleStart), 0.f) && Near(e.Value(Parameter::Speed), .83f));
    assert(e.ApplyPatch(SelectSample(e.GetParameters(), 0, 0, 3), SlotPolicy::Select));
    assert(Near(e.Value(Parameter::SampleStart), .3f) && Near(e.Value(Parameter::Speed), .9f));
    // A recalled Forge preset on slot 3: the slot's settings win; the webapp's patch keeps its own.
    Parameters preset = Sampler(0, 3); preset.sample_start = .6f; preset.cutoff = .4f;
    assert(e.ApplyPatch(preset, SlotPolicy::Recall));
    assert(Near(e.Value(Parameter::SampleStart), .3f) && Near(e.GetParameters().cutoff, .4f));
    assert(e.ApplyPatch(preset, SlotPolicy::Patch) && Near(e.Value(Parameter::SampleStart), .6f));
    // A recalled preset on a slot with nothing saved keeps the preset's values.
    Parameters other = Sampler(0, 7); other.sample_start = .45f;
    assert(e.ApplyPatch(other, SlotPolicy::Recall) && Near(e.Value(Parameter::SampleStart), .45f));
    // Long-press reset: TAPE's default, saved.
    assert(e.ResetControl(Parameter::SampleStart) && Near(e.Value(Parameter::SampleStart), 0.f) && rig.slots.Valid(0, 0, 7));
    // Synth patches are untouched by slot settings.
    Parameters synth; synth.version = 4; synth.synth = true; synth.attack = .2f;
    assert(e.ApplyPatch(synth) && e.Apply({Parameter::Attack, .5f}) && Near(e.Value(Parameter::Attack), .5f));
    assert(e.ResetControl(Parameter::Attack) && Near(e.Value(Parameter::Attack), .2f));
    // The settings are heard: slot 3 (pitch .9 = faster) against slot 5 (1x).
    assert(e.ApplyPatch(Sampler(0, 3), SlotPolicy::Select));
    double l1, r1, l2, r2; unsigned fast, normal;
    rig.Play(60, 24000, l1, r1, fast);
    assert(e.ApplyPatch(Sampler(0, 5), SlotPolicy::Select));
    rig.Play(60, 24000, l2, r2, normal);
    assert(fast > normal * 11 / 10);
}

void Kit() {
    Rig rig; Engine& e = rig.engine;
    assert(e.ApplyPatch(Sampler(1, 0), SlotPolicy::Select));
    double base_l, base_r; unsigned base;
    rig.Play(48, 24000, base_l, base_r, base);                     // pad 1 (C3) as the patch has it
    assert(e.FocusPad() == 0 && base > 0 && Near(static_cast<float>(base_l / base_r), 1.f, .01f));
    // The knobs now edit pad 1 only: pitch and pan saved for it, heard on it.
    assert(e.Apply({Parameter::Speed, .9f}) && e.Apply({Parameter::Pan, 0.f}));
    assert(rig.slots.Valid(1, 0, 0) && !rig.slots.Valid(1, 0, 1) && Near(e.Value(Parameter::Speed), .9f));
    assert(Near(e.GetPerformance().speed, .83f));                   // the shared pitch is not touched
    double l, r; unsigned crossings;
    rig.Play(48, 24000, l, r, crossings);
    assert(crossings > base * 11 / 10 && r < l * 1e-6);              // faster, and panned hard left
    // Pad 2 (D3) still plays as the patch has it, and becomes the knobs' pad.
    rig.Play(50, 24000, l, r, crossings);
    assert(e.FocusPad() == 1 && crossings <= base + 2 && crossings + 2 >= base && Near(static_cast<float>(l / r), 1.f, .01f));
    assert(Near(e.Value(Parameter::Speed), .83f));
    // Window and envelope per pad: a short window on pad 2 ends a one-shot early.
    e.ToggleSampleLoop(); assert(rig.slots.Valid(1, 0, 1) && rig.slots.Get(1, 0, 1).loop == !e.GetParameters().sample_loop);
    if(rig.slots.Get(1, 0, 1).loop) e.ToggleSampleLoop();
    assert(e.Apply({Parameter::SampleEnd, .1f}));
    rig.Play(50, 24000, l, r, crossings);
    assert(crossings < base / 2);                                    // 0.1 s of a 1 s sample, then silence
    // The menu (main loop) erases pad 1's settings: the next block follows.
    rig.slots.Invalidate(1, 0, 0); e.SyncSlotSettings();
    rig.Play(48, 24000, l, r, crossings);
    assert(crossings + 2 >= base && crossings <= base + 2 && Near(static_cast<float>(l / r), 1.f, .01f));
    // Copy pad 2 to pad 3 (E3): pad 3 now ends early too.
    rig.slots.Copy(1, 0, 1, 1, 0, 2); e.SyncSlotSettings();
    rig.Play(52, 24000, l, r, crossings);
    assert(crossings < base / 2);
    // Back to chromatic: the shared pitch applies again (pads do not).
    assert(e.ApplyPatch(Sampler(0, 0), SlotPolicy::Select));
    rig.Play(60, 24000, l, r, crossings);
    assert(crossings + 2 >= base && crossings <= base + 2);
}
} // namespace

int main() {
    FileFormat(); Chromatic(); Kit();
    std::cout << "PASS: presets.json TAPE layout/round trip/v1/junk/8 KB bound, menu save/copy/erase, chromatic save-on-turn and"
                 " restore, recall/select/patch policies, reset, kit pads (pitch, pan, window, loop) heard per pad, menu sync\n";
}
