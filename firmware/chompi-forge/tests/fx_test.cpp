// The FX section (docs/forge/CONTINUE.md, 0.20 development). Golden: with the Classic route and every
// new module at zero, the engine's output is bit-identical to the chain before the FX section
// (DJ filter -> saturation -> warble -> delay -> reverb -> compressor), on a scene that turns every one
// of those on. The fingerprint was recorded from the pre-FX engine (build e245ee6).
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <cmath>
#include "../core/engine.h"
#include "../core/fx.h"

using namespace forge;

namespace {
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<float> warble = std::vector<float>(2 * tape::Warble::kLength);
    std::vector<float> fxm = std::vector<float>(Engine::FxFloats(), 77.f);   // stale, as D2 SRAM at boot
    Engine engine;
    Rig() {
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetWarbleMemory(warble.data());
        assert(!engine.SetFxMemory(fxm.data(), fxm.size() - 1) && engine.SetFxMemory(fxm.data(), fxm.size()));
    }
};
uint64_t Fingerprint(Rig& rig, uint32_t blocks) {
    uint64_t h = 1469598103934665603ull;
    for(uint32_t b = 0; b < blocks; ++b) {
        rig.engine.Block(24);
        for(int i = 0; i < 24; ++i) {
            const float in = (i & 1) ? .05f : -.05f;          // a little line input as well
            float a, c; rig.engine.Process(in, -in, a, c);
            uint32_t bits[2]; std::memcpy(&bits[0], &a, 4); std::memcpy(&bits[1], &c, 4);
            for(uint32_t v : bits) { h ^= v; h *= 1099511628211ull; }
        }
    }
    return h;
}
// Every pre-FX effect on: a v3 synth sound with delay and reverb, then TAPE's saturation, warble,
// DJ filter (low-pass) and output compressor turned up; a chord held, then released into the tails.
uint64_t Scene(fx::Route route = fx::Route::Classic, float motion = 0.f, uint8_t space = 0) {
    Rig rig; rig.engine.SetRoute(route); assert(rig.engine.SetChoice(fx::Module::Space, space));
    if(motion > 0.f) assert(rig.engine.Apply({Parameter::Motion, motion}));
    Parameters p; p.version = 3; p.synth = true; p.voices = 4; p.osc2_level = .6f; p.noise = .1f;
    p.mix = .3f; p.feedback = .45f; p.time = .35f; p.reverb_mix = .4f; p.reverb_size = .8f; p.reverb_damping = .4f; p.level = .5f;
    assert(rig.engine.ApplyPatch(p));
    for(auto c : {Command{Parameter::Saturation, .6f}, Command{Parameter::Warble, .5f}, Command{Parameter::DjFilter, .3f},
                  Command{Parameter::DjResonance, .4f}, Command{Parameter::Compressor, .5f}})
        assert(rig.engine.Apply(c));
    for(uint8_t n : {48, 55, 60, 64, 67}) rig.engine.Note(n, 100, 0);
    uint64_t h = Fingerprint(rig, 2000);                                   // 1 s held
    for(uint8_t n : {48, 55, 60, 64, 67}) rig.engine.Note(n, 0, 0);
    return h ^ Fingerprint(rig, 4000);                                     // 2 s of tails
}

// Chorus: transparent at zero, bounded and audibly different when on, stereo when on, follows rate.
void ChorusUnit() {
    std::vector<float> memory(2 * fx::Chorus::kLength, 123.f);              // stale memory must not leak
    fx::Chorus c; c.Init(memory.data(), 48000.f);
    assert(c.Neutral());
    c.Set(.8f, .4f); assert(!c.Neutral());
    float peak = 0.f, diff = 0.f, width = 0.f; uint32_t seed = 7;
    for(int i = 0; i < 96000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float x = (static_cast<float>(seed >> 8) / 8388608.f - 1.f) * .9f;   // noise, mono
        float l = x, r = x; c.Process(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
        peak = std::fmax(peak, std::fmax(std::fabs(l), std::fabs(r)));
        if(i > 4800) { diff += std::fabs(l - x); width += std::fabs(l - r); }
    }
    assert(peak < 1.f && peak > .5f && diff > 1000.f && width > 1000.f);     // never above the input range
    c.Set(0.f, .4f);
    for(int i = 0; i < 48000; ++i) { float l = .3f, r = .3f; c.Process(l, r); }
    assert(c.Neutral());
    c.Rest(); c.Set(.5f, .4f);
    float l = 0.f, r = 0.f; c.Process(l, r); assert(l == 0.f && r == 0.f);  // the line was cleared on return
}
// Diffuser: all-passes keep the energy (long run), smear an impulse over time, stay finite.
void DiffuserUnit() {
    std::vector<float> memory(fx::Diffuser::Required(), -9.f);
    fx::Diffuser d; assert(!d.Init(memory.data(), memory.size() - 1, 48000.f));
    assert(d.Init(memory.data(), memory.size(), 48000.f));
    double in = 0.0, out = 0.0, tone_in = 0.0, tone_out = 0.0; uint32_t seed = 3;
    for(int i = 0; i < 480000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float x = static_cast<float>(seed >> 8) / 8388608.f - 1.f;
        float l = x, r = -x; d.Process(l, r);
        assert(std::isfinite(l) && std::isfinite(r) && std::fabs(l) < 8.f);
        in += 2.0 * x * x; out += l * l + r * r;
    }
    // The drifting lines are interpolated, which dulls the highs inside the all-pass loops: white
    // noise loses about 4 dB (darker, as a cloud should be) while lows and mids keep their level.
    assert(out / in > .3 && out / in < 1.1);
    d.Rest();
    for(int i = 0; i < 240000; ++i) {
        const float x = .5f * std::sin(6.2831853f * 200.f * static_cast<float>(i) / 48000.f);
        float l = x, r = x; d.Process(l, r);
        if(i > 48000) { tone_in += 2.0 * x * x; tone_out += l * l + r * r; }
    }
    assert(tone_out / tone_in > .9 && tone_out / tone_in < 1.1);
    d.Rest(); int spread = 0;
    for(int i = 0; i < 9600; ++i) { float l = i == 0 ? 1.f : 0.f, r = l; d.Process(l, r); if(std::fabs(l) > 1e-3f) ++spread; }
    assert(spread > 200);                                                   // one click becomes a haze
    float big = 1e30f, nan = std::nanf(""); d.Process(big, nan);            // garbage in: no crash
}

// Routes and choices in the engine: every route its own sound and bounded, Motion audible, unbuilt
// effects refused, cloud a longer tail with the echoes faded out.
void EngineFx() {
    const uint64_t classic = Scene();
    // Wash first only moves Motion: with Motion at zero it is Classic; the other routes differ.
    const uint64_t wear = Scene(fx::Route::WearLast), wash = Scene(fx::Route::WashFirst), side = Scene(fx::Route::SideBySide);
    assert(wash == classic && wear != classic && side != classic && wear != side);
    assert(Scene(fx::Route::WashFirst, .7f) != Scene(fx::Route::Classic, .7f));
    assert(Scene(fx::Route::Classic, .7f) != classic);                       // Motion (chorus) on
    assert(Scene(fx::Route::Classic, 0.f, 1) != classic);                    // cloud
    Rig rig;
    for(unsigned m = 0; m < fx::kModules; ++m) assert(!rig.engine.SetChoice(static_cast<fx::Module>(m), 3));   // only 0-2 exist
    assert(rig.engine.Choice(fx::Module::Space) == 0 && rig.engine.SetChoice(fx::Module::Space, 1) && rig.engine.Choice(fx::Module::Space) == 1);
    // Cloud vs echo + room: a short chord, then the tail 1-3 s later; cloud's is longer, and with the
    // reverb off cloud passes no echoes at all.
    auto tail = [](uint8_t space, float reverb) {
        Rig t; assert(t.engine.SetChoice(fx::Module::Space, space));
        Parameters p; p.version = 3; p.synth = true; p.voices = 4; p.release = 0.f; p.sustain = 1.f;
        p.mix = .5f; p.feedback = .6f; p.time = .2f; p.reverb_mix = reverb; p.reverb_size = .5f; p.level = 1.f;
        assert(t.engine.ApplyPatch(p));
        for(uint8_t n : {60, 64, 67}) t.engine.Note(n, 110, 0);
        double energy = 0.0;
        for(int b = 0; b < 8000; ++b) {
            if(b == 200) for(uint8_t n : {60, 64, 67}) t.engine.Note(n, 0, 0);
            t.engine.Block(24);
            for(int i = 0; i < 24; ++i) {
                float a, c; t.engine.Process(0.f, 0.f, a, c);
                assert(std::isfinite(a) && std::fabs(a) <= 1.f);
                if(b > 2400) energy += a * a + c * c;
            }
        }
        return energy;
    };
    assert(tail(1, .5f) > 2.0 * tail(0, .5f));
    assert(tail(1, 0.f) < 1e-6 && tail(0, 0.f) > 1e-3);
}

// Phase 2: every new effect is transparent at zero (bit for bit), bounded and finite on loud noise,
// and changes the sound when on.
template<typename E> void EffectUnit(E& e, float amount, float character, float rate_hint = 0.f) {
    (void)rate_hint;
    uint32_t seed = 11;
    auto noise = [&]() { seed = seed * 1664525u + 1013904223u; return (static_cast<float>(seed >> 8) / 8388608.f - 1.f) * .9f; };
    e.Set(0.f, character);
    for(int i = 0; i < 4800; ++i) { const float x = noise(); float l = x, r = -x; if(!e.Neutral()) e.Process(l, r); assert(l == x && r == -x); }
    e.Set(amount, character);
    double diff = 0.0; float peak = 0.f;
    for(int i = 0; i < 96000; ++i) {
        const float x = noise(); float l = x, r = .5f * x; e.Process(l, r);
        assert(std::isfinite(l) && std::isfinite(r));
        peak = std::fmax(peak, std::fmax(std::fabs(l), std::fabs(r)));
        if(i > 9600) diff += std::fabs(l - x) + std::fabs(r - .5f * x);
    }
    assert(peak <= 1.5f && diff > 500.0);
    e.Set(0.f, character);
    for(int i = 0; i < 96000; ++i) { float l = .3f, r = .3f; if(!e.Neutral()) e.Process(l, r); }
    assert(e.Neutral());                                                    // settles back off
}
// Fold keeps roughly the input's level (it clipped at 70 % on CHOMPI before the compensation).
void FoldLevel() {
    for(float amount : {.3f, .7f, 1.f}) {
        fx::Fold f; f.Set(amount, 0.f);
        double in = 0.0, out = 0.0;
        for(int i = 0; i < 96000; ++i) {
            const float x = .3f * std::sin(6.2831853f * 220.f * static_cast<float>(i) / 48000.f);
            float l = x, r = x; f.Process(l, r);
            if(i > 9600) { in += x * x; out += l * l; }
        }
        const double db = 10.0 * std::log10(out / in);
        assert(db > -9.0 && db < 6.0);
    }
}
// The 0.20 secret, broken radio: cut-outs only on the 1/16 grid (the gain moves just after step edges), the
// downbeat of every bar open, bounded, a radio band (less low end than the input), off again when the knob is.
void BrokenRadio() {
    const uint32_t step = 3000;                                            // a 1/16 at 240 BPM, 48 kHz
    fx::Broken b; b.Init(48000.f); b.Set(1.f, 0.f); b.SetRadio(true, step); assert(b.Radio());
    // A 1 kHz tone (inside the radio band): its loudness in eighths of each step only changes at the step's start.
    const uint32_t w = step / 8; uint32_t moves_late = 0; double lp_in = 0.0, lp_out = 0.0, window = 0.0, first = 0.0;
    for(uint32_t i = 0; i < 16 * step * 4; ++i) {                         // four bars
        const float x = .3f * std::sin(6.2831853f * 1000.f * static_cast<float>(i) / 48000.f);
        float l = x, r = x; b.Process(l, r);
        assert(std::isfinite(l) && std::fabs(l) < .6f);
        lp_in += x * x; lp_out += l * l; window += l * l;
        if((i + 1) % w == 0) {
            const uint32_t eighth = (i % step) / w;
            if(eighth == 2) first = window;                                 // after the ~5 ms edge has settled
            // From the second bar (the band has faded in), in open steps (a closed one sinks towards zero).
            else if(i >= 16 * step && eighth > 2 && first > .25 * w * .045 && (window > 1.3 * first || window < first / 1.3)) ++moves_late;
            window = 0.0;
        }
    }
    assert(moves_late == 0);                                               // no cut-outs between grid steps
    assert(lp_out < .7 * lp_in);                                           // cut-outs and the band: clearly broken
    b.SetRadio(false, step); b.Set(0.f, 0.f);
    for(int i = 0; i < 96000; ++i) { float l = .3f, r = .3f; if(!b.Neutral()) b.Process(l, r); }
    assert(b.Neutral());                                                   // knob down: off, radio or not
    fx::Broken quiet; quiet.Init(48000.f); quiet.Set(0.f, 0.f); quiet.SetRadio(true, step);
    assert(quiet.Neutral());                                               // radio on, amount 0: bypassed
}
void Phase2Units() {
    FoldLevel(); BrokenRadio();
    { fx::Fold f; EffectUnit(f, .7f, .5f); }
    { fx::Crush c; EffectUnit(c, .8f, .6f); }
    { fx::Squash q; q.Init(48000.f); EffectUnit(q, .8f, .3f); }
    { fx::Broken b; b.Init(48000.f); EffectUnit(b, 1.f, .8f); }
    { fx::Phaser p; p.Init(48000.f); EffectUnit(p, .9f, .5f); }
    { fx::Tremolo m; m.Init(48000.f); EffectUnit(m, 1.f, .6f); }
}
// In the engine: each of the twelve choices, with its module turned up, is its own sound and bounded;
// tape echo and cassette's character are audible; switching choices never leaves a stale tail.
void EngineAllTwelve() {
    auto run = [](fx::Module m, uint8_t choice, float amount, float character) {
        Rig rig;
        Parameters p; p.version = 3; p.synth = true; p.voices = 4; p.release = .2f; p.sustain = .8f;
        p.mix = .4f; p.feedback = .5f; p.time = .3f; p.reverb_mix = .3f; p.level = .8f;
        assert(rig.engine.ApplyPatch(p) && rig.engine.SetChoice(m, choice));
        const Parameter amount_of[4] = {Parameter::Space, Parameter::Saturation, Parameter::Warble, Parameter::Motion};
        const Parameter character_of[4] = {Parameter::ReverbSize, Parameter::ColourCharacter, Parameter::TextureCharacter, Parameter::MotionRate};
        assert(rig.engine.Apply({amount_of[static_cast<unsigned>(m)], amount}));
        assert(rig.engine.Apply({character_of[static_cast<unsigned>(m)], character}));
        for(uint8_t n : {48, 55, 60, 64}) rig.engine.Note(n, 100, 0);
        uint64_t h = 1469598103934665603ull;
        for(int b = 0; b < 4000; ++b) {
            if(b == 2000) for(uint8_t n : {48, 55, 60, 64}) rig.engine.Note(n, 0, 0);
            rig.engine.Block(24);
            for(int i = 0; i < 24; ++i) {
                float l, r; rig.engine.Process(0.f, 0.f, l, r);
                assert(std::isfinite(l) && std::isfinite(r) && std::fabs(l) <= 1.f && std::fabs(r) <= 1.f);
                uint32_t bits[2]; std::memcpy(&bits[0], &l, 4); std::memcpy(&bits[1], &r, 4);
                for(uint32_t v : bits) { h ^= v; h *= 1099511628211ull; }
            }
        }
        return h;
    };
    for(unsigned m = 0; m < fx::kModules; ++m) {
        const fx::Module module = static_cast<fx::Module>(m);
        const uint64_t a = run(module, 0, .7f, .5f), b = run(module, 1, .7f, .5f), c = run(module, 2, .7f, .5f);
        assert(a != b && b != c && a != c);
    }
    assert(run(fx::Module::Texture, 0, .5f, 0.f) != run(fx::Module::Texture, 0, .5f, .8f));   // cassette: hiss and wear
    assert(run(fx::Module::Colour, 1, .6f, 0.f) != run(fx::Module::Colour, 1, .6f, .9f));     // fold: bias
    // A v8 sound carries the characters (round trip through the snapshot).
    Rig rig; assert(rig.engine.SetChoice(fx::Module::Texture, 2));
    assert(rig.engine.Apply({Parameter::Warble, .4f}) && rig.engine.Apply({Parameter::TextureCharacter, .6f}) && rig.engine.Apply({Parameter::ColourCharacter, .2f}));
    Parameters base; base.version = 3; base.synth = true; assert(rig.engine.ApplyPatch(base));
    const Parameters saved = rig.engine.Snapshot();
    assert(saved.version == 8 && std::fabs(saved.fx_texture_character - .6f) < 1e-4f && std::fabs(saved.fx_colour_character - .2f) < 1e-4f);
    Rig other; assert(other.engine.ApplyPatch(saved) && other.engine.Choice(fx::Module::Texture) == 2);
    assert(std::fabs(other.engine.Value(Parameter::TextureCharacter) - .6f) < 1e-4f);
}
}

int main() {
    const uint64_t golden = Scene();
    std::cout << "fx golden " << std::hex << golden << std::dec << "\n";
    assert(golden == Scene());                                             // deterministic
    assert(golden == 0x1f4ac63c29d80babull);                              // the pre-FX chain (e245ee6)
    assert(Engine::FxFloats() <= 8448);                                    // forge_main.cpp kFxFloats
    ChorusUnit(); DiffuserUnit(); EngineFx(); Phase2Units(); EngineAllTwelve();
    std::cout << "PASS: FX section: golden (Classic route, new modules at zero = the pre-FX chain), chorus, diffuser, routes, Motion, cloud (longer tail, no echoes); Phase 2: fold, crush, squash, broken, phaser, tremolo, tape echo, characters, all twelve distinct and bounded\n";
}
