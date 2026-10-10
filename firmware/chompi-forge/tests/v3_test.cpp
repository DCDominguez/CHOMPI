// Version 3 instrument: second oscillator, noise, resonant per-voice filter with
// envelope, LFO and mod wheel, voice limit, glide and reverb.
#include <cassert>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <iostream>
#include <limits>
#include <vector>
#include "../core/runtime.h"
using namespace forge;

constexpr float kRate = 48000.f;
Parameters V3() {
    Parameters p; p.version = 3; p.synth = true; p.mix = 0; p.level = 1; p.waveform = 0;
    p.attack = p.decay = p.release = 0; p.sustain = 1; p.cutoff = 1;
    return p;
}
float Hz(float hz) { return std::log(hz / 40.f) / std::log(400.f); }  // normalized cutoff
std::vector<float> Render(Synth& s, unsigned n) { std::vector<float> o(n); for(auto& x : o) x = s.Process(); return o; }
double Power(const std::vector<float>& x, double hz, size_t from = 0, size_t to = 0) { // Goertzel
    if(!to) to = x.size();
    const double w = 2 * M_PI * hz / kRate, c = 2 * std::cos(w);
    double s1 = 0, s2 = 0;
    for(size_t i = from; i < to; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / double(to - from);
}
unsigned Crossings(const std::vector<float>& x, size_t from, size_t to) {
    unsigned n = 0; for(size_t i = from + 1; i < to; ++i) if(x[i - 1] <= 0 && x[i] > 0) ++n; return n;
}
float Peak(const std::vector<float>& x, size_t from, size_t to) {
    float p = 0; for(size_t i = from; i < to; ++i) p = std::max(p, std::fabs(x[i])); return p;
}
double Energy(const std::vector<float>& x, size_t from, size_t to) {
    double e = 0; for(size_t i = from; i < to; ++i) e += double(x[i]) * x[i]; return e / double(to - from);
}
double Roughness(const std::vector<float>& x, size_t from, size_t to) { // HF proxy: first-difference energy / energy
    double d = 0; for(size_t i = from + 1; i < to; ++i) d += double(x[i] - x[i - 1]) * (x[i] - x[i - 1]);
    return d / (Energy(x, from, to) * double(to - from));
}

void NeutralV3IsAPlainVoice() {
    Synth s; s.Init(kRate); s.Configure(V3()); s.Note(69, 127, 0);
    auto x = Render(s, 48000);
    assert(Crossings(x, 4800, 48000) >= 395 && Crossings(x, 4800, 48000) <= 397); // 440 Hz over 0.9 s
    assert(Peak(x, 4800, 48000) > 0.19f && Peak(x, 4800, 48000) < 0.21f);
}
void SecondOscillatorAndNoise() {
    auto p = V3(); p.osc2_semitones = 36; p.osc2_level = 1;                   // +12 semitones
    Synth s; s.Init(kRate); s.Configure(p); s.Note(57, 127, 0);                  // 220 Hz
    auto x = Render(s, 24000);
    assert(Power(x, 440, 4800) > 0.3 * Power(x, 220, 4800));
    p.osc2_level = 0; Synth off; off.Init(kRate); off.Configure(p); off.Note(57, 127, 0);
    auto y = Render(off, 24000);
    assert(Power(y, 440, 4800) < 1e-4 * Power(y, 220, 4800));
    p = V3(); p.osc2_level = 1; p.osc2_detune = 1;                             // +50 cents
    Synth d; d.Init(kRate); d.Configure(p); d.Note(57, 127, 0);
    auto z = Render(d, 48000);
    assert(Power(z, 220 * std::pow(2.0, 0.5 / 12), 4800) > 0.3 * Power(z, 220, 4800));
    p = V3(); p.noise = 1;
    Synth n; n.Init(kRate); n.Configure(p); n.Note(57, 127, 0);
    auto w = Render(n, 24000);
    assert(Power(w, 5000, 4800) > 100 * Power(y, 5000, 4800));
    assert(Peak(w, 0, w.size()) < 0.25f);     // normalized: unnormalized sine+noise would reach ~0.4
}
void ResonantFilterAndEnvelope() {
    // Saw at 110 Hz into a 990 Hz cutoff: resonance boosts the 9th harmonic.
    auto p = V3(); p.waveform = 2; p.cutoff = Hz(990);
    Synth flat; flat.Init(kRate); flat.Configure(p); flat.Note(45, 127, 0);
    p.resonance = 0.9f;
    Synth peak; peak.Init(kRate); peak.Configure(p); peak.Note(45, 127, 0);
    auto a = Render(flat, 24000), b = Render(peak, 24000);
    const double ra = Power(a, 990, 4800) / Power(a, 110, 4800), rb = Power(b, 990, 4800) / Power(b, 110, 4800);
    assert(rb > 4 * ra);
    // A higher cutoff is brighter than a lower one (filter actually tracks cutoff).
    p = V3(); p.waveform = 2; p.cutoff = Hz(300);
    Synth dark; dark.Init(kRate); dark.Configure(p); dark.Note(45, 127, 0);
    auto c = Render(dark, 24000);
    assert(Power(c, 3080, 4800) < 0.02 * Power(a, 3080, 4800));             // measured ~0.008
    // Filter envelope: +6 octaves decaying to 0 makes the start much brighter than the tail.
    p = V3(); p.waveform = 2; p.cutoff = Hz(150); p.filter_amount = 1; p.filter_attack = 0;
    p.filter_decay = 0.1f; p.filter_sustain = 0;
    Synth env; env.Init(kRate); env.Configure(p); env.Note(45, 127, 0);
    auto e = Render(env, 48000);
    assert(Roughness(e, 0, 960) > 5 * Roughness(e, 36000, 48000));
    // Negative amount closes the filter below its cutoff at the start.
    p.filter_amount = 0; p.cutoff = Hz(4000); p.filter_sustain = 0;
    Synth neg; neg.Init(kRate); neg.Configure(p); neg.Note(45, 127, 0);
    auto f = Render(neg, 48000);
    assert(Roughness(f, 0, 960) < 0.5 * Roughness(f, 36000, 48000));
}
void LfoAndModWheel() {
    auto p = V3(); p.lfo_waveform = 2; p.lfo_rate = std::log(10.f) / std::log(400.f); p.lfo_pitch = 0.5f; // square 0.5 Hz, 100 c
    Synth s; s.Init(kRate); s.Configure(p); s.Note(69, 127, 0);
    auto x = Render(s, 96000);
    const unsigned up = Crossings(x, 9600, 38400), down = Crossings(x, 57600, 86400);  // 0.6 s windows
    assert(up >= 277 && up <= 282 && down >= 246 && down <= 251);            // 466 Hz / 415 Hz
    p.lfo_wheel = true;                                                       // wheel at rest: no vibrato
    Synth w; w.Init(kRate); w.Configure(p); w.Note(69, 127, 0);
    auto y = Render(w, 96000);
    assert(Crossings(y, 9600, 38400) >= 263 && Crossings(y, 9600, 38400) <= 265);
    assert(Crossings(y, 57600, 86400) >= 263 && Crossings(y, 57600, 86400) <= 265);
    w.ModWheel(127); auto z = Render(w, 96000);
    const unsigned zu = Crossings(z, 9600, 38400), zd = Crossings(z, 57600, 86400);
    assert((zu > 270) != (zd > 270));                                         // modulation back on
    w.Silence(); w.Note(69, 127, 0); auto q = Render(w, 96000);               // panic resets the wheel
    assert(Crossings(q, 57600, 86400) >= 263 && Crossings(q, 57600, 86400) <= 265);
    // Tremolo: square LFO at full depth mutes alternate half cycles.
    p = V3(); p.lfo_waveform = 2; p.lfo_rate = std::log(10.f) / std::log(400.f); p.lfo_amp = 1;
    Synth t; t.Init(kRate); t.Configure(p); t.Note(69, 127, 0);
    auto a = Render(t, 96000);
    assert(Peak(a, 57600, 86400) < 0.01f * Peak(a, 9600, 38400));
    // Every LFO shape stays bounded with all destinations at full depth.
    for(uint8_t shape = 0; shape < 4; ++shape) {
        p = V3(); p.waveform = 2; p.lfo_waveform = shape; p.lfo_rate = 1; p.lfo_pitch = 1; p.lfo_filter = 1;
        p.lfo_amp = 1; p.resonance = 1;
        Synth b; b.Init(kRate); b.Configure(p); for(uint8_t n : {40, 60, 80, 100}) b.Note(n, 127, 0);
        for(float v : Render(b, 24000)) assert(std::isfinite(v) && std::fabs(v) <= 1.f);
    }
}
void VoiceLimitAndGlide() {
    auto p = V3(); p.voices = 1;
    Synth s; s.Init(kRate); s.Configure(p);
    for(uint8_t n : {60, 64, 67}) s.Note(n, 100, 0);
    assert(s.Active() == 1);
    p.voices = 2; s.Configure(p); s.Note(72, 100, 0); assert(s.Active() == 2);
    p.voices = 4; s.Configure(p); for(uint8_t n : {40, 41, 42}) s.Note(n, 100, 0); assert(s.Active() == 4);
    p.voices = 1; p.release = 0; s.Configure(p);                              // shrinking releases the extras
    Render(s, 4800); assert(s.Active() == 1);
    // Glide 200 ms: right after the jump the pitch is still far below A440.
    p = V3(); p.voices = 1; p.glide = 0.1f;
    Synth g; g.Init(kRate); g.Configure(p); g.Note(57, 100, 0); Render(g, 9600);
    g.Note(69, 100, 0); auto x = Render(g, 48000);
    assert(Crossings(x, 0, 2400) < 17);                                       // < 340 Hz in first 50 ms
    const unsigned settled = Crossings(x, 24000, 48000);
    assert(settled >= 219 && settled <= 221);                                 // 440 Hz after 0.5 s
    p.glide = 0; Synth i; i.Init(kRate); i.Configure(p); i.Note(57, 100, 0); Render(i, 9600);
    i.Note(69, 100, 0); auto y = Render(i, 4800);
    assert(Crossings(y, 0, 2400) >= 21);                                      // no glide: ~440 Hz at once
    // Every glide ends exactly on its target (0.13): the old 1e-9 rule let slow glides stall
    // up to ~1.2 cents short and keep running every sample.
    for(const float seconds : {0.01f, 0.08f, 0.5f, 2.f}) for(const float from : {0.25f, 1.f, 3.7f})
        for(const float target : {0.001f, 0.0372f, 0.5f, 1.f, 1.4983f, 7.9f}) {
            const float slew = 1.f - std::exp(-1.f / (seconds * 0.25f * kRate)), snap = GlideSnap(slew);
            float x = from * target, prev = x; int n = 0;
            while(x != target && n < 40 * static_cast<int>(seconds * kRate) + 100) { prev = x; GlideStep(x, target, slew, snap); ++n; }
            assert(x == target);
            // The last jump: no more than the old rule's stall left for good (~1.2 cents per
            // second of glide), and only 1e-6 for short glides.
            assert(std::fabs(prev / target - 1.f) <= std::fmax(1.2e-6f, 1.5e-3f * seconds / 2.f));
        }
}
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(kRate));
    Engine engine;
    explicit Rig(bool reverb = true) {
        assert(reverb ? engine.Init(kRate, l.data(), r.data(), l.size(), rv.data(), rv.size())
                      : engine.Init(kRate, l.data(), r.data(), l.size()));
    }
    // Feed one impulse after settling, return the stereo response.
    void Impulse(std::vector<float>& left, std::vector<float>& right, unsigned n) {
        float a, b; for(unsigned i = 0; i < 4800; ++i) engine.Process(0, 0, a, b);
        left.resize(n); right.resize(n);
        for(unsigned i = 0; i < n; ++i) engine.Process(i == 0 ? 1.f : 0.f, i == 0 ? 1.f : 0.f, left[i], right[i]);
    }
};
Parameters ReverbPatch(float size, float damping) {
    Parameters p; p.version = 3; p.synth = false; p.mix = 0; p.level = 1;
    p.reverb_mix = 1; p.reverb_size = size; p.reverb_damping = damping; return p;
}
void ReverbTail() {
    std::vector<float> L, R, L2, R2;
    Rig big; assert(big.engine.ApplyPatch(ReverbPatch(1, 0.3f))); big.Impulse(L, R, 96000);
    Rig small; assert(small.engine.ApplyPatch(ReverbPatch(0, 0.3f))); small.Impulse(L2, R2, 96000);
    assert(Energy(L, 48000, 96000) > 100 * Energy(L2, 48000, 96000) + 1e-12);  // bigger size, longer tail
    assert(Energy(L, 4800, 9600) > 0 && Energy(R, 4800, 9600) > 0);
    double diff = 0; for(size_t i = 0; i < 9600; ++i) diff += std::fabs(L[i] - R[i]);
    assert(diff > 0.01);                                                       // decorrelated stereo
    // Damping darkens the tail.
    Rig dark; assert(dark.engine.ApplyPatch(ReverbPatch(0.7f, 1))); dark.Impulse(L2, R2, 48000);
    Rig bright; assert(bright.engine.ApplyPatch(ReverbPatch(0.7f, 0))); bright.Impulse(L, R, 48000);
    assert(Roughness(L2, 4800, 24000) < 0.5 * Roughness(L, 4800, 24000));
    // Panic clears the tail completely and at once.
    bright.engine.Panic(); float a, b;
    for(unsigned i = 0; i < 96000; ++i) { bright.engine.Process(0, 0, a, b); assert(a == 0.f && b == 0.f); }
    // Worst case: longest decay, no damping, loud noise for 1 s, then 10 s of silence.
    Rig worst; assert(worst.engine.ApplyPatch(ReverbPatch(1, 0)));
    uint32_t seed = 1; double first = 0, last = 0;
    for(unsigned i = 0; i < 11 * 48000; ++i) {
        seed = seed * 1664525u + 1013904223u;
        const float in = i < 48000 ? (static_cast<int32_t>(seed) * (1.f / 2147483648.f)) : 0.f;
        worst.engine.Process(in, in, a, b);
        assert(std::isfinite(a) && std::fabs(a) <= 1 && std::isfinite(b) && std::fabs(b) <= 1);
        if(i >= 48000 && i < 96000) first += double(a) * a;
        if(i >= 10 * 48000) last += double(a) * a;
    }
    assert(last < 0.01 * first);                                               // it decays
}
void ReverbIgnoresUninitializedMemory() {
    // Firmware places reverb memory in DTCM, which is not zeroed at boot. Garbage
    // (NaN, huge values) must never reach the output: the result has to match
    // zeroed memory exactly, before and after a panic.
    Rig clean, dirty;
    std::fill(dirty.rv.begin(), dirty.rv.end(), std::numeric_limits<float>::quiet_NaN());
    for(size_t i = 0; i < dirty.rv.size(); i += 3) dirty.rv[i] = 1e30f;
    assert(dirty.engine.Init(kRate, dirty.l.data(), dirty.r.data(), dirty.l.size(), dirty.rv.data(), dirty.rv.size()));
    for(Rig* rig : {&clean, &dirty}) assert(rig->engine.ApplyPatch(ReverbPatch(1, 0.2f)));
    std::vector<float> a, b, c, d;
    clean.Impulse(a, b, 48000); dirty.Impulse(c, d, 48000);
    assert(a == c && b == d);
    clean.engine.Panic(); dirty.engine.Panic();
    clean.Impulse(a, b, 48000); dirty.Impulse(c, d, 48000);
    assert(a == c && b == d);
    for(float v : c) assert(std::isfinite(v));
}
void ReverbMixAndCompatibility() {
    // Reverb mix 0 is exactly the dry path; no reverb memory is exactly dry too.
    for(int variant = 0; variant < 2; ++variant) {
        Rig with(variant == 0); Rig without(true);
        auto p = ReverbPatch(0.5f, 0.5f); if(variant == 0) p.reverb_mix = 0;
        auto q = ReverbPatch(0.5f, 0.5f); q.reverb_mix = 0;
        assert(with.engine.ApplyPatch(p) && without.engine.ApplyPatch(q));
        float a, b, c, d;
        for(unsigned i = 0; i < 48000; ++i) {
            const float in = std::sin(i * 0.01f) * 0.5f;
            with.engine.Process(in, -in, a, b); without.engine.Process(in, -in, c, d);
            assert(a == c && b == d);
        }
    }
    // CC91 reverb mix works live on v3 and is refused on v2.
    Rig rig; auto p = ReverbPatch(0.5f, 0.5f); p.reverb_mix = 0; assert(rig.engine.ApplyPatch(p));
    assert(rig.engine.Apply({Parameter::ReverbMix, 0.6f}) && rig.engine.GetParameters().reverb_mix == 0.6f);
    assert(rig.engine.Apply({Parameter::Resonance, 0.4f}) && rig.engine.GetParameters().resonance == 0.4f);
    Parameters v2; v2.version = 2; v2.synth = true; assert(rig.engine.ApplyPatch(v2));
    assert(!rig.engine.Apply({Parameter::ReverbMix, 0.6f}) && rig.engine.GetParameters().reverb_mix == 0.f);
}
void StructuralChangesAndFuzz() {
    Rig rig; auto p = V3(); p.waveform = 2; assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 100, 0); assert(rig.engine.ActiveVoices() == 1);
    p.resonance = 0.5f; p.osc2_level = 0.3f; assert(rig.engine.ApplyPatch(p));
    assert(rig.engine.ActiveVoices() == 1);                                   // ordinary v3 edit keeps playing
    Parameters v2; v2.version = 2; v2.synth = true; v2.waveform = 2; assert(rig.engine.ApplyPatch(v2));
    assert(rig.engine.ActiveVoices() == 0);                                   // v3 -> v2 architecture: silenced
    auto bad = V3(); bad.voices = 0; assert(!rig.engine.ApplyPatch(bad));
    bad = V3(); bad.lfo_rate = NAN; assert(!rig.engine.ApplyPatch(bad));
    assert(rig.engine.GetParameters().version == 2);                          // rejected patches change nothing
    // Seeded fuzz over every v3 field: output stays finite and bounded.
    uint32_t seed = 99; auto unit = [&]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.f; };
    for(unsigned trial = 0; trial < 60; ++trial) {
        Parameters q = V3(); q.synth = unit() < 0.8f;
        q.waveform = uint8_t(unit() * 4); q.osc2_waveform = uint8_t(unit() * 4); q.osc2_semitones = uint8_t(unit() * 49);
        q.lfo_waveform = uint8_t(unit() * 4); q.voices = uint8_t(1 + unit() * 4); q.lfo_wheel = unit() < 0.5f;
        for(float* f : {&q.attack, &q.decay, &q.sustain, &q.release, &q.cutoff, &q.mix, &q.time, &q.feedback, &q.level,
                        &q.osc2_level, &q.osc2_detune, &q.noise, &q.glide, &q.resonance, &q.filter_amount,
                        &q.filter_attack, &q.filter_decay, &q.filter_sustain, &q.filter_release, &q.lfo_rate,
                        &q.lfo_pitch, &q.lfo_filter, &q.lfo_amp, &q.reverb_mix, &q.reverb_size, &q.reverb_damping})
            *f = unit() < 0.1f ? (unit() < 0.5f ? 0.f : 1.f) : unit();
        assert(q.Valid() && rig.engine.ApplyPatch(q));
        rig.engine.ModWheel(uint8_t(unit() * 128)); rig.engine.Bend(0, uint16_t(unit() * 16384));
        float a, b;
        for(unsigned i = 0; i < 2400; ++i) {
            if(i % 300 == 0) rig.engine.Note(uint8_t(unit() * 128), uint8_t(1 + unit() * 126), 0);
            if(i % 300 == 150) rig.engine.Note(uint8_t(unit() * 128), 0, 0);
            rig.engine.Process(unit() - 0.5f, unit() - 0.5f, a, b);
            assert(std::isfinite(a) && std::fabs(a) <= 1 && std::isfinite(b) && std::fabs(b) <= 1);
        }
    }
}
int main() {
    NeutralV3IsAPlainVoice(); SecondOscillatorAndNoise(); ResonantFilterAndEnvelope(); LfoAndModWheel();
    VoiceLimitAndGlide(); ReverbTail(); ReverbIgnoresUninitializedMemory(); ReverbMixAndCompatibility();
    StructuralChangesAndFuzz();
    std::cout << "PASS: v3 oscillators/noise, resonant filter + envelope, LFO/mod wheel, voices/glide, reverb, compatibility, fuzz\n";
}
