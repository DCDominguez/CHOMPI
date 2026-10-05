#pragma once
#include <cstddef>
#include <cstdint>
#include "parameters.h"

namespace forge {
constexpr uint8_t kProtocolVersion = 1, kPatchVersion = 1;
constexpr uint8_t kFirmwareMinor = 10; // 0.10: TAPE parity (knobs, effects, count-in, restart record); 0.9: key lights while playing (TAPE); 0.8: power as stock (off gesture, SW6 battery, charger hand-over); 0.7: USB file transfer (opcode 0C)
// 7-9 are device-preset (SD) errors: empty slot, no/failed card, storage busy.
enum class Error : uint8_t { None, Length, Version, Checksum, Patch, Opcode, Busy, Empty, Storage, StorageBusy };
// Device presets: 8 banks x 15 slots on the SD card (see preset_store.h).
constexpr uint8_t kPresetBanks = 8, kPresetSlots = 15;
// Request/reply sizes exclude F0/F7. v5 is the largest: 88-byte apply request,
// 100-byte status reply (v4: 84 / 96). Transport buffers are sized from these constants.
constexpr size_t kV3Request = 69, kV4Request = 84, kMaxRequest = 88, kMaxReply = 100;
// Note, Pedal (CC64), Bend, ModWheel (CC1) and ResetControllers (CC121) are
// channel-1 performance events: no reply, dropped if queued before an emergency.
// Store/Recall/Erase/List are host requests for device presets; the main loop
// performs SD I/O (Store first takes a parameter snapshot from the audio owner).
// SampleList/SampleJob (opcodes 08/09) are sampler requests: the main loop
// answers lists and runs erase/copy; a save first locks the recording (audio).
// Panel/Probe (opcodes 0A/0B) exist only in development builds (FORGE_TEST_HOOKS).
enum class RequestKind : uint8_t { Parameter, Patch, Status, Note, Panic, Pedal, Bend, ResetControllers, ModWheel,
                                   Store, Recall, Erase, List, SampleList, SampleJob, Panel, Probe,
                                   Looper };   // internal (MIDI CC 24/26/27): note = control, value = CC value
enum class SampleAction : uint8_t { Save, Erase, Copy };   // opcode 09 byte 7
struct Request {
    RequestKind kind = RequestKind::Status;
    uint8_t note = 0, velocity = 0;
    uint16_t value = 0; // Bend: 14-bit, 8192 = centre. Pedal: 0 up, 1 down. ModWheel: 0..127.
    Command command{Parameter::Mix, 0.f};
    Parameters patch{};
    uint16_t sequence = 0;
    uint8_t source = 0;
    uint8_t epoch = 0; // main-loop emergency count when queued; never on the wire
    uint8_t bank = 0, slot = 0; // device preset address (Store/Recall/Erase), 0-based
    bool silent = false;        // apply without a reply (on-device recall)
    bool reset_cpu = false;     // Status: start a new CPU peak after reporting (per-step measurements)
    // SampleJob: action, source mode/bank/slot (bank/slot above) and copy destination.
    SampleAction action = SampleAction::Save;
    uint8_t mode = 0, to_mode = 0, to_bank = 0, to_slot = 0;
    // Panel (development): event kind, id, signed value; Probe: page.
    uint8_t panel_kind = 0, panel_id = 0, page = 0;
    int8_t panel_value = 0;
#ifdef FORGE_TEST_HOOKS
    uint32_t inspector_cursor = 0;
#endif
};
// Status: current patch + diagnostics (op 0x40). Stored/Erased: 0x42 storage ack.
// Occupancy: 0x43 bank bitmaps. Snapshot: internal only (audio -> main for Store).
// SampleOccupancy: 0x44 sample slots + recording. SampleDone: 0x45 sample job ack.
// SampleSnapshot: internal (audio locked the recording for a host save).
enum class ResponseKind : uint8_t { Status, Stored, Erased, Occupancy, Snapshot, SampleOccupancy, SampleDone, SampleSnapshot };
constexpr uint8_t kSampleCardReady = 1, kSampleRecording = 2, kSampleBusy = 4;   // 0x44 flags
struct Response {
    ResponseKind kind = ResponseKind::Status;
    uint8_t bank = 0, slot = 0;
    uint16_t occupancy[kPresetBanks]{};
    // Sampler replies.
    uint16_t samples[2][kSampleBanks]{};
    uint8_t flags = 0, mode = 0;
    SampleAction action = SampleAction::Save;
    uint32_t record_ms = 0, capacity_ms = 0, frames = 0;
    float gain = 1.f;
    Error error = Error::None;
    Parameters patch{};
    uint16_t sequence = 0;
    uint8_t source = 0;
    float cpu_average = 0.f, cpu_max = 0.f;
};
inline uint16_t Read14(const uint8_t* bytes) { return bytes[0] | (uint16_t(bytes[1]) << 7); }
inline void Write14(uint8_t* bytes, unsigned value) {
    bytes[0] = value & 127; bytes[1] = (value >> 7) & 127;
}
inline uint8_t Checksum(const uint8_t* bytes, size_t size) {
    unsigned sum = 0;
    for(size_t i = 0; i < size; ++i) sum += bytes[i];
    return (128 - (sum & 127)) & 127;
}
// v3 fields after the v2 layout (request index; reply index is +1).
// Kind: 0 = 14-bit normalized float, 1 = byte with an inclusive maximum.
struct V3Field { uint8_t index, kind, max; float Parameters::* unit; uint8_t Parameters::* byte; };
inline const V3Field* V3Fields(size_t& count) {
    static const V3Field fields[] = {
        {29, 1, 3, nullptr, &Parameters::osc2_waveform}, {30, 0, 0, &Parameters::osc2_level, nullptr},
        {32, 1, 48, nullptr, &Parameters::osc2_semitones}, {33, 0, 0, &Parameters::osc2_detune, nullptr},
        {35, 0, 0, &Parameters::noise, nullptr}, {37, 0, 0, &Parameters::resonance, nullptr},
        {39, 0, 0, &Parameters::filter_amount, nullptr}, {41, 0, 0, &Parameters::filter_attack, nullptr},
        {43, 0, 0, &Parameters::filter_decay, nullptr}, {45, 0, 0, &Parameters::filter_sustain, nullptr},
        {47, 0, 0, &Parameters::filter_release, nullptr}, {49, 1, 3, nullptr, &Parameters::lfo_waveform},
        {50, 0, 0, &Parameters::lfo_rate, nullptr}, {52, 0, 0, &Parameters::lfo_pitch, nullptr},
        {54, 0, 0, &Parameters::lfo_filter, nullptr}, {56, 0, 0, &Parameters::lfo_amp, nullptr},
        // 58: lfo_wheel (bool), 59: voices (1..4): handled explicitly
        {60, 0, 0, &Parameters::glide, nullptr}, {62, 0, 0, &Parameters::reverb_mix, nullptr},
        {64, 0, 0, &Parameters::reverb_size, nullptr}, {66, 0, 0, &Parameters::reverb_damping, nullptr},
        // v4 sampler (version 4+). 78-80: loop, gate, reverse (bools): handled explicitly.
        {68, 1, 1, nullptr, &Parameters::source}, {69, 1, 1, nullptr, &Parameters::sample_mode},
        {70, 1, kSampleBanks - 1, nullptr, &Parameters::sample_bank}, {71, 1, kSampleSlots - 1, nullptr, &Parameters::sample_slot},
        {72, 0, 0, &Parameters::sample_pitch, nullptr}, {74, 0, 0, &Parameters::sample_start, nullptr},
        {76, 0, 0, &Parameters::sample_end, nullptr}, {81, 0, 0, &Parameters::sample_xfade, nullptr},
    };
    count = sizeof(fields) / sizeof(fields[0]);
    return fields;
}
// Patch DATA: the version byte and fields after the 7-byte header of an apply
// request (request index i is DATA index i - 7; replies carry it from index 8).
// Shared by SysEx requests, status replies and SD preset records.
inline size_t PatchDataSize(uint8_t version) {
    return version == 1 ? 10 : version == 2 ? 22 : version == 3 ? kV3Request - 8 : version == 4 ? kV4Request - 8
         : version == 5 ? kMaxRequest - 8 : 0;
}
FORGE_NOINLINE inline Error DecodePatchData(const uint8_t* data, size_t size, Parameters& out) {
    if(!size || data[0] < 1 || data[0] > 5) return Error::Version;
    if(size != PatchDataSize(data[0])) return Error::Length;
    for(size_t i = 0; i < size; ++i) if(data[i] > 127) return Error::Patch;
    auto at = [data](size_t request_index) { return data + request_index - 7; };
    Parameters p;
    p.version = data[0];
    if(*at(16) > 1) return Error::Patch;
    p.mix = Read14(at(8)) / 16383.f;
    p.time = Read14(at(10)) / 16383.f;
    p.feedback = Read14(at(12)) / 16383.f;
    p.level = Read14(at(14)) / 16383.f;
    p.bypass = *at(16) != 0;
    if(p.version >= 2) {
        if(*at(17) > 1 || *at(18) > 3) return Error::Patch;
        p.synth = *at(17) != 0; p.waveform = *at(18);
        p.attack = Read14(at(19)) / 16383.f;
        p.decay = Read14(at(21)) / 16383.f;
        p.sustain = Read14(at(23)) / 16383.f;
        p.release = Read14(at(25)) / 16383.f;
        p.cutoff = Read14(at(27)) / 16383.f;
    }
    if(p.version >= 3) {
        size_t count; const V3Field* fields = V3Fields(count);
        for(size_t i = 0; i < count; ++i) {
            const V3Field& f = fields[i];
            if(f.index >= kV3Request - 1 && p.version < 4) continue;
            if(f.kind == 0) p.*f.unit = Read14(at(f.index)) / 16383.f;
            else if(*at(f.index) > f.max) return Error::Patch;
            else p.*f.byte = *at(f.index);
        }
        if(*at(58) > 1 || *at(59) < 1 || *at(59) > p.MaxVoices()) return Error::Patch;
        p.lfo_wheel = *at(58) != 0; p.voices = *at(59);
    }
    if(p.version >= 4) {
        if(*at(78) > 1 || *at(79) > 1 || *at(80) > 1) return Error::Patch;
        p.sample_loop = *at(78) != 0; p.sample_gate = *at(79) != 0; p.sample_reverse = *at(80) != 0;
    }
    if(p.version >= 5) for(unsigned k = 0; k < 4; ++k) p.knobs[k] = *at(83 + k);   // request 83-86; checked by Valid()
    if(!p.Valid()) return Error::Patch;      // e.g. sample start not before end
    out = p;
    return Error::None;
}
inline bool IsRequest(const uint8_t* bytes, size_t size) {
    return size >= 7 && bytes[0] == 0x7d && bytes[1] == 'F'
        && bytes[2] == 'G' && bytes[4] < 0x40;
}
FORGE_NOINLINE inline Error DecodeRequest(const uint8_t* bytes, size_t size, Request& out) {
    if(!IsRequest(bytes, size) || size < 8) return Error::Length;
    for(size_t i = 0; i < size; ++i) if(bytes[i] > 127) return Error::Patch;
    if(bytes[3] != kProtocolVersion) return Error::Version;
    if(Checksum(bytes, size) != 0) return Error::Checksum;
    Request candidate;
    candidate.sequence = Read14(bytes + 5);
    if(bytes[4] == 1) {
        if(bytes[7] < 1 || bytes[7] > 5) return Error::Version;
        if(size != 8 + PatchDataSize(bytes[7])) return Error::Length;
        const Error error = DecodePatchData(bytes + 7, size - 8, candidate.patch);
        if(error != Error::None) return error;
        candidate.kind = RequestKind::Patch;
    } else if(bytes[4] >= 4 && bytes[4] <= 6) {      // store / recall / erase (bank, slot)
        if(size != 10) return Error::Length;
        if(bytes[7] >= kPresetBanks || bytes[8] >= kPresetSlots) return Error::Patch;
        candidate.kind = bytes[4] == 4 ? RequestKind::Store : bytes[4] == 5 ? RequestKind::Recall : RequestKind::Erase;
        candidate.bank = bytes[7]; candidate.slot = bytes[8];
    } else if(bytes[4] == 7) {                       // list occupied slots
        if(size != 8) return Error::Length;
        candidate.kind = RequestKind::List;
    } else if(bytes[4] == 8) {                       // list samples
        if(size != 8) return Error::Length;
        candidate.kind = RequestKind::SampleList;
    } else if(bytes[4] == 9) {                       // sample job: action, mode, bank, slot, to mode/bank/slot
        if(size != 15) return Error::Length;
        if(bytes[7] > 2 || bytes[8] > 1 || bytes[9] >= kSampleBanks || bytes[10] >= kRamSlot
           || bytes[11] > 1 || bytes[12] >= kSampleBanks || bytes[13] >= kRamSlot) return Error::Patch;
        candidate.kind = RequestKind::SampleJob; candidate.action = static_cast<SampleAction>(bytes[7]);
        candidate.mode = bytes[8]; candidate.bank = bytes[9]; candidate.slot = bytes[10];
        candidate.to_mode = bytes[11]; candidate.to_bank = bytes[12]; candidate.to_slot = bytes[13];
#ifdef FORGE_TEST_HOOKS
    } else if(bytes[4] == 0x0a) {                    // panel event: kind, id, value (+64 for turns/overrides)
        if(size != 11) return Error::Length;
        const uint8_t kind = bytes[7], id = bytes[8];
        const int value = int(bytes[9]) - 64;
        const bool ok = (kind == 0 && id < 40 && (value == 0 || value == 1))
                     || (kind == 1 && id < 6 && value >= -63 && value <= 63)
                     || (kind == 2 && id == 4 && value == 0)
                     || ((kind == 3 || kind == 4) && id == 0 && value >= -1 && value <= 1)
                     || (kind == 5 && id == 0 && value == 0);
        if(!ok) return Error::Patch;
        candidate.kind = RequestKind::Panel; candidate.panel_kind = kind; candidate.panel_id = id;
        candidate.panel_value = static_cast<int8_t>(value);
    } else if(bytes[4] == 0x0b) {                    // probe: page 1 LEDs, 2-7 versioned Inspector pages
        if(size != 9 && !(size == 14 && bytes[7] == 6)) return Error::Length;
        if(bytes[7] == 0 || bytes[7] > 7) return Error::Patch;   // page 0 (old state page) retired: Inspector covers it
        if(size == 14) {
            if(bytes[12] > 15) return Error::Patch;
            for(unsigned i=0;i<5;++i) candidate.inspector_cursor |= uint32_t(bytes[8+i]) << (7*i);
        }
        candidate.kind = RequestKind::Probe; candidate.page = bytes[7];
#endif
    } else if(bytes[4] == 2) {                       // status; optional flags byte: bit 0 resets the CPU peak after this reply
        if(size != 8 && size != 9) return Error::Length;
        if(size == 9 && bytes[7] > 1) return Error::Patch;
        candidate.kind = RequestKind::Status;
        candidate.reset_cpu = size == 9 && bytes[7] == 1;
    } else if(bytes[4] == 3) {
        if(size != 8) return Error::Length;
        candidate.kind = RequestKind::Panic;
    } else return Error::Opcode;
    out = candidate;
    return Error::None;
}
inline void Header(uint8_t* bytes, uint8_t opcode, uint16_t sequence) {
    bytes[0] = 0x7d; bytes[1] = 'F'; bytes[2] = 'G';
    bytes[3] = kProtocolVersion; bytes[4] = opcode;
    Write14(bytes + 5, sequence);
}
inline void WriteNormalized(uint8_t* bytes, float value) {
    Write14(bytes, static_cast<unsigned>(Clamp(value, 0.f, 1.f) * 16383.f + 0.5f));
}
inline void Write21(uint8_t* bytes, uint32_t value) {
    if(value > 0x1fffff) value = 0x1fffff;
    Write14(bytes, value); bytes[2] = (value >> 14) & 127;
}
inline size_t EncodeError(uint16_t sequence, Error error, uint8_t* bytes) {
    Header(bytes, 0x41, sequence);
    bytes[7] = static_cast<uint8_t>(error); bytes[8] = Checksum(bytes, 8);
    return 9;
}
// All sizes here exclude MIDI's F0/F7 envelope. Caller supplies >= kMaxReply bytes.
// Writes patch DATA (see DecodePatchData); returns its size.
FORGE_NOINLINE inline size_t EncodePatchData(const Parameters& p, uint8_t* data) {
    auto at = [data](size_t request_index) { return data + request_index - 7; };
    data[0] = p.version;
    WriteNormalized(at(8), p.mix);
    WriteNormalized(at(10), p.time);
    WriteNormalized(at(12), p.feedback);
    WriteNormalized(at(14), p.level);
    *at(16) = p.bypass ? 1 : 0;
    if(p.version >= 2) {
        *at(17) = p.synth ? 1 : 0; *at(18) = p.waveform;
        WriteNormalized(at(19), p.attack);
        WriteNormalized(at(21), p.decay);
        WriteNormalized(at(23), p.sustain);
        WriteNormalized(at(25), p.release);
        WriteNormalized(at(27), p.cutoff);
    }
    if(p.version >= 3) {
        size_t count; const V3Field* fields = V3Fields(count);
        for(size_t i = 0; i < count; ++i) {
            const V3Field& f = fields[i];
            if(f.index >= kV3Request - 1 && p.version < 4) continue;
            if(f.kind == 0) WriteNormalized(at(f.index), p.*f.unit);
            else *at(f.index) = p.*f.byte;
        }
        *at(58) = p.lfo_wheel ? 1 : 0; *at(59) = p.voices;
    }
    if(p.version >= 4) {
        *at(78) = p.sample_loop ? 1 : 0; *at(79) = p.sample_gate ? 1 : 0; *at(80) = p.sample_reverse ? 1 : 0;
    }
    if(p.version >= 5) for(unsigned k = 0; k < 4; ++k) *at(83 + k) = p.knobs[k];
    return PatchDataSize(p.version);
}
FORGE_NOINLINE inline size_t EncodeResponse(const Response& response, uint32_t dropped,
                             uint32_t rejected, uint8_t* bytes) {
    if(response.error != Error::None) return EncodeError(response.sequence, response.error, bytes);
    if(response.kind == ResponseKind::Stored || response.kind == ResponseKind::Erased) {
        Header(bytes, 0x42, response.sequence);
        bytes[7] = 0; bytes[8] = response.kind == ResponseKind::Stored ? 1 : 2;
        bytes[9] = response.bank; bytes[10] = response.slot;
        bytes[11] = Checksum(bytes, 11);
        return 12;
    }
    if(response.kind == ResponseKind::SampleOccupancy) {
        Header(bytes, 0x44, response.sequence);
        bytes[7] = 0;
        for(unsigned m = 0; m < 2; ++m) for(unsigned b = 0; b < kSampleBanks; ++b)
            Write14(bytes + 8 + 2 * (m * kSampleBanks + b), response.samples[m][b] & 0x3fff);
        bytes[28] = response.flags & 7u;
        Write21(bytes + 29, response.record_ms); Write21(bytes + 32, response.capacity_ms);
        bytes[35] = Checksum(bytes, 35);
        return 36;
    }
    if(response.kind == ResponseKind::SampleDone) {
        Header(bytes, 0x45, response.sequence);
        bytes[7] = 0; bytes[8] = static_cast<uint8_t>(response.action);
        bytes[9] = response.mode; bytes[10] = response.bank; bytes[11] = response.slot;
        bytes[12] = Checksum(bytes, 12);
        return 13;
    }
    if(response.kind == ResponseKind::Occupancy) {
        Header(bytes, 0x43, response.sequence);
        bytes[7] = 0;
        for(unsigned b = 0; b < kPresetBanks; ++b) {
            const uint16_t bits = response.occupancy[b];
            bytes[8 + 3 * b] = bits & 127; bytes[9 + 3 * b] = (bits >> 7) & 127; bytes[10 + 3 * b] = (bits >> 14) & 1;
        }
        bytes[32] = Checksum(bytes, 32);
        return 33;
    }
    Header(bytes, 0x40, response.sequence);
    bytes[7] = 0;
    const size_t offset = 8 + EncodePatchData(response.patch, bytes + 8);
    const auto cpu = [](float value) -> unsigned {
        return std::isfinite(value) ? static_cast<unsigned>(Clamp(value * 1000.f, 0.f, 16383.f)) : 0;
    };
    Write14(bytes + offset, cpu(response.cpu_average));
    Write14(bytes + offset + 2, cpu(response.cpu_max));
    Write21(bytes + offset + 4, dropped); Write21(bytes + offset + 7, rejected);
    bytes[offset + 10] = kFirmwareMinor;
    bytes[offset + 11] = Checksum(bytes, offset + 11);
    return offset + 12;
}
#ifdef FORGE_TEST_HOOKS
// Development probe replies (main loop). 0x47: panel event queued. 0x46 page 1:
// the 25 key LEDs + CHOMPI LED as 7-bit RGB (pages 2-7: core/inspector.h).
inline size_t EncodePanelAck(uint16_t sequence, uint8_t* bytes) {
    Header(bytes, 0x47, sequence); bytes[7] = 0; bytes[8] = Checksum(bytes, 8);
    return 9;
}
inline size_t EncodeProbeLeds(uint16_t sequence, const uint8_t (&leds)[26][3], uint8_t* bytes) {
    Header(bytes, 0x46, sequence); bytes[7] = 0; bytes[8] = 1;
    for(unsigned i = 0; i < 26; ++i) for(unsigned c = 0; c < 3; ++c) bytes[9 + 3 * i + c] = leds[i][c] & 127;
    bytes[87] = Checksum(bytes, 87);
    return 88;
}
#endif
} // namespace forge

