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
// Mirrors the firmware audio-callback loop: notes queued before an emergency are
// dropped, control requests still run, and notes queued after it play at once,
// even when the queue never drains.
struct Callback {
    Engine& engine; RecoveryGate gate; unsigned replies = 0;
    void Block(uint8_t epoch, std::vector<Request> queue) {
        gate.Observe(epoch, engine);
        for(const auto& request : queue) {
            if(!gate.Admit(request, engine)) continue;
            Response response; if(ExecuteRequest(request, engine, response)) ++replies;
        }
    }
};
Request NoteRequest(uint8_t n, uint8_t v, uint8_t epoch) {
    Request q; q.kind = RequestKind::Note; q.note = n; q.velocity = v; q.source = 1; q.epoch = epoch; return q;
}
void RecoveryAfterLostNotes() {
    std::vector<float> l(48002), r(48002); Engine engine;
    assert(engine.Init(48000, l.data(), r.data(), l.size()));
    Parameters p = Instrument(); assert(engine.ApplyPatch(p));
    Callback cb{engine, RecoveryGate{}, 0};
    cb.Block(0, {NoteRequest(60, 100, 0)}); assert(engine.ActiveVoices() == 1); // its note-off is "lost"
    Request status; status.kind = RequestKind::Status; status.epoch = 0;
    // Emergency 1 raised: stale epoch-0 notes and a status are still queued,
    // fresh epoch-1 notes are queued behind them in the same block.
    cb.Block(1, {NoteRequest(62, 100, 0), status, NoteRequest(64, 100, 0), NoteRequest(67, 100, 1)});
    assert(cb.replies == 1 && engine.ActiveVoices() == 1);    // only 67 sounds
    // Request stamped with a newer epoch than the block start (race): silence first, then play.
    cb.Block(1, {NoteRequest(69, 100, 2)});
    assert(cb.gate.Epoch() == 2 && engine.ActiveVoices() == 1);
    cb.Block(2, {NoteRequest(69, 0, 2)});
    float left, right; for(unsigned i = 0; i < 48000; ++i) engine.Process(0, 0, left, right);
    assert(engine.ActiveVoices() == 0);                       // releases normally
    // CC120 with nothing queued still silences on the next block.
    cb.Block(2, {NoteRequest(72, 100, 2)}); assert(engine.ActiveVoices() == 1);
    cb.Block(3, {}); assert(engine.ActiveVoices() == 0);
    // Wraparound: 255 -> 0 is newer, 0 -> 255 is older.
    RecoveryGate gate; for(unsigned e = 1; e <= 255; ++e) gate.Observe(uint8_t(e), engine);
    assert(gate.Epoch() == 255);
    engine.Note(60, 100, 1); gate.Observe(0, engine); assert(gate.Epoch() == 0 && engine.ActiveVoices() == 0);
    assert(!gate.Admit(NoteRequest(60, 100, 255), engine));
    assert(gate.Admit(NoteRequest(60, 100, 0), engine));
    Request old_status = status; old_status.epoch = 255; assert(gate.Admit(old_status, engine));
}
// Worst sample-to-sample step after reusing a sounding voice, relative to the
// steady-state worst step of the same chord. Sine makes discontinuities obvious.
float StepRatio(bool retrigger, uint8_t new_velocity) {
    Synth s; s.Init(48000); auto p = Instrument(); p.attack = 0.002f; p.sustain = 0.8f; s.Configure(p);
    for(uint8_t n : {60, 64, 67, 71}) s.Note(n, 127, 0);
    float prior = 0, steady = 0, jump = 0;
    for(unsigned i = 0; i < 24000; ++i) { const float y = s.Process(); if(i > 12000) steady = std::max(steady, std::fabs(y - prior)); prior = y; }
    s.Note(retrigger ? 64 : 74, new_velocity, 0);
    for(unsigned i = 0; i < 480; ++i) { const float y = s.Process(); jump = std::max(jump, std::fabs(y - prior)); prior = y; }
    return jump / steady;
}
void ClickFreeStealAndRetrigger() {
    assert(StepRatio(false, 127) < 1.5f);  // steal (was ~5x before level/phase carry-over)
    assert(StepRatio(true, 127) < 1.5f);   // same-note retrigger
    assert(StepRatio(false, 20) < 1.5f);   // steal by a much softer note
    // A releasing voice is stolen before any held voice.
    Synth s; s.Init(48000); auto p = Instrument(); p.release = 0.5f; s.Configure(p);
    for(uint8_t n : {60, 62, 64, 65}) s.Note(n, 100, 0);
    for(unsigned i = 0; i < 480; ++i) s.Process();
    s.Note(64, 0, 0); for(unsigned i = 0; i < 480; ++i) s.Process();
    s.Note(70, 100, 0); assert(s.Active() == 4);
    // Short release from here on: if 70 replaced the releasing 64, every voice
    // now ends quickly; had it stolen held 60, slow-releasing 64 would remain.
    p.release = 0; s.Configure(p);
    for(uint8_t n : {60, 62, 65, 70}) s.Note(n, 0, 0);
    for(unsigned i = 0; i < 2400; ++i) s.Process();
    assert(s.Active() == 0);
}
// Non-harmonic energy below 12 kHz relative to harmonic energy, in dB.
double AliasDb(uint8_t waveform, uint8_t note) {
    Synth s; s.Init(48000); auto p = Instrument(); p.waveform = waveform; s.Configure(p);
    s.Note(note, 127, 0);
    const unsigned N = 4096; std::vector<double> x(N);
    for(unsigned i = 0; i < 12000 - N; ++i) s.Process();
    for(unsigned i = 0; i < N; ++i) {
        const double a = 2 * M_PI * i / (N - 1);
        x[i] = (0.35875 - 0.48829 * std::cos(a) + 0.14128 * std::cos(2 * a) - 0.01168 * std::cos(3 * a)) * s.Process();
    }
    const double f0 = 440 * std::pow(2.0, (note - 69) / 12.0), bin = 48000.0 / N;
    double harmonic = 0, alias = 0;
    for(unsigned k = 1; k * bin < 12000; ++k) {
        double re = 0, im = 0, c = 1, sn = 0; const double dc = std::cos(2 * M_PI * k / N), ds = std::sin(2 * M_PI * k / N);
        for(unsigned i = 0; i < N; ++i) { re += x[i] * c; im -= x[i] * sn; const double t = c * dc - sn * ds; sn = sn * dc + c * ds; c = t; }
        const double f = k * bin, m = std::round(f / f0), power = re * re + im * im;
        (m >= 1 && std::fabs(f - m * f0) < 5 * bin ? harmonic : alias) += power;
    }
    return 10 * std::log10(alias / harmonic);
}
void TriangleAliasing() {
    // Naive corners measured -46.9 dB (C7) and -36.2 dB (C8); polyBLAMP about -79 and -62.
    assert(AliasDb(1, 96) < -70);
    assert(AliasDb(1, 108) < -55);
    assert(AliasDb(0, 96) < -80);  // sine: measurement floor sanity check
}
int main() { VoicesAndPitch(); EnvelopeVelocityAndFilter(); RoutingPanicAndBounds(); RecoveryAfterLostNotes();
    ClickFreeStealAndRetrigger(); TriangleAliasing();
    std::cout << "PASS: synth pitch, ADSR, velocity, filter, source ownership, voices, routing, panic, bounds, recovery gate, click-free steal, triangle aliasing\n"; }
