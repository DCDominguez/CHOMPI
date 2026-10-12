// The FX section (docs/forge/CONTINUE.md, 0.20 development). Golden: with the Classic route and every
// new module at zero, the engine's output is bit-identical to the chain before the FX section
// (DJ filter -> saturation -> warble -> delay -> reverb -> compressor), on a scene that turns every one
// of those on. The fingerprint was recorded from the pre-FX engine (build e245ee6).
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include "../core/engine.h"

using namespace forge;

namespace {
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<float> warble = std::vector<float>(2 * tape::Warble::kLength);
    Engine engine;
    Rig() {
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetWarbleMemory(warble.data());
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
uint64_t Scene() {
    Rig rig;
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
}

int main() {
    const uint64_t golden = Scene();
    std::cout << "fx golden " << std::hex << golden << std::dec << "\n";
    assert(golden == Scene());                                             // deterministic
    assert(golden == 0x1f4ac63c29d80babull);                              // the pre-FX chain (e245ee6)
    std::cout << "PASS: FX section: golden (Classic route, new modules at zero = the pre-FX chain)\n";
}
