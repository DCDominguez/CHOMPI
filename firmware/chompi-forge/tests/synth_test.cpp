#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include "../core/runtime.h"
using namespace forge;

Parameters Instrument() {
    Parameters p; p.version = 2; p.synth = true; p.mix = 0; p.level = 1;
    p.attack = p.decay = p.release = 0; p.sustain = 1; p.cutoff = 1;
    return p;
}
void VoicesAndPitch() {
    Synth synth; synth.Init(48000); const auto p = Instrument(); synth.Configure(p);
    for(unsigned i = 0; i < 4800; ++i) assert(synth.Process() == 0);
    synth.Note(69, 127, 0);
    for(unsigned i = 0; i < 4800; ++i) synth.Process();
    unsigned crossings = 0; float prior = 0, peak = 0;
    for(unsigned i = 0; i < 48000; ++i) {
        float x = synth.Process(); if(prior <= 0 && x > 0) ++crossings;
        prior = x; peak = std::max(peak, std::fabs(x));
    }
    assert(crossings >= 439 && crossings <= 441 && peak > 0.15f && peak <= 0.201f);
    synth.Note(69, 0, 1); // different input must not release this note
    for(unsigned i = 0; i < 4800; ++i) synth.Process();
    assert(synth.Active() == 1);
    synth.Note(69, 0, 0);
    for(unsigned i = 0; i < 4800; ++i) synth.Process();
    assert(synth.Active() == 0 && std::fabs(synth.Process()) < 1e-9f);
    for(unsigned n = 40; n < 80; ++n) synth.Note(n, 100, 0);
    assert(synth.Active() == 4); // bounded stealing
    synth.Note(40, 0, 0); // stolen note-off must not kill replacement
    assert(synth.Active() == 4);
    synth.Silence(); assert(synth.Active() == 0 && synth.Process() == 0);
}
void EnvelopeVelocityAndFilter() {
    auto p = Instrument(); p.attack = (100.f - 1.f) / 1999.f; p.decay = p.attack; p.sustain = 0.2f;
    p.release = (100.f - 5.f) / 4995.f;
    Synth loud, soft; loud.Init(48000); soft.Init(48000); loud.Configure(p); soft.Configure(p);
    loud.Note(69, 100, 0); soft.Note(69, 50, 0);
    float early = 0, attack = 0, sustain = 0;
    for(unsigned i = 0; i < 24000; ++i) {
        float x = loud.Process(), y = soft.Process();
        assert(std::fabs(y * 2 - x) < 1e-5f);
        if(i < 480) early = std::max(early, std::fabs(x));
        if(i >= 4320 && i < 4800) attack = std::max(attack, std::fabs(x));
        if(i > 18000) sustain = std::max(sustain, std::fabs(x));
    }
    assert(early < attack * .15f && sustain < attack * .25f);
    loud.Note(69, 0, 0);
    for(unsigned i = 0; i < 6000; ++i) loud.Process();
    assert(loud.Active() == 0);
    Synth dark, bright; dark.Init(48000); bright.Init(48000);
    p = Instrument(); bright.Configure(p); p.cutoff = 0; dark.Configure(p);
    bright.Note(100,127,0); dark.Note(100,127,0);
    float bright_energy = 0, dark_energy = 0;
    for(unsigned i = 0; i < 10000; ++i) {
        const float a = bright.Process(), b = dark.Process();
        if(i > 5000) { bright_energy += a*a; dark_energy += b*b; }
    }
    assert(dark_energy < bright_energy * .01f);
}
void RoutingPanicAndBounds() {
    Engine engine; std::vector<float> l(48002), r(48002); assert(engine.Init(48000,l.data(),r.data(),l.size()));
    auto p = Instrument(); assert(engine.ApplyPatch(p)); float left, right;
    for(unsigned i = 0; i < 9600; ++i) { engine.Process(1,-1,left,right); assert(left == 0 && right == 0); }
    engine.Note(60,100,0);
    float peak = 0;
    for(unsigned i = 0; i < 4800; ++i) { engine.Process(0,0,left,right); peak = std::max(peak,std::fabs(left)); assert(left == right); }
    assert(peak > .05f);
    auto invalid = p; invalid.cutoff = std::numeric_limits<float>::quiet_NaN();
    assert(!engine.ApplyPatch(invalid) && engine.ActiveVoices() == 1);
    p.mix = 1; p.feedback = 1; assert(engine.ApplyPatch(p));
    for(unsigned i = 0; i < 48000; ++i) engine.Process(0,0,left,right);
    engine.Panic(); assert(engine.ActiveVoices() == 0);
    for(unsigned i = 0; i < 96000; ++i) { engine.Process(0,0,left,right); assert(left == 0 && right == 0); }
    for(unsigned wave = 0; wave < 4; ++wave) {
        p.waveform = wave; p.mix = .3f; assert(engine.ApplyPatch(p));
        for(unsigned note = 0; note < 128; ++note) {
            engine.Note(note,127,0);
            for(unsigned i = 0; i < 100; ++i) {
                engine.Process(0,0,left,right); assert(std::isfinite(left) && std::fabs(left) <= 1 && left == right);
            }
        }
    }
    Parameters fx; fx.level = 1; assert(engine.ApplyPatch(fx)); assert(engine.ActiveVoices() == 0);
    for(unsigned i = 0; i < 48000; ++i) engine.Process(.2f,-.3f,left,right);
    assert(std::fabs(left-.2f) < .0001f && std::fabs(right+.3f) < .0001f);
}
int main() { VoicesAndPitch(); EnvelopeVelocityAndFilter(); RoutingPanicAndBounds();
    std::cout << "PASS: synth pitch, ADSR, velocity, filter, source ownership, voices, routing, panic, bounds\n"; }
