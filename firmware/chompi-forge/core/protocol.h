#pragma once
#include <cstddef>
#include <cstdint>
#include "parameters.h"

namespace forge {
constexpr uint8_t kProtocolVersion = 1, kPatchVersion = 1;
constexpr uint8_t kFirmwareMinor = 2;
enum class Error : uint8_t { None, Length, Version, Checksum, Patch, Opcode, Busy };
enum class RequestKind : uint8_t { Parameter, Patch, Status };
struct Request {
    RequestKind kind = RequestKind::Status;
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
        if(size != 18) return Error::Length;
        if(bytes[7] != kPatchVersion) return Error::Version;
        if(bytes[16] > 1) return Error::Patch;
        candidate.kind = RequestKind::Patch;
        candidate.patch.mix = Read14(bytes + 8) / 16383.f;
        candidate.patch.time = Read14(bytes + 10) / 16383.f;
        candidate.patch.feedback = Read14(bytes + 12) / 16383.f;
        candidate.patch.level = Read14(bytes + 14) / 16383.f;
        candidate.patch.bypass = bytes[16] != 0;
    } else if(bytes[4] == 2) {
        if(size != 8) return Error::Length;
        candidate.kind = RequestKind::Status;
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
// All sizes here exclude MIDI's F0/F7 envelope. Caller supplies >= 30 bytes.
inline size_t EncodeResponse(const Response& response, uint32_t dropped,
                             uint32_t rejected, uint8_t* bytes) {
    if(response.error != Error::None) return EncodeError(response.sequence, response.error, bytes);
    Header(bytes, 0x40, response.sequence);
    bytes[7] = 0; bytes[8] = kPatchVersion;
    WriteNormalized(bytes + 9, response.patch.mix);
    WriteNormalized(bytes + 11, response.patch.time);
    WriteNormalized(bytes + 13, response.patch.feedback);
    WriteNormalized(bytes + 15, response.patch.level);
    bytes[17] = response.patch.bypass ? 1 : 0;
    const auto cpu = [](float value) -> unsigned {
        return std::isfinite(value) ? static_cast<unsigned>(Clamp(value * 1000.f, 0.f, 16383.f)) : 0;
    };
    Write14(bytes + 18, cpu(response.cpu_average));
    Write14(bytes + 20, cpu(response.cpu_max));
    Write21(bytes + 22, dropped); Write21(bytes + 25, rejected);
    bytes[28] = kFirmwareMinor;
    bytes[29] = Checksum(bytes, 29);
    return 30;
}
} // namespace forge
