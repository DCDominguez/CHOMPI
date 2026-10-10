#pragma once
#include <cstdint>
#include "command_queue.h"
#include "parameters.h"

// Musical parts (docs/forge/HARMONY_BRIEF.md Phase 2, firmware 0.14): a clock, an
// arpeggiator and a bass part. Keys (or harmony chords) give a note set; the arp plays
// it one note at a time on the clock, the bass plays the chord's root (fifth, octave)
// in its own register. Generated notes go to the synth (one owner per pitch, so the arp
// and bass never cut each other) and to MIDI out (arp channel 1, bass channel 2, as
// options.json's "Midi Out Channel" + part). Tempo is internal (tap, knob, patch) or
// follows MIDI clock while it arrives; the internal clock is sent out (24 PPQN).
// Audio owner only: Advance() once per audio block, Key() at note on/off. Fixed arrays,
// no allocation, bounded loops; randomness is a seeded xorshift (reproducible).
// Original Forge code; TEMPO's arp/clock are behavioural references only.
namespace forge {
// One MIDI message for the main loop to send (UART and USB). Channel messages carry the
// part in the channel nibble (0 keys/arp, 1 bass), added to the output channel when
// sent; real-time bytes (F8 clock, FA start, FC stop) have status only.
struct MidiOut { uint8_t status = 0, data1 = 0, data2 = 0; };

namespace parts {
enum class Pattern : uint8_t { Off, Up, Down, UpDown, Order, Random };
enum class Rate : uint8_t { Quarter, Eighth, EighthTriplet, Sixteenth, SixteenthTriplet, ThirtySecond };
enum class Bass : uint8_t { Off, Root, Fifth, Alternate, Octave };
enum class BassRate : uint8_t { Chord, Half, Quarter, Eighth };
constexpr unsigned kPatterns = 6, kRates = 6, kBassModes = 5, kBassRates = 4;
constexpr unsigned kPpqn = 24;                       // MIDI clock ticks per quarter note
constexpr uint8_t kRateTicks[kRates] = {24, 12, 8, 6, 4, 3};
constexpr uint8_t kBassTicks[kBassRates] = {0, 48, 24, 12};
constexpr uint16_t kMinBpm = 40, kMaxBpm = 300, kDefaultBpm = 120;
constexpr unsigned kMaxSet = 16;                      // notes the arp cycles through (before octaves)

// Zero-initialised = everything off at 120 BPM (Construct / .bss); Pack/Unpack carry it in
// patch v7 as two 21-bit words (PROTOCOL.md).
struct Settings {
    Pattern pattern = Pattern::Off;
    Rate rate = Rate::Eighth;
    uint8_t octaves = 1;          // 1-4
    uint8_t gate = 10;            // 1-20: 5 %..100 % of a step
    bool latch = true;            // the set keeps playing after the keys are released
    Bass bass = Bass::Off;
    BassRate bass_rate = BassRate::Quarter;
    uint8_t bass_octave = 1;      // 0 C1, 1 C2, 2 C3 (the root's octave)
    uint16_t bpm = kDefaultBpm;   // internal tempo, 40-300
    uint16_t seed = 0;            // 0-2047: Random's sequence
    bool clock_out = true;        // send MIDI clock while running on the internal tempo
    bool operator==(const Settings& o) const {
        return pattern == o.pattern && rate == o.rate && octaves == o.octaves && gate == o.gate && latch == o.latch
            && bass == o.bass && bass_rate == o.bass_rate && bass_octave == o.bass_octave && bpm == o.bpm
            && seed == o.seed && clock_out == o.clock_out;
    }
};
// Word 1: bits 0-2 pattern, 3-5 rate, 6-7 octaves-1, 8-12 gate-1, 13 latch, 14-16 bass,
// 17-18 bass rate, 19-20 bass octave. Word 2: bits 0-8 bpm, 9-19 seed, 20 clock out.
inline uint32_t PackArp(const Settings& s) {
    return static_cast<uint32_t>(s.pattern) | static_cast<uint32_t>(s.rate) << 3 | ((s.octaves - 1u) & 3u) << 6
         | ((s.gate - 1u) & 31u) << 8 | (s.latch ? 1u << 13 : 0u) | static_cast<uint32_t>(s.bass) << 14
         | static_cast<uint32_t>(s.bass_rate) << 17 | (s.bass_octave & 3u) << 19;
}
inline uint32_t PackClock(const Settings& s) { return (s.bpm & 511u) | (s.seed & 2047u) << 9 | (s.clock_out ? 1u << 20 : 0u); }
// The words a patch may carry (Parameters::Valid uses this).
inline bool ValidWords(uint32_t arp, uint32_t clock) { return PartsWordsValid(arp, clock); }   // parameters.h
inline Settings Unpack(uint32_t arp, uint32_t clock) {
    Settings s;
    s.pattern = static_cast<Pattern>(arp & 7u); s.rate = static_cast<Rate>(arp >> 3 & 7u);
    s.octaves = static_cast<uint8_t>((arp >> 6 & 3u) + 1); s.gate = static_cast<uint8_t>((arp >> 8 & 31u) + 1);
    s.latch = arp >> 13 & 1u; s.bass = static_cast<Bass>(arp >> 14 & 7u); s.bass_rate = static_cast<BassRate>(arp >> 17 & 3u);
    s.bass_octave = static_cast<uint8_t>(arp >> 19 & 3u);
    s.bpm = static_cast<uint16_t>(clock & 511u); s.seed = static_cast<uint16_t>(clock >> 9 & 2047u); s.clock_out = clock >> 20 & 1u;
    if(s.bpm < kMinBpm || s.bpm > kMaxBpm) s.bpm = kDefaultBpm;
    return s;
}

// Tempo and position in 24 PPQN ticks. Internal: the tempo in BPM. External: MIDI clock
// ticks as they arrive (start / continue / stop obeyed); without a tick for half a second
// the internal tempo takes over again (from the beat it reached).
class Clock {
public:
    enum Message : uint8_t { Tick, Start, Continue, Stop };
    void Init(float rate) { rate_ = rate; }
    void SetTempo(uint16_t bpm) { bpm_ = bpm < kMinBpm ? kMinBpm : bpm > kMaxBpm ? kMaxBpm : bpm; }
    uint16_t Tempo() const { return bpm_; }
    // The tempo in use: MIDI's (measured) while following, else the internal one.
    float Bpm() const { return external_ ? 60.f * rate_ / (kPpqn * interval_) : static_cast<float>(bpm_); }
    float SamplesPerTick() const { return external_ ? interval_ : 60.f * rate_ / (kPpqn * static_cast<float>(bpm_)); }
    bool External() const { return external_; }
    bool Running() const { return running_; }
    uint32_t Ticks() const { return ticks_; }
    // Internal run / stop (the parts page); MIDI start / stop when following.
    void Run(bool on) { if(on && !running_) { ticks_ = 0; phase_ = 0.f; external_ = false; } running_ = on; }
    // Tap tempo: the average of the last taps (each 0.2-1.5 s apart) once there are two.
    FORGE_COLD bool Tap(uint32_t now_samples) {
        const uint32_t gap = now_samples - last_tap_;
        last_tap_ = now_samples;
        if(gap < 0.2f * rate_ || gap > 1.5f * rate_) { taps_ = 0; return false; }
        tap_sum_ = taps_ ? tap_sum_ + static_cast<float>(gap) : static_cast<float>(gap);
        if(taps_ < 4) ++taps_; else tap_sum_ -= tap_sum_ / 5.f;
        const float bpm = 60.f * rate_ * static_cast<float>(taps_) / tap_sum_;
        SetTempo(static_cast<uint16_t>(bpm + 0.5f));
        return true;
    }
    FORGE_COLD void Midi(Message m) {
        if(m == Tick) { if(pending_ < 64) ++pending_; return; }
        if(!external_ || interval_ <= 0.f) interval_ = 60.f * rate_ / (kPpqn * static_cast<float>(bpm_));   // until MIDI's is measured
        if(m == Start) { ticks_ = 0; phase_ = 0.f; pending_ = 0; running_ = true; external_ = true; since_ = 0; }
        else if(m == Continue) { running_ = true; external_ = true; since_ = 0; }
        else { running_ = false; external_ = true; since_ = 0; }
    }
    // Advance one audio block; returns the ticks crossed (at most 8: a stalled caller
    // never bursts notes), counting `ticks_` up.
    FORGE_COLD unsigned Advance(unsigned frames) {
        unsigned crossed = 0;
        if(pending_) {                                // MIDI clock (arrives between blocks)
            if(!external_) { interval_ = SamplesPerTick(); external_ = true; running_ = true; }
            else if(since_ > 0) {                     // smooth the measured tick length
                const float gap = static_cast<float>(since_) / static_cast<float>(pending_);
                interval_ += 0.2f * (gap - interval_);
            }
            crossed = running_ ? pending_ : 0;
            pending_ = 0; since_ = 0;
        }
        if(external_) {
            since_ += frames;
            // MIDI clock gone while running: CHOMPI's own tempo again. After a MIDI stop it
            // stays stopped until start / continue (or B4), whatever the host sends meanwhile.
            if(running_ && since_ > static_cast<uint32_t>(0.5f * rate_)) {
                external_ = false; running_ = true; phase_ = 0.f;
            }
        } else if(running_) {
            const float tick = SamplesPerTick();      // phase_ in samples (exact for whole numbers)
            phase_ += static_cast<float>(frames);
            while(phase_ >= tick && crossed < 8) { phase_ -= tick; ++crossed; }
            if(phase_ >= tick) phase_ = 0.f;
        }
        if(crossed > 8) crossed = 8;
        ticks_ += crossed;
        return crossed;
    }
private:
    float rate_ = 48000.f, phase_ = 0.f, interval_ = 0.f, tap_sum_ = 0.f;
    uint32_t ticks_ = 0, since_ = 0, last_tap_ = 0, pending_ = 0;
    uint16_t bpm_ = kDefaultBpm;
    uint8_t taps_ = 0;
    bool external_ = false, running_ = true;
};

// What the parts module wants sounded: the synth (with the key's source) and MIDI out.
struct Event { uint8_t note = 0, velocity = 0, source = 0; };

class Parts {
public:
    Settings settings;
    void Init(float rate) { clock_.Init(rate); clock_.SetTempo(settings.bpm); }
    Clock& GetClock() { return clock_; }
    const Clock& GetClock() const { return clock_; }
    // MIDI clock in: Start also starts the phrase over on the downbeat.
    FORGE_COLD void ClockMessage(Clock::Message m) {
        clock_.Midi(m);
        if(m == Clock::Start) { step_ = 0; bass_step_ = 0; random_ = Seed(); restart_ = set_count_ != 0; gather_ = 0; }
        if(m == Clock::Stop) { StopArp(); StopBass(); }
    }
    // Tap tempo (the parts page): true once the tempo changed.
    FORGE_COLD bool Tap() { if(!clock_.Tap(now_)) return false; settings.bpm = clock_.Tempo(); return true; }
    bool ArpOn() const { return settings.pattern != Pattern::Off; }
    bool BassOn() const { return settings.bass != Bass::Off; }
    bool Active() const { return ArpOn() || BassOn(); }
    // A new chord or key set: `root` its root (harmony), or -1 for the lowest note; `fifth`
    // the chord's fifth in semitones (7 without harmony).
    // `key`: the key (or MIDI note) pressed; held keys are counted once per source, so a
    // repeated note-on without its note-off never keeps a phrase open, and one source's
    // note-off never releases the same key held on another (panel C4 vs loop / MIDI C4).
    static constexpr unsigned kSources = 4;          // UART, USB, keybed, event recorder (synth.h)
    FORGE_COLD void KeyDown(uint8_t key, const uint8_t* notes, unsigned count, int root, uint8_t fifth, uint8_t velocity, uint8_t source) {
        if(!count || key > 127 || source >= kSources) return;
        if(!held_) {                                  // first key after a release: a new phrase
            ClearSet(); step_ = 0; random_ = Seed(); restart_ = true; bass_step_ = 0;
            gather_ = kGatherFrames;                  // let the rest of a chord's keys arrive first
        }
        const uint8_t bit = static_cast<uint8_t>(1u << source);
        if(!(keys_[key] & bit)) { keys_[key] |= bit; ++held_; }
        for(unsigned i = 0; i < count; ++i) Add(notes[i], bit);
        root_ = root >= 0 ? static_cast<uint8_t>(root % 12) : Lowest() % 12;
        fifth_ = fifth; velocity_ = velocity ? velocity : velocity_; source_ = source;
        bass_change_ = true;
        BuildOrder();
    }
    // A key-up of a key this source does not hold (another source's, or one pressed before
    // the parts were on) changes nothing.
    FORGE_COLD void KeyUp(uint8_t key, const uint8_t* notes, unsigned count, uint8_t source) {
        if(key > 127 || source >= kSources) return;
        const uint8_t bit = static_cast<uint8_t>(1u << source);
        if(!(keys_[key] & bit)) return;
        keys_[key] &= static_cast<uint8_t>(~bit); --held_;
        if(settings.latch && ArpOn()) return;        // the set keeps cycling until the next phrase
        for(unsigned i = 0; i < count; ++i) Remove(notes[i], bit);
        BuildOrder();
        if(!set_count_) { ReleaseKeys(); bass_change_ = true; }
    }
    // Patch / panel changes: stop what no longer applies.
    FORGE_COLD void Changed() {
        clock_.SetTempo(settings.bpm);
        if(!ArpOn()) { StopArp(); if(!held_) ClearSet(); }
        if(!BassOn()) StopBass();
        if(!Active()) { ClearSet(); ReleaseKeys(); }
        BuildOrder();
    }
    // Everything off (panic, patch silence): the notes it owns stop on MIDI too.
    FORGE_COLD void Clear() {
        StopArp(); StopBass();
        ClearSet(); order_count_ = 0; ReleaseKeys();
        for(auto& r : refs_) r = 0;
    }
    // One audio block: the clock, then steps and gate ends. The synth events it (or any
    // other call) made wait for Take().
    FORGE_COLD void Advance(unsigned frames) {
        now_ += frames;
        const unsigned ticks = clock_.Advance(frames);
        last_ticks_ = ticks;
        // MIDI clock out: start, 24 ticks per beat, stop, while the parts play on the internal tempo.
        const bool sending = Active() && settings.clock_out && !clock_.External() && clock_.Running();
        if(sending != sending_) { Midi(sending ? 0xfa : 0xfc); sending_ = sending; }
        const float tick = clock_.SamplesPerTick();
        if(arp_note_ && (arp_left_ -= static_cast<float>(frames)) <= 0.f) StopArp();
        if(bass_count_ && bass_left_ > 0.f && (bass_left_ -= static_cast<float>(frames)) <= 0.f) StopBass();
        if(!Active()) { restart_ = false; return; }
        if(!clock_.Running()) { StopArp(); return; }
        // A new phrase starts kGatherFrames after its first key (a chord's keys land a few ms apart:
        // DC's CHOMPI played the first steps of a half-built chord, 2026-10-11); then on the grid.
        if(restart_) {
            if(gather_ > frames) gather_ -= frames;
            else { gather_ = 0; restart_ = false; if(ArpOn() && order_count_) ArpStep(tick); if(BassOn()) BassStep(tick); }
        }
        for(unsigned t = 0; t < ticks; ++t) {
            const uint32_t at = clock_.Ticks() - ticks + t + 1;
            if(sending) Midi(0xf8);
            if(restart_) continue;                    // still gathering the chord: no grid steps yet
            if(ArpOn() && order_count_ && at % kRateTicks[static_cast<unsigned>(settings.rate)] == 0) ArpStep(tick);
            const uint8_t bt = kBassTicks[static_cast<unsigned>(settings.bass_rate)];
            if(BassOn() && bt && at % bt == 0) BassStep(tick);
        }
        if(!restart_ && BassOn() && settings.bass_rate == BassRate::Chord && bass_change_) BassStep(tick);
        if(BassOn() && !set_count_ && bass_count_) StopBass();
    }
    // Synth note events (velocity 0 = off) since the last call, oldest first.
    FORGE_COLD unsigned Take(Event* out, unsigned max) {
        const unsigned n = events_ < max ? events_ : max;
        for(unsigned i = 0; i < n; ++i) out[i] = event_[i];
        events_ = 0;
        return n;
    }
    static constexpr unsigned kEvents = 16;
    bool PopMidi(MidiOut& m) { return midi_.Pop(m); }
    // Ticks the last Advance crossed (the event recorder steps with them).
    unsigned LastTicks() const { return last_ticks_; }
    // A note for MIDI out on the keys' channel (event recorder playback).
    void SendNote(uint8_t note, uint8_t velocity) { Midi(velocity ? 0x90 : 0x80, note, velocity); }
    // Inspector: the set as played and what sounds.
    unsigned SetCount() const { return set_count_; }
    const uint8_t* Set() const { return set_; }
    uint8_t ArpNote() const { return arp_note_; }
    // The synth voice (note, source) is the parts' own: a key-up of that pair must not stop it.
    bool Owns(uint8_t note, uint8_t source) const { return note < 128 && refs_[note] != 0 && owner_[note] == source; }
    uint8_t BassNote() const { return bass_count_ ? bass_[0] : 0; }
    bool Latched() const { return set_count_ && !held_; }
    uint32_t Drops() const { return drops_; }
private:
    // The set keeps, per note, the sources that hold it (`bit` = 1 << source); a note leaves
    // only when its last source lets go. Within one source harmony counts shared chord notes.
    void Add(uint8_t note, uint8_t bit) {
        if(note > 127) return;
        for(unsigned i = 0; i < set_count_; ++i) if(set_[i] == note) { set_sources_[note] |= bit; return; }
        if(set_count_ < kMaxSet) { set_[set_count_++] = note; set_sources_[note] = bit; }
    }
    void Remove(uint8_t note, uint8_t bit) {
        if(note > 127 || !(set_sources_[note] & bit)) return;
        if((set_sources_[note] &= static_cast<uint8_t>(~bit))) return;   // another source still holds it
        for(unsigned i = 0; i < set_count_; ++i) if(set_[i] == note) {
            for(unsigned j = i + 1; j < set_count_; ++j) set_[j - 1] = set_[j];
            --set_count_; return;
        }
    }
    void ClearSet() { for(unsigned i = 0; i < set_count_; ++i) set_sources_[set_[i]] = 0; set_count_ = 0; }
    uint8_t Lowest() const { uint8_t n = 127; for(unsigned i = 0; i < set_count_; ++i) if(set_[i] < n) n = set_[i]; return n; }
    uint32_t Seed() const { return 0x12345678u ^ (static_cast<uint32_t>(settings.seed) * 2654435761u); }
    // The cycle: played order or sorted, over the octaves (Up/Down/UpDown/Random sort).
    FORGE_COLD void BuildOrder() {
        uint8_t base[kMaxSet]; unsigned n = set_count_;
        for(unsigned i = 0; i < n; ++i) base[i] = set_[i];
        if(settings.pattern != Pattern::Order)
            for(unsigned i = 1; i < n; ++i) for(unsigned j = i; j > 0 && base[j - 1] > base[j]; --j) { const uint8_t t = base[j]; base[j] = base[j - 1]; base[j - 1] = t; }
        order_count_ = 0;
        const unsigned octaves = settings.octaves < 1 ? 1 : settings.octaves > 4 ? 4 : settings.octaves;
        for(unsigned o = 0; o < octaves; ++o) for(unsigned i = 0; i < n; ++i) {
            const unsigned note = base[i] + 12u * o;
            if(note <= 127 && order_count_ < sizeof(order_)) order_[order_count_++] = static_cast<uint8_t>(note);
        }
    }
    FORGE_COLD unsigned NextIndex() {
        const unsigned n = order_count_;
        unsigned i = 0;
        switch(settings.pattern) {
            case Pattern::Up: case Pattern::Order: i = step_ % n; break;
            case Pattern::Down: i = n - 1 - step_ % n; break;
            case Pattern::UpDown: { const unsigned cycle = n > 1 ? 2 * n - 2 : 1, k = step_ % cycle; i = k < n ? k : cycle - k; break; }
            case Pattern::Random:
                random_ ^= random_ << 13; random_ ^= random_ >> 17; random_ ^= random_ << 5;
                i = random_ % n; break;
            case Pattern::Off: break;
        }
        ++step_;
        return i;
    }
    FORGE_COLD void ArpStep(float tick) {
        StopArp();
        const uint8_t note = order_[NextIndex()];
        Start(note, 0);
        arp_note_ = note;
        arp_left_ = tick * kRateTicks[static_cast<unsigned>(settings.rate)] * (settings.gate < 1 ? 1 : settings.gate > 20 ? 20 : settings.gate) / 20.f;
    }
    FORGE_COLD void BassStep(float tick) {
        StopBass();
        bass_change_ = false;
        if(!set_count_) return;
        const uint8_t base = static_cast<uint8_t>(12 * (settings.bass_octave + 2) + root_);   // C1 / C2 / C3 octave
        const bool odd = bass_step_++ & 1u;
        bass_count_ = 1; bass_[0] = base;
        switch(settings.bass) {
            case Bass::Root: case Bass::Off: break;
            case Bass::Fifth: bass_[1] = static_cast<uint8_t>(base + fifth_); bass_count_ = 2; break;
            case Bass::Alternate: if(odd) bass_[0] = static_cast<uint8_t>(base + fifth_); break;
            case Bass::Octave: if(odd) bass_[0] = static_cast<uint8_t>(base + 12); break;
        }
        for(unsigned i = 0; i < bass_count_; ++i) Start(bass_[i], 1);
        const uint8_t bt = kBassTicks[static_cast<unsigned>(settings.bass_rate)];
        bass_left_ = bt ? tick * bt * 0.9f : 0.f;    // Chord: held until the chord changes
    }
    void StopArp() { if(arp_note_) { Stop(arp_note_, 0); arp_note_ = 0; } }
    void StopBass() { for(unsigned i = 0; i < bass_count_; ++i) Stop(bass_[i], 1); bass_count_ = 0; bass_left_ = 0.f; }
    void Start(uint8_t note, uint8_t part) {
        if(!note) return;
        Midi(static_cast<uint8_t>(0x90 | part), note, velocity_);
        if(refs_[note]++ == 0) { owner_[note] = source_; Emit(note, velocity_, source_); }
    }
    void Stop(uint8_t note, uint8_t part) {
        if(!note) return;
        Midi(static_cast<uint8_t>(0x80 | part), note, 0);
        if(refs_[note] && --refs_[note] == 0) Emit(note, 0, owner_[note]);
    }
    void Emit(uint8_t note, uint8_t velocity, uint8_t source) {
        if(events_ < kEvents) event_[events_++] = Event{note, velocity, source};
        else ++drops_;
    }
    void Midi(uint8_t status, uint8_t a = 0, uint8_t b = 0) { if(!midi_.Push(MidiOut{status, a, b})) ++drops_; }

    Clock clock_;
    SpscQueue<MidiOut, 64> midi_;
    uint8_t set_[kMaxSet] = {}, order_[kMaxSet * 4] = {}, refs_[128] = {}, owner_[128] = {}, bass_[2] = {};
    void ReleaseKeys() { for(auto& k : keys_) k = 0; held_ = 0; }
    uint8_t keys_[128] = {}, set_sources_[128] = {};   // per key / set note: a bit per source holding it
    unsigned set_count_ = 0, order_count_ = 0, step_ = 0, held_ = 0, bass_step_ = 0, bass_count_ = 0, last_ticks_ = 0;
    uint32_t random_ = 0x12345678u, drops_ = 0, now_ = 0;
    float arp_left_ = 0.f, bass_left_ = 0.f;
    uint8_t arp_note_ = 0, root_ = 0, fifth_ = 7, velocity_ = 100, source_ = 2;
    bool restart_ = false, bass_change_ = false, sending_ = false;
    static constexpr unsigned kGatherFrames = 720;   // 15 ms at 48 kHz
    unsigned gather_ = 0;
    Event event_[kEvents] = {}; unsigned events_ = 0;
};
} // namespace parts
} // namespace forge
