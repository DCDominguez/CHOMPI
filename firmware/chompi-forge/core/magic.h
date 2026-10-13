#pragma once
#include <cstdint>
#include "fx.h"
#include "harmony.h"
#include "parameters.h"
#include "parts.h"

// The magic batch (0.17, the owner 2026-10-11): "surprise me" (a new sound rolled within musical
// bounds), "mutate" (the current sound nudged a little) and the start-up chime. Pure and
// allocation-free: the panel runs them in the audio callback, between blocks, and the
// patch they make goes through Engine::ApplyPatch like any other.
namespace forge {
namespace magic {

// xorshift32: cheap and good enough for sound choices. Stir() mixes in panel timing so
// each start rolls differently.
struct Rng {
    uint32_t s = 0x9e3779b9u;
    uint32_t Next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float Unit() { return static_cast<float>(Next() >> 8) * (1.f / 16777216.f); }
    float Range(float a, float b) { return a + (b - a) * Unit(); }
    unsigned Pick(unsigned n) { return Next() % n; }
    bool Chance(float p) { return Unit() < p; }
    void Stir(uint32_t v) { s ^= v * 2654435761u; if(!s) s = 1; Next(); }
};

// The FX section in a roll (0.20, the owner: "does our random patch generator take advantage of the new fx").
// Half the rolls turn one or two modules up, each with a random effect at a moderate amount (Colour and
// Texture lower, so fold, crush and broken stay musical; Space follows the roll's delay and reverb; Motion
// slow); about one in six of those runs Wear last or Wash first. Never side by side, never a secret.
FORGE_COLD inline void RollFx(Parameters& p, Rng& r) {
    unsigned choice[fx::kModules] = {0, 0, 0, 0};
    p.fx_colour = p.fx_texture = p.fx_motion = 0.f; p.fx_motion_rate = .3f; p.fx_colour_character = p.fx_texture_character = 0.f;
    choice[0] = r.Chance(.5f) ? 0u : r.Chance(.6f) ? 1u : 2u;                 // Space: echo + room, cloud, tape echo
    uint8_t order = 0;
    if(r.Chance(.5f)) {
        const unsigned count = r.Chance(.35f) ? 2u : 1u;
        bool used[fx::kModules] = {true, false, false, false};               // Space's amount is the roll's mix
        for(unsigned n = 0; n < count; ++n) {
            unsigned m = 1u + r.Pick(3u); if(used[m]) m = m % 3u + 1u;
            used[m] = true; choice[m] = r.Pick(3u);
            if(m == 2 && choice[2] == 1 && p.voices == 1) choice[2] = 0;      // no dropouts on a mono lead (too quiet)
            if(m == 1) {                                                       // Colour: saturation, fold, crush
                static const float top[3] = {.45f, .35f, .3f};
                p.fx_colour = r.Range(.12f, top[choice[1]]); p.fx_colour_character = r.Range(0.f, .5f);
            } else if(m == 2) {                                                // Texture: cassette, broken, squash
                static const float top[3] = {.5f, .35f, .5f};
                p.fx_texture = r.Range(.15f, top[choice[2]]); p.fx_texture_character = r.Range(0.f, choice[2] == 2 ? .6f : .4f);
            } else {                                                           // Motion: chorus, phaser, tremolo
                static const float top[3] = {.5f, .45f, .35f};
                p.fx_motion = r.Range(.18f, top[choice[3]]); p.fx_motion_rate = r.Range(.12f, .5f);
            }
        }
        if(r.Chance(1.f / 6.f)) order = fx::OrderIndex(fx::kOrder[1u + r.Pick(2u)]);
    }
    p.fx = static_cast<uint16_t>(choice[0] | choice[1] << 2 | choice[2] << 4 | choice[3] << 6 | static_cast<unsigned>(order) << 8);
}
// Mutate's FX drift: the amounts move like everything else (an effect that's off stays off); now and then
// one module's effect changes within its category. The order and side by side stay.
FORGE_COLD inline void MutateFx(Parameters& p, Rng& r) {
    float* amounts[3] = {&p.fx_colour, &p.fx_texture, &p.fx_motion};
    for(float* a : amounts) if(*a > 0.f) *a = Clamp(*a + r.Range(-.06f, .06f), .02f, *a > .55f ? *a : .55f);
    if(p.fx_motion > 0.f) p.fx_motion_rate = Clamp(p.fx_motion_rate + r.Range(-.06f, .06f), .05f, .8f);
    if(r.Chance(.1f)) {
        const unsigned m = 1u + r.Pick(3u), shift = 2u * m;
        p.fx = static_cast<uint16_t>((p.fx & ~(3u << shift)) | (r.Pick(3u) << shift));
    }
}

// Normalised 0..1 fields (core/parameters.h; physical ranges in host/forge_host.py).
// Bounds keep every roll playable: no near-silent attack, no screaming resonance at a high
// cutoff, delay feedback well below runaway, the output level the player already chose.
FORGE_COLD inline Parameters Roll(const Parameters& base, Rng& r) {
    Parameters p;
    p.version = 8; p.synth = true; p.source = 0;
    p.waveform = static_cast<uint8_t>(r.Pick(4));
    p.osc2_waveform = r.Chance(.6f) ? p.waveform : static_cast<uint8_t>(r.Pick(4));
    static const int8_t intervals[7] = {0, 0, 7, 12, -12, 5, 19};
    p.osc2_semitones = static_cast<uint8_t>(24 + intervals[r.Pick(7)]);
    p.osc2_level = r.Chance(.25f) ? 0.f : r.Range(.15f, .6f);
    p.osc2_detune = .5f + (r.Chance(.5f) ? 1.f : -1.f) * r.Range(0.f, .15f);   // up to 15 cents
    p.noise = r.Chance(.15f) ? r.Range(.01f, .08f) : 0.f;
    const float a = r.Unit();
    p.attack = .001f + .15f * a * a * a;                      // 3 ms .. 300 ms, mostly short
    p.decay = r.Range(.025f, .75f);
    p.sustain = r.Range(.4f, 1.f);
    p.release = r.Range(.015f, .5f);                          // 80 ms .. 2.5 s
    p.cutoff = r.Range(.45f, .9f);                            // ~600 Hz .. 9 kHz (log)
    p.resonance = r.Range(0.f, p.cutoff > .75f ? .35f : .6f);
    p.filter_amount = r.Range(.48f, .75f);                    // -0.25 .. +3 octaves: mostly opening
    p.filter_attack = .001f + .3f * r.Unit() * r.Unit();
    p.filter_decay = r.Range(.025f, .75f);
    p.filter_sustain = r.Range(0.f, .8f);
    p.filter_release = r.Range(.015f, .5f);
    if(r.Chance(.5f)) {
        p.lfo_waveform = static_cast<uint8_t>(r.Pick(3));
        p.lfo_rate = r.Range(.23f, .8f);                      // ~0.2 .. 6 Hz
        p.lfo_pitch = r.Chance(.5f) ? r.Range(0.f, .075f) : 0.f;
        p.lfo_filter = r.Range(0.f, .375f);
        p.lfo_amp = r.Chance(.3f) ? r.Range(0.f, .3f) : 0.f;
    }
    p.mix = r.Range(0.f, .35f); p.time = r.Range(.14f, .6f); p.feedback = r.Range(0.f, .53f);
    p.reverb_mix = r.Range(.1f, .45f); p.reverb_size = r.Range(.3f, .85f); p.reverb_damping = r.Range(.3f, .7f);
    if(r.Chance(.2f)) { p.voices = 1; p.glide = r.Range(.015f, .075f); }   // a gliding mono lead
    else { static const uint8_t poly[3] = {4, 6, 7}; p.voices = poly[r.Pick(3)]; }
    p.level = base.level;
    // Chomp to instrument (0.18): rolling over a sampler patch keeps the sample (your recording or
    // a card slot, its window and pitch) and rolls the instrument around it, with chords and the
    // arp more often. Kit patches keep their pads and get no chords or arp.
    const bool sample = base.Sampler(), kit = sample && base.sample_mode == 1;
    if(sample) {
        p.source = 1; p.sample_mode = base.sample_mode; p.sample_bank = base.sample_bank; p.sample_slot = base.sample_slot;
        p.sample_pitch = base.sample_pitch; p.sample_start = base.sample_start; p.sample_end = base.sample_end;
        p.sample_xfade = base.sample_xfade; p.sample_loop = base.sample_loop; p.sample_gate = base.sample_gate;
        p.sample_reverse = base.sample_reverse;
    }
    const float harmony_chance = kit ? 0.f : sample ? .6f : .25f, arp_chance = kit ? 0.f : sample ? .5f : .2f;
    // Harmony (25 %; 60 % around a sample) in the player's key; the arp (20 %; 50 % around a
    // sample), never latched, so it stops with the keys.
    harmony::State h; h.tonic = static_cast<uint8_t>(base.version >= 6 ? (base.harmony & 15u) % 12u : 0u);
    if(r.Chance(harmony_chance)) {
        static const harmony::Mode modes[5] = {harmony::Mode::Major, harmony::Mode::NaturalMinor, harmony::Mode::Dorian,
                                               harmony::Mode::Mixolydian, harmony::Mode::Lydian};
        h.enabled = true; h.mode = modes[r.Pick(5)];
        h.extension = r.Chance(.5f) ? harmony::Extension::Triad : harmony::Extension::Seventh;
    }
    p.harmony = harmony::Pack(h);
    parts::Settings s = base.version >= 7 ? parts::Unpack(base.parts_arp, base.parts_clock) : parts::Settings{};
    s.pattern = parts::Pattern::Off; s.bass = parts::Bass::Off; s.latch = false;
    if(r.Chance(arp_chance)) {
        static const parts::Pattern patterns[3] = {parts::Pattern::Up, parts::Pattern::UpDown, parts::Pattern::Order};
        s.pattern = patterns[r.Pick(3)];
        s.rate = r.Chance(.5f) ? parts::Rate::Eighth : parts::Rate::Sixteenth;
        s.octaves = static_cast<uint8_t>(1 + r.Pick(2)); s.gate = static_cast<uint8_t>(8 + r.Pick(7));
    }
    p.parts_arp = parts::PackArp(s); p.parts_clock = parts::PackClock(s);
    RollFx(p, r);
    return p.Valid() ? p : base;
}

// Each continuous sound field moves by up to +/-6 % (within the same bounds as Roll); the
// second oscillator's interval jumps now and then. Level, sample choice, knobs, harmony
// and parts stay: the sound drifts, the instrument stays the player's.
FORGE_COLD inline Parameters Mutate(const Parameters& base, Rng& r) {
    Parameters p = base;
    struct Bound { float Parameters::* field; float lo, hi; };
    static const Bound bounds[] = {
        {&Parameters::attack, .001f, .3f}, {&Parameters::decay, .02f, .8f}, {&Parameters::sustain, .1f, 1.f},
        {&Parameters::release, .01f, .55f}, {&Parameters::cutoff, .3f, .95f}, {&Parameters::resonance, 0.f, .65f},
        {&Parameters::filter_amount, .4f, .78f}, {&Parameters::filter_attack, .001f, .3f}, {&Parameters::filter_decay, .02f, .8f},
        {&Parameters::filter_sustain, 0.f, .85f}, {&Parameters::filter_release, .01f, .55f}, {&Parameters::osc2_level, 0.f, .7f},
        {&Parameters::osc2_detune, .35f, .65f}, {&Parameters::lfo_rate, .15f, .85f}, {&Parameters::lfo_pitch, 0.f, .1f},
        {&Parameters::lfo_filter, 0.f, .45f}, {&Parameters::lfo_amp, 0.f, .35f}, {&Parameters::mix, 0.f, .45f},
        {&Parameters::time, .05f, .7f}, {&Parameters::feedback, 0.f, .6f}, {&Parameters::reverb_mix, 0.f, .55f},
        {&Parameters::reverb_size, .2f, .9f}, {&Parameters::reverb_damping, .2f, .8f}};
    for(const Bound& b : bounds) {
        float& v = p.*(b.field);
        const float lo = v < b.lo ? v : b.lo, hi = v > b.hi ? v : b.hi;     // never pull a value back into bounds it was set outside
        v = Clamp(v + r.Range(-.06f, .06f), lo, hi);
    }
    if(p.version >= 3 && p.synth && r.Chance(.15f)) {
        static const int8_t intervals[6] = {0, 7, 12, -12, 5, 19};
        p.osc2_semitones = static_cast<uint8_t>(24 + intervals[r.Pick(6)]);
    }
    if(p.resonance > .35f && p.cutoff > .8f) p.resonance = .35f;
    if(p.version >= 8) MutateFx(p, r);
    return p.Valid() ? p : base;
}

// The start-up chime: four rising notes (C4 E4 G4 C5) through the sound CHOMPI woke up
// with, and a light sweep across the keys while it plays. Frames at 48 kHz.
class Chime {
public:
    static constexpr unsigned kNotes = 4;
    static constexpr uint32_t kDelay = 48 * 150, kStep = 48 * 110, kLength = 48 * 320, kVelocity = 72;
    static constexpr uint32_t kTotal = kDelay + (kNotes - 1) * kStep + kLength + 48;   // the last note-off falls inside
    void Start() { t_ = 0; active_ = true; }
    bool Active() const { return active_; }
    float Progress() const { return active_ ? static_cast<float>(t_) / kTotal : 0.f; }
    // Calls note(midi, velocity) for every note that starts or ends within this block.
    template<typename F> void Tick(uint32_t frames, F note) {
        if(!active_) return;
        static const uint8_t notes[kNotes] = {60, 64, 67, 72};
        const uint32_t from = t_, to = t_ + frames;
        for(unsigned i = 0; i < kNotes; ++i) {
            const uint32_t on = kDelay + i * kStep, off = on + kLength;
            if(on >= from && on < to) note(notes[i], static_cast<uint8_t>(kVelocity));
            if(off >= from && off < to) note(notes[i], static_cast<uint8_t>(0));
        }
        t_ = to;
        if(t_ >= kTotal) active_ = false;
    }
private:
    uint32_t t_ = 0;
    bool active_ = false;
};

// Dream mode (0.18): left alone (no key, knob, MIDI) for 2 minutes with a playable sound and
// nothing else playing, CHOMPI plays a quiet, slow line in the current key (harmony's tonic and
// mode, used even while harmony is off), straight to the voices. Any touch ends it at once (all
// its notes released); after 3 minutes it sleeps until the next touch. Frames at 48 kHz.
class Dream {
public:
    static constexpr unsigned kMaxNotes = 3;
    static constexpr uint8_t kVelocity = 45;
    void SetTiming(uint32_t idle, uint32_t length) { idle_after_ = idle; length_ = length; }
    bool Active() const { return active_; }
    // Notes it is sounding, one bit per key C3..C5 (bit 0 = C3), folded into that range.
    uint32_t KeyMask() const {
        uint32_t m = 0;
        for(const Held& h : held_) if(h.frames) { int n = h.note; while(n > 72) n -= 12; while(n < 48) n += 12; m |= 1u << (n - 48); }
        return m;
    }
    // `quiet`: nothing touched this block; `allowed`: a playable sound, nothing else playing.
    template<typename F> void Tick(uint32_t frames, bool quiet, bool allowed, const harmony::State& key, Rng& r, F note) {
        if(!quiet || !allowed) {
            if(active_) Stop(note);
            if(!quiet) { idle_ = 0; asleep_ = false; }
            return;
        }
        if(asleep_) return;
        if(!active_) {
            idle_ += frames;
            if(idle_ < idle_after_) return;
            active_ = true; elapsed_ = 0; next_ = 0; step_ = 0; degree_ = 7;
        }
        elapsed_ += frames;
        for(Held& h : held_) if(h.frames) { if(h.frames > frames) h.frames -= frames; else { h.frames = 0; note(h.note, 0); } }
        if(elapsed_ >= length_) { Stop(note); asleep_ = true; return; }
        if(next_ > frames) { next_ -= frames; return; }
        next_ = kStep;
        const bool root = step_++ % 8 == 0;
        if(!root && r.Chance(.15f)) return;                                   // a breath
        const uint8_t* scale = harmony::kScale[static_cast<unsigned>(key.mode) % harmony::kModes];
        int midi;
        if(root) midi = 48 + key.tonic % 12;
        else {
            static const int8_t moves[6] = {-2, -1, -1, 1, 1, 2};
            degree_ += moves[r.Pick(6)];
            if(degree_ < 0) degree_ = 1;
            if(degree_ > 13) degree_ = 12;
            midi = 60 + key.tonic % 12 + scale[degree_ % 7] + 12 * (degree_ / 7);
        }
        Held* slot = &held_[0];
        for(Held& h : held_) if(!h.frames) { slot = &h; break; }
        if(slot->frames) note(slot->note, 0);
        slot->note = static_cast<uint8_t>(midi); slot->frames = root ? 2 * kNote : kNote;
        note(slot->note, kVelocity);
    }
private:
    static constexpr uint32_t kStep = 48 * 600, kNote = 48 * 900;
    struct Held { uint8_t note = 0; uint32_t frames = 0; };
    template<typename F> void Stop(F note) {
        for(Held& h : held_) if(h.frames) { h.frames = 0; note(h.note, 0); }
        active_ = false; idle_ = 0;
    }
    uint32_t idle_after_ = 48000u * 120u, length_ = 48000u * 180u, idle_ = 0, elapsed_ = 0, next_ = 0, step_ = 0;
    int degree_ = 7;
    bool active_ = false, asleep_ = false;
    Held held_[kMaxNotes];
};

} // namespace magic
} // namespace forge
