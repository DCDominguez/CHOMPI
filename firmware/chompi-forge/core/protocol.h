#pragma once
#include <cstddef>
#include <cstdint>
#include "parameters.h"

namespace forge {
constexpr uint8_t kProtocolVersion = 1, kPatchVersion = 1;
constexpr uint8_t kFirmwareMinor = 3;
enum class Error : uint8_t { None, Length, Version, Checksum, Patch, Opcode, Busy };
enum class RequestKind : uint8_t { Parameter, Patch, Status, Note, Panic };
struct Request {
    RequestKind kind = RequestKind::Status;
    uint8_t note = 0, velocity = 0;
    Command command{Parameter::Mix, 0.f};
    Parameters patch{};
    uint16_t sequence = 0;
    uint8_t source = 0;
};
struct Response {
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
inline bool IsRequest(const uint8_t* bytes, size_t size) {
    return size >= 7 && bytes[0] == 0x7d && bytes[1] == 'F'
        && bytes[2] == 'G' && bytes[4] < 0x40;
}
inline Error DecodeRequest(const uint8_t* bytes, size_t size, Request& out) {
    if(!IsRequest(bytes, size) || size < 8) return Error::Length;
    for(size_t i = 0; i < size; ++i) if(bytes[i] > 127) return Error::Patch;
    if(bytes[3] != kProtocolVersion) return Error::Version;
    if(Checksum(bytes, size) != 0) return Error::Checksum;
    Request candidate;
    candidate.sequence = Read14(bytes + 5);
    if(bytes[4] == 1) {
        if(bytes[7] != 1 && bytes[7] != 2) return Error::Version;
        if(size != (bytes[7] == 1 ? 18u : 30u)) return Error::Length;
        candidate.patch.version = bytes[7];
        if(bytes[16] > 1) return Error::Patch;
        candidate.kind = RequestKind::Patch;
        candidate.patch.mix = Read14(bytes + 8) / 16383.f;
        candidate.patch.time = Read14(bytes + 10) / 16383.f;
        candidate.patch.feedback = Read14(bytes + 12) / 16383.f;
        candidate.patch.level = Read14(bytes + 14) / 16383.f;
        candidate.patch.bypass = bytes[16] != 0;
        if(bytes[7] == 2) {
            if(bytes[17] > 1 || bytes[18] > 3) return Error::Patch;
            candidate.patch.synth = bytes[17] != 0; candidate.patch.waveform = bytes[18];
            candidate.patch.attack = Read14(bytes + 19) / 16383.f;
            candidate.patch.decay = Read14(bytes + 21) / 16383.f;
            candidate.patch.sustain = Read14(bytes + 23) / 16383.f;
            candidate.patch.release = Read14(bytes + 25) / 16383.f;
            candidate.patch.cutoff = Read14(bytes + 27) / 16383.f;
        }
    } else if(bytes[4] == 2) {
        if(size != 8) return Error::Length;
        candidate.kind = RequestKind::Status;
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
// All sizes here exclude MIDI's F0/F7 envelope. Caller supplies >= 42 bytes.
inline size_t EncodeResponse(const Response& response, uint32_t dropped,
                             uint32_t rejected, uint8_t* bytes) {
    if(response.error != Error::None) return EncodeError(response.sequence, response.error, bytes);
    Header(bytes, 0x40, response.sequence);
    bytes[7] = 0; bytes[8] = response.patch.version;
    WriteNormalized(bytes + 9, response.patch.mix);
    WriteNormalized(bytes + 11, response.patch.time);
    WriteNormalized(bytes + 13, response.patch.feedback);
    WriteNormalized(bytes + 15, response.patch.level);
    bytes[17] = response.patch.bypass ? 1 : 0;
    const auto cpu = [](float value) -> unsigned {
        return std::isfinite(value) ? static_cast<unsigned>(Clamp(value * 1000.f, 0.f, 16383.f)) : 0;
    };
    size_t offset = 18;
    if(response.patch.version == 2) {
        bytes[18] = response.patch.synth ? 1 : 0; bytes[19] = response.patch.waveform;
        WriteNormalized(bytes + 20, response.patch.attack);
        WriteNormalized(bytes + 22, response.patch.decay);
        WriteNormalized(bytes + 24, response.patch.sustain);
        WriteNormalized(bytes + 26, response.patch.release);
        WriteNormalized(bytes + 28, response.patch.cutoff);
        offset = 30;
    }
    Write14(bytes + offset, cpu(response.cpu_average));
    Write14(bytes + offset + 2, cpu(response.cpu_max));
    Write21(bytes + offset + 4, dropped); Write21(bytes + offset + 7, rejected);
    bytes[offset + 10] = kFirmwareMinor;
    bytes[offset + 11] = Checksum(bytes, offset + 11);
    return offset + 12;
}
} // namespace forge
