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
    for(uint8_t c : {1, 2}) assert(!rig.engine.SetChoice(fx::Module::Colour, c) && !rig.engine.SetChoice(fx::Module::Texture, c));
    assert(!rig.engine.SetChoice(fx::Module::Space, 2) && !rig.engine.SetChoice(fx::Module::Motion, 1) && !rig.engine.SetChoice(fx::Module::Motion, 3));
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
}

int main() {
    const uint64_t golden = Scene();
    std::cout << "fx golden " << std::hex << golden << std::dec << "\n";
    assert(golden == Scene());                                             // deterministic
    assert(golden == 0x1f4ac63c29d80babull);                              // the pre-FX chain (e245ee6)
    assert(Engine::FxFloats() <= 8448);                                    // forge_main.cpp kFxFloats
    ChorusUnit(); DiffuserUnit(); EngineFx();
    std::cout << "PASS: FX section: golden (Classic route, new modules at zero = the pre-FX chain), chorus, diffuser, routes, Motion, cloud (longer tail, no echoes), unbuilt choices refused\n";
}
