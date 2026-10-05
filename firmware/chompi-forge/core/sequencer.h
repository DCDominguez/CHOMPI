#pragma once
#include <atomic>
#include <cstdint>
#include "parameters.h"

// Event recorder (firmware 0.15, DC's item 4): what is played (keys, MIDI notes, knob and
// parameter moves) recorded as events on the parts clock (core/parts.h, 24 PPQN), looped
// over whole bars (1-8) and overdubbed; not audio. It plays back through the engine like a
// player (its own note source), so harmony, the arp and the bass treat it like the keys.
// Saved with a device preset as FORGE/BbSss.FSQ (a "project" = preset + sequence).
// Audio owner only, except Mailbox (the save / load handoff with the main loop). Fixed
// arrays, no allocation; per block at most kPlayPerBlock events are played.
namespace forge {
namespace seq {

constexpr unsigned kMaxEvents = 1024;          // 6 KB
constexpr unsigned kTicksPerBar = 96;          // 4 beats x 24
constexpr unsigned kMaxBars = 8;
constexpr unsigned kMaxTicks = kTicksPerBar * kMaxBars;
constexpr uint8_t kSource = 3;                 // the engine's note source for playback
enum Kind : uint8_t { Note = 0, Param = 1 };   // Note: a = note, b = velocity (0 = off); Param: a = Parameter, b = value x 16383
struct Event { uint16_t tick = 0; uint8_t kind = 0, a = 0; uint16_t b = 0; };
// A recorded loop: `length` ticks (0 = empty), events sorted by tick (stable).
struct Sequence {
    uint16_t length = 0, count = 0;
    Event events[kMaxEvents];
};

// Which parameter moves are recorded: everything a knob or CC changes except the volume
// controls (output level, input gain), which stay with the player.
inline bool Recordable(Parameter p) { return p != Parameter::Level && p != Parameter::InputGain && p < Parameter::Count; }

// What a recorded event asks the engine to do (playback).
struct Action { Kind kind; uint8_t a; uint16_t b; };

class Sequencer {
public:
    enum class State : uint8_t { Empty, Armed, Recording, Playing, Stopped };
    State GetState() const { return state_; }
    bool Overdub() const { return overdub_; }
    bool Capturing() const { return state_ == State::Recording || (state_ == State::Playing && overdub_); }
    uint16_t Length() const { return sequence_.length; }
    uint16_t Position() const { return pos_; }
    unsigned Count() const { return sequence_.count; }
    uint32_t Drops() const { return drops_; }

    // The record key: empty -> armed (starts on the next bar); armed -> empty; recording ->
    // closes at the next bar line; playing -> stopped; stopped -> playing from the start.
    FORGE_COLD void RecordKey() {
        switch(state_) {
            case State::Empty: state_ = State::Armed; break;
            case State::Armed: state_ = State::Empty; break;
            case State::Recording: close_ = true; break;
            case State::Playing: Stop(); break;
            case State::Stopped: Rewind(); state_ = State::Playing; break;
        }
    }
    // Overdub on / off while playing (the notes still held when it ends get their ends).
    FORGE_COLD void OverdubKey() {
        if(state_ != State::Playing) return;
        if(overdub_) EndHeld(pos_);
        overdub_ = !overdub_;
    }
    FORGE_COLD void Clear() { Release(); sequence_.length = sequence_.count = 0; pos_ = 0; overdub_ = close_ = false; state_ = State::Empty; ClearHeld(); }
    // Panic: silent and stopped; the loop is kept (like the audio looper).
    FORGE_COLD void Panic() {
        Release();
        if(state_ == State::Recording) Close(pos_);
        if(state_ == State::Armed) state_ = State::Empty;
        if(state_ == State::Playing) { EndHeld(pos_); state_ = State::Stopped; }
        overdub_ = false;
    }
    // MIDI start: back to the loop's beginning.
    void Restart() { if(state_ == State::Playing) Rewind(); }

    // Live input while capturing (the engine calls these; playback is never re-recorded).
    FORGE_COLD void RecordNote(uint8_t note, uint8_t velocity) {
        if(!Capturing() || note > 127) return;
        if(velocity) { if(held_[note] < 255) ++held_[note]; }
        else if(held_[note]) --held_[note];
        else return;                                   // a release of something pressed before recording
        Insert(Event{pos_, Note, note, velocity});
    }
    FORGE_COLD void RecordParam(Parameter p, float value) {
        if(!Capturing() || !Recordable(p)) return;
        const uint16_t v = static_cast<uint16_t>(Clamp(value, 0.f, 1.f) * 16383.f + .5f);
        // One value per control per tick: a fast turn keeps its last position.
        for(int i = static_cast<int>(sequence_.count) - 1; i >= 0 && sequence_.events[i].tick == pos_; --i)
            if(sequence_.events[i].kind == Param && sequence_.events[i].a == static_cast<uint8_t>(p)) { sequence_.events[i].b = v; return; }
        Insert(Event{pos_, Param, static_cast<uint8_t>(p), v});
    }

    // One audio block: `ticks` clock ticks crossed, `clock` the parts clock's tick count
    // after them (bar lines are multiples of 96). Playback actions go to `out` (at most
    // `max`, oldest first); returns how many.
    FORGE_COLD unsigned Advance(unsigned ticks, uint32_t clock, bool running, Action* out, unsigned max) {
        unsigned n = 0;
        if(!running) return Flush(out, max);
        for(unsigned t = 0; t < ticks; ++t) {
            const uint32_t at = clock - ticks + t + 1;
            switch(state_) {
                case State::Armed:
                    if(at % kTicksPerBar == 0) { state_ = State::Recording; pos_ = 0; sequence_.count = 0; sequence_.length = 0; ClearHeld(); }
                    break;
                case State::Recording:
                    if(pos_ + 1u >= kMaxTicks || (close_ && (pos_ + 1u) % kTicksPerBar == 0)) Close(static_cast<uint16_t>(pos_ + 1u));
                    else ++pos_;
                    break;
                case State::Playing:
                    pos_ = static_cast<uint16_t>((pos_ + 1u) % sequence_.length);
                    n += Play(out + n, max - n);
                    break;
                default: break;
            }
        }
        return n + Flush(out + n, max - n);
    }
    // Notes the playback holds now (for panic / stop: the engine releases them).
    bool Sounding(uint8_t note) const { return note < 128 && playing_[note] != 0; }

    // Save / load (the mailbox copies; audio owner).
    void Export(Sequence& out) const { out.length = sequence_.length; out.count = sequence_.count;
        for(unsigned i = 0; i < sequence_.count; ++i) out.events[i] = sequence_.events[i]; }
    FORGE_COLD void Import(const Sequence& in) {
        Release(); ClearHeld();
        sequence_.length = in.length; sequence_.count = in.count;
        for(unsigned i = 0; i < in.count; ++i) sequence_.events[i] = in.events[i];
        overdub_ = close_ = false;
        state_ = sequence_.length ? State::Playing : State::Empty;
        Rewind();
    }
    const Sequence& Data() const { return sequence_; }

private:
    // Inserted after any event of the same tick (keeps the played order).
    void Insert(const Event& e) {
        if(sequence_.count >= kMaxEvents) { ++drops_; return; }
        unsigned i = sequence_.count;
        while(i > 0 && sequence_.events[i - 1].tick > e.tick) { sequence_.events[i] = sequence_.events[i - 1]; --i; }
        sequence_.events[i] = e; ++sequence_.count;
        if(state_ == State::Playing && i <= cursor_) ++cursor_;   // the playback cursor keeps its place
    }
    // Events at the current position.
    unsigned Play(Action* out, unsigned max) {
        if(pos_ == 0) cursor_ = 0;
        unsigned n = 0;
        while(cursor_ < sequence_.count && sequence_.events[cursor_].tick < pos_) ++cursor_;
        while(cursor_ < sequence_.count && sequence_.events[cursor_].tick == pos_) {
            const Event& e = sequence_.events[cursor_++];
            if(e.kind == Note) {
                if(e.b) { if(playing_[e.a] < 255) ++playing_[e.a]; }
                else if(playing_[e.a]) --playing_[e.a];
                else continue;                          // nothing of ours to stop
            }
            if(n < max) out[n++] = Action{static_cast<Kind>(e.kind), e.a, e.b}; else ++drops_;
        }
        return n;
    }
    // Pending note-offs (stop, panic, clear) go out with the next Advance.
    unsigned Flush(Action* out, unsigned max) {
        unsigned n = 0;
        for(unsigned note = 0; note < 128 && release_; ++note)
            while(pending_off_[note] && n < max) { --pending_off_[note]; out[n++] = Action{Note, static_cast<uint8_t>(note), 0}; }
        bool left = false; for(uint8_t c : pending_off_) left = left || c;
        release_ = left;
        return n;
    }
    void Release() {
        for(unsigned note = 0; note < 128; ++note) { pending_off_[note] = static_cast<uint8_t>(pending_off_[note] + playing_[note]); playing_[note] = 0; }
        release_ = true;
    }
    void Stop() { Release(); EndHeld(pos_); overdub_ = false; state_ = State::Stopped; }
    // The next tick plays the loop's first.
    void Rewind() { pos_ = sequence_.length ? static_cast<uint16_t>(sequence_.length - 1) : 0; cursor_ = sequence_.count; }
    // The first take ends on a bar line: notes still held get their ends just before it.
    void Close(uint16_t length) {
        length = static_cast<uint16_t>((length + kTicksPerBar - 1) / kTicksPerBar * kTicksPerBar);   // whole bars
        if(length < kTicksPerBar) length = kTicksPerBar;
        if(length > kMaxTicks) length = kMaxTicks;
        sequence_.length = length;
        EndHeld(static_cast<uint16_t>(length - 1));
        close_ = false;
        state_ = sequence_.count ? State::Playing : State::Empty;
        if(state_ == State::Empty) sequence_.length = 0;
        Rewind();
    }
    void EndHeld(uint16_t tick) {
        for(unsigned note = 0; note < 128; ++note)
            while(held_[note]) { --held_[note]; Insert(Event{tick, Note, static_cast<uint8_t>(note), 0}); }
    }
    void ClearHeld() { for(auto& h : held_) h = 0; }

    Sequence sequence_;
    uint8_t held_[128] = {}, playing_[128] = {}, pending_off_[128] = {};
    uint32_t drops_ = 0;
    unsigned cursor_ = 0;
    uint16_t pos_ = 0;
    State state_ = State::Empty;
    bool overdub_ = false, close_ = false, release_ = false;
};

// Save / load handoff between the audio owner and the main loop (SD card). One sequence
// at a time: the audio callback exports into it on a save and imports from it on a load.
struct Mailbox {
    enum : uint8_t { Free, Exporting, Exported, Importing, Imported, Main };
    std::atomic<uint8_t> state{Free};
    uint8_t bank = 0, slot = 0;
    Sequence sequence;
    // Audio: a save of bank/slot. False when the mailbox is busy (the save goes without it).
    bool Export(const Sequencer& s, uint8_t b, uint8_t sl) {
        uint8_t expected = Free;
        if(!state.compare_exchange_strong(expected, Exporting, std::memory_order_acquire)) return false;
        s.Export(sequence); bank = b; slot = sl;
        state.store(Exported, std::memory_order_release);
        return true;
    }
    // Audio: take a loaded sequence (after the main loop filled it).
    bool Import(Sequencer& s) {
        if(state.load(std::memory_order_acquire) != Imported) return false;
        s.Import(sequence);
        state.store(Free, std::memory_order_release);
        return true;
    }
    // Main loop: own the buffer to fill it (load) or to write it out (after Exported).
    bool Claim() { uint8_t expected = Free; return state.compare_exchange_strong(expected, Main, std::memory_order_acquire); }
    void Loaded() { state.store(Imported, std::memory_order_release); }
    void Done() { state.store(Free, std::memory_order_release); }
};

// File: "FSQ" 1, length (2), count (2), events (6 each: tick 2, kind, a, b 2; little-endian),
// CRC-16/CCITT over everything before it (2, big-endian). Bounded by kMaxEvents.
constexpr size_t kFileHeader = 8, kFileMax = kFileHeader + 6 * kMaxEvents + 2;
inline uint16_t FileCrc(const uint8_t* data, size_t size) {
    uint16_t crc = 0xffff;
    for(size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for(int bit = 0; bit < 8; ++bit) crc = crc & 0x8000 ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}
FORGE_COLD inline size_t EncodeFile(const Sequence& s, uint8_t* out) {
    out[0] = 'F'; out[1] = 'S'; out[2] = 'Q'; out[3] = 1;
    out[4] = s.length & 0xff; out[5] = s.length >> 8; out[6] = s.count & 0xff; out[7] = s.count >> 8;
    size_t n = kFileHeader;
    for(unsigned i = 0; i < s.count; ++i) {
        const Event& e = s.events[i];
        out[n++] = e.tick & 0xff; out[n++] = e.tick >> 8; out[n++] = e.kind; out[n++] = e.a; out[n++] = e.b & 0xff; out[n++] = e.b >> 8;
    }
    const uint16_t crc = FileCrc(out, n);
    out[n++] = static_cast<uint8_t>(crc >> 8); out[n++] = static_cast<uint8_t>(crc);
    return n;
}
// False for anything malformed (the slot then keeps no sequence).
FORGE_COLD inline bool DecodeFile(const uint8_t* data, size_t size, Sequence& out) {
    if(size < kFileHeader + 2 || data[0] != 'F' || data[1] != 'S' || data[2] != 'Q' || data[3] != 1) return false;
    const unsigned length = data[4] | data[5] << 8, count = data[6] | data[7] << 8;
    if(count > kMaxEvents || length > kMaxTicks || length % kTicksPerBar || (count && !length) || size != kFileHeader + 6 * count + 2) return false;
    const uint16_t crc = FileCrc(data, size - 2);
    if(data[size - 2] != (crc >> 8) || data[size - 1] != (crc & 0xff)) return false;
    uint16_t last = 0;
    for(unsigned i = 0; i < count; ++i) {
        const uint8_t* p = data + kFileHeader + 6 * i;
        Event e; e.tick = static_cast<uint16_t>(p[0] | p[1] << 8); e.kind = p[2]; e.a = p[3]; e.b = static_cast<uint16_t>(p[4] | p[5] << 8);
        const bool ok = e.tick < length && e.tick >= last
            && ((e.kind == Note && e.a < 128 && e.b < 128)
                || (e.kind == Param && e.a < static_cast<uint8_t>(Parameter::Count) && Recordable(static_cast<Parameter>(e.a)) && e.b <= 16383));
        if(!ok) return false;
        out.events[i] = e; last = e.tick;
    }
    out.length = static_cast<uint16_t>(length); out.count = static_cast<uint16_t>(count);
    return true;
}
inline void FilePath(uint8_t bank, uint8_t slot, char (&path)[20]) {
    const char text[] = "FORGE/B0S00.FSQ";
    for(size_t i = 0; i < sizeof(text); ++i) path[i] = text[i];
    path[7] = static_cast<char>('1' + bank);
    path[9] = static_cast<char>('0' + (slot + 1) / 10);
    path[10] = static_cast<char>('0' + (slot + 1) % 10);
}
} // namespace seq
} // namespace forge
