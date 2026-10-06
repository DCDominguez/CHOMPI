#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "parameters.h"

// TAPE's per-slot sample settings (TAPE PresetManager.h, presets.json), shared with
// TAPE on the same card (DC, 2026-10-05: share the file, keep a backup; save as TAPE
// does, the moment a knob turns; kit pads each keep their own; the slot's settings
// win over a Forge preset's). Per sample slot (chromatic/kit x banks a-e x slots
// 1-14): pitch, start, end, attack, decay (Forge: release), auto-loop, sustain
// (Forge: gate), gain, pan, as TAPE's knob positions 0-1. The recording has one more
// set in memory only (TAPE: "chompi" values, never written to the file).
//
// Audio owner: reads entries and saves knob turns. Main loop: parses the file before
// audio starts, copies/erases entries after sample jobs and writes the file. Each
// value is an atomic 16-bit word, so a reader sees every value whole; a set being
// rewritten while it is read can mix old and new values for one slot until the next
// write, which `Dirty` guarantees.
namespace forge {

struct SlotValues {
    // TAPE's defaults (ui.h enc_defaults / PresetManager defaults).
    float pitch = .83f, start = 0.f, end = 1.f, attack = 0.f, release = 0.f, gain = .704f, pan = .5f;
    bool loop = true, sustain = true;
};

class SlotSettings {
public:
    static constexpr unsigned kModes = 2, kBanks = kSampleBanks, kSlots = 14, kControls = 9;
    static constexpr size_t kFileMax = 8192;           // TAPE's buffer; a full file is about 7.5 KB

    SlotSettings() { Clear(); }
    FORGE_COLD void Clear() {
        for(auto& e : entries_) Store(e, SlotValues{}, false);
        Store(recording_, SlotValues{}, false);
        dirty_.store(false, std::memory_order_relaxed);
        changes_.fetch_add(1, std::memory_order_relaxed);
    }
    // slot 0-13 (TAPE 1-14) or kRamSlot (the recording).
    bool Valid(unsigned mode, unsigned bank, unsigned slot) const {
        const Entry* e = Find(mode, bank, slot);
        return e && e->valid.load(std::memory_order_relaxed);
    }
    FORGE_NOINLINE SlotValues Get(unsigned mode, unsigned bank, unsigned slot) const {
        const Entry* e = Find(mode, bank, slot);
        return e ? Load(*e) : SlotValues{};
    }
    FORGE_NOINLINE void Set(unsigned mode, unsigned bank, unsigned slot, const SlotValues& v) {
        Entry* e = Find(mode, bank, slot);
        if(!e) return;
        Store(*e, v, true);
        if(e != &recording_) Touch();
    }
    FORGE_COLD void Invalidate(unsigned mode, unsigned bank, unsigned slot) {
        Entry* e = Find(mode, bank, slot);
        if(!e || !e->valid.load(std::memory_order_relaxed)) return;
        e->valid.store(false, std::memory_order_relaxed);
        if(e != &recording_) Touch();
    }
    // TAPE's menu: Save copies the recording's settings, Copy copies a slot's, Erase invalidates.
    FORGE_COLD void Copy(unsigned from_mode, unsigned from_bank, unsigned from_slot, unsigned mode, unsigned bank, unsigned slot) {
        const Entry* from = Find(from_mode, from_bank, from_slot);
        Entry* to = Find(mode, bank, slot);
        if(!from || !to || from == to) return;
        Store(*to, Load(*from), from->valid.load(std::memory_order_relaxed));
        if(to != &recording_) Touch();
    }
    // Changed since the last TakeDirty (file entries only); the main loop writes the file.
    bool Dirty() const { return dirty_.load(std::memory_order_acquire); }
    bool TakeDirty() { return dirty_.exchange(false, std::memory_order_acq_rel); }
    // A write that failed: the changes stay pending (0.15.1).
    void Retry() { dirty_.store(true, std::memory_order_release); }
    uint32_t Changes() const { return changes_.load(std::memory_order_relaxed); }   // grows with every change

    // presets.json (TAPE PRE_VERSION 2: [mode][bank][slot] = [9 values x 1000, valid],
    // then the version; version 1 had 7 values). Unknown or malformed entries are
    // skipped (left at TAPE's defaults, not valid). Returns false if nothing parsed.
    FORGE_COLD bool Parse(const char* text, size_t size) {
        int depth = 0, index[5] = {};
        long items[10] = {}; unsigned count = 0; bool valid = false, any = false, overflow = false;
        for(size_t i = 0; i < size && text[i]; ++i) {
            const char c = text[i];
            if(c == '[') {
                if(++depth > 4) return any;
                index[depth] = 0;
                if(depth == 4) { count = 0; valid = false; overflow = false; }
            } else if(c == ']') {
                if(depth == 4 && !overflow && (count == 7 || count == 9) && index[1] < int(kModes) && index[2] < int(kBanks)
                   && index[3] < int(kSlots)) {
                    SlotValues v;
                    v.pitch = Unit(items[0]); v.start = Unit(items[1]); v.end = Unit(items[2]); v.attack = Unit(items[3]);
                    v.release = Unit(items[4]); v.loop = items[5] != 0; v.sustain = items[6] != 0;
                    if(count == 9) { v.gain = Unit(items[7]); v.pan = Unit(items[8]); }
                    Entry& e = entries_[Index(index[1], index[2], index[3])];
                    Store(e, v, valid);
                    changes_.fetch_add(1, std::memory_order_relaxed);
                    any = true;
                }
                if(depth <= 0) return any;
                --depth;
            } else if(c == ',') {
                if(depth >= 1) ++index[depth];
            } else if(depth == 4) {
                // A number (TAPE writes integers) or true/false; the boolean after the values is "valid".
                if(c == 't' || c == 'f') {
                    const bool t = c == 't';
                    if(index[4] == 7 || index[4] == 9) { valid = t; while(i + 1 < size && text[i + 1] >= 'a' && text[i + 1] <= 'z') ++i; }
                    else { overflow = true; }
                } else if(c == '-' || (c >= '0' && c <= '9')) {
                    bool negative = c == '-'; long v = negative ? 0 : c - '0';
                    while(i + 1 < size && ((text[i + 1] >= '0' && text[i + 1] <= '9') || text[i + 1] == '.')) {
                        ++i;
                        if(text[i] == '.') { while(i + 1 < size && text[i + 1] >= '0' && text[i + 1] <= '9') ++i; break; }
                        if(v < 100000) v = v * 10 + (text[i] - '0');
                    }
                    if(static_cast<unsigned>(index[4]) < 10) items[index[4]] = negative ? -v : v;
                    if(static_cast<unsigned>(index[4]) + 1 > count) count = static_cast<unsigned>(index[4]) + 1;
                    if(count > 9) overflow = true;
                }
            }
        }
        return any;
    }
    // TAPE's exact layout (PresetManager::WriteWholeFile). Returns the length, or 0 if
    // `capacity` is too small.
    FORGE_COLD size_t Write(char* out, size_t capacity) const {
        size_t n = 0;
        auto put = [&](const char* s) { while(*s) { if(n + 1 >= capacity) { n = capacity; return; } out[n++] = *s++; } };
        auto num = [&](unsigned v) { char b[8]; int i = 7; b[i] = 0; do { b[--i] = static_cast<char>('0' + v % 10); v /= 10; } while(v && i > 0); put(b + i); };
        put("[");
        for(unsigned mode = 0; mode < kModes; ++mode) {
            put("[");
            for(unsigned bank = 0; bank < kBanks; ++bank) {
                put("[");
                for(unsigned slot = 0; slot < kSlots; ++slot) {
                    const Entry& e = entries_[Index(mode, bank, slot)];
                    put("[");
                    for(unsigned c = 0; c < kControls; ++c) { num(e.value[c].load(std::memory_order_relaxed)); put(","); }
                    put(e.valid.load(std::memory_order_relaxed) ? "true" : "false");
                    put(slot + 1 < kSlots ? "]," : "]");
                }
                put(bank + 1 < kBanks ? "]," : "]");
            }
            put("],");
        }
        put("2]");
        if(n >= capacity) return 0;
        out[n] = 0;
        return n;
    }

private:
    struct Entry { std::atomic<uint16_t> value[kControls]; std::atomic<uint8_t> valid; };
    static unsigned Index(unsigned mode, unsigned bank, unsigned slot) { return (mode * kBanks + bank) * kSlots + slot; }
    Entry* Find(unsigned mode, unsigned bank, unsigned slot) {
        if(slot == kRamSlot) return &recording_;
        return mode < kModes && bank < kBanks && slot < kSlots ? &entries_[Index(mode, bank, slot)] : nullptr;
    }
    const Entry* Find(unsigned mode, unsigned bank, unsigned slot) const { return const_cast<SlotSettings*>(this)->Find(mode, bank, slot); }
    static float Unit(long thousandths) { return thousandths <= 0 ? 0.f : thousandths >= 1000 ? 1.f : static_cast<float>(thousandths) * .001f; }
    static uint16_t Word(float v) { return static_cast<uint16_t>(Clamp(v, 0.f, 1.f) * 1000.f + .5f); }
    FORGE_NOINLINE static void Store(Entry& e, const SlotValues& v, bool valid) {
        const uint16_t w[kControls] = {Word(v.pitch), Word(v.start), Word(v.end), Word(v.attack), Word(v.release),
                                       uint16_t(v.loop ? 1000 : 0), uint16_t(v.sustain ? 1000 : 0), Word(v.gain), Word(v.pan)};
        for(unsigned c = 0; c < kControls; ++c) e.value[c].store(w[c], std::memory_order_relaxed);
        e.valid.store(valid ? 1 : 0, std::memory_order_relaxed);
    }
    FORGE_NOINLINE static SlotValues Load(const Entry& e) {
        auto f = [&](unsigned c) { return static_cast<float>(e.value[c].load(std::memory_order_relaxed)) * .001f; };
        SlotValues v;
        v.pitch = f(0); v.start = f(1); v.end = f(2); v.attack = f(3); v.release = f(4);
        v.loop = e.value[5].load(std::memory_order_relaxed) != 0; v.sustain = e.value[6].load(std::memory_order_relaxed) != 0;
        v.gain = f(7); v.pan = f(8);
        return v;
    }
    void Touch() { changes_.fetch_add(1, std::memory_order_relaxed); dirty_.store(true, std::memory_order_release); }
    Entry entries_[kModes * kBanks * kSlots];
    Entry recording_;
    std::atomic<bool> dirty_{false};
    std::atomic<uint32_t> changes_{0};
};

// When the main loop writes presets.json: 2 s after the last change (turns come in
// bursts); after a failed write again later (4 s, doubling to 1 min) instead of never
// (0.15.1). Times in ms.
struct SlotSettingsSchedule {
    static constexpr uint32_t kSettle = 2000, kMaxWait = 60000;
    uint32_t seen = 0, changed_at = 0, wait = kSettle;
    bool Due(const SlotSettings& s, uint32_t now) {
        const uint32_t changes = s.Changes();
        if(changes != seen) { seen = changes; changed_at = now; wait = kSettle; return false; }
        return s.Dirty() && now - changed_at >= wait;
    }
    void Failed(SlotSettings& s, uint32_t now) {
        s.Retry(); changed_at = now;
        wait = wait >= kMaxWait / 2 ? kMaxWait : wait * 2;
    }
};
} // namespace forge
