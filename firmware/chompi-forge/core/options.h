#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "parameters.h"

// TAPE's options.json (OptionsManager.h), read at start-up so one card works for both
// firmwares. Forge only reads it (TAPE rewrites it at every boot). Format:
//   {"chompi": [{"name": "Record Latch", "value": false}, {"name": "Midi In Channel", "value": 1}, ...]}
// Unknown names are ignored; a missing or malformed value keeps the default.
namespace forge {
struct Options {
    bool record_latch = false;          // "Record Latch": press to start, press again to stop
    uint8_t midi_in = 0, midi_out = 0;  // "Midi In/Out Channel" 1-16 -> 0-15
    bool tape_slew = true;              // "Tape Slew On": looper scrub glides like tape
    uint8_t monitor = 0;                // "Monitor Position" 1-3 -> 0 headphones, 1 both, 2 send/return
    bool quantise_menu = true;          // "Pitch Quantize In Shift Menu": menu quantised, normal page free
    bool split_delay = false;           // "Split Delay": SW3 page 1 left = delay, right = reverb
};

namespace options {
// The value after `"name": "<name>"`: true/false or a number. Returns false if absent.
inline bool Find(const char* text, size_t size, const char* name, long& value) {
    const size_t n = std::strlen(name);
    for(size_t i = 0; i + n + 2 <= size; ++i) {
        if(text[i] != '"' || std::strncmp(text + i + 1, name, n) != 0 || text[i + 1 + n] != '"') continue;
        // The first "value" after the name, inside the same {...} entry.
        size_t k = i + n + 2;
        while(k + 7 <= size && std::strncmp(text + k, "\"value\"", 7) != 0) { if(text[k] == '}') return false; ++k; }
        if(k + 7 > size) return false;
        k += 7;
        while(k < size && (text[k] == ' ' || text[k] == '\t' || text[k] == ':' || text[k] == '\r' || text[k] == '\n')) ++k;
        if(k + 4 <= size && std::strncmp(text + k, "true", 4) == 0) { value = 1; return true; }
        if(k + 5 <= size && std::strncmp(text + k, "false", 5) == 0) { value = 0; return true; }
        long v = 0; bool digits = false;
        while(k < size && text[k] >= '0' && text[k] <= '9' && v < 100000) { v = v * 10 + (text[k] - '0'); ++k; digits = true; }
        if(!digits) return false;
        value = v;
        return true;
    }
    return false;
}
FORGE_COLD inline Options Parse(const char* text, size_t size) {
    Options o; long v;
    if(Find(text, size, "Record Latch", v)) o.record_latch = v != 0;
    if(Find(text, size, "Midi In Channel", v) && v >= 1 && v <= 16) o.midi_in = static_cast<uint8_t>(v - 1);
    if(Find(text, size, "Midi Out Channel", v) && v >= 1 && v <= 16) o.midi_out = static_cast<uint8_t>(v - 1);
    if(Find(text, size, "Tape Slew On", v)) o.tape_slew = v != 0;
    if(Find(text, size, "Monitor Position", v) && v >= 1 && v <= 3) o.monitor = static_cast<uint8_t>(v - 1);
    if(Find(text, size, "Pitch Quantize In Shift Menu", v)) o.quantise_menu = v != 0;
    if(Find(text, size, "Split Delay", v)) o.split_delay = v != 0;
    return o;
}
} // namespace options
} // namespace forge
