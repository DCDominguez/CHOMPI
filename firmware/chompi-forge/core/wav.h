#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace forge {
// WAV reading for the sampler (main loop, pure; no I/O). Accepts PCM 8/16/24-bit
// and 32-bit float, mono or stereo, 8-96 kHz, including WAVE_FORMAT_EXTENSIBLE.
// Samples are stored as int16 with the file's channel count (mono stays mono).
struct WavInfo {
    uint8_t channels = 0, bytes_per_sample = 0;
    bool is_float = false;
    uint32_t rate = 0, data_offset = 0, data_size = 0;
    uint32_t BlockAlign() const { return uint32_t(channels) * bytes_per_sample; }
    uint32_t Frames() const { return BlockAlign() ? data_size / BlockAlign() : 0; }
};
enum class WavError : uint8_t { None, NotWav, Unsupported, HeaderTooLarge };

inline uint32_t ReadLe32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24); }
inline uint16_t ReadLe16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline void WriteLe32(uint8_t* p, uint32_t v) { p[0] = v & 255; p[1] = (v >> 8) & 255; p[2] = (v >> 16) & 255; p[3] = v >> 24; }
inline void WriteLe16(uint8_t* p, uint16_t v) { p[0] = v & 255; p[1] = v >> 8; }

// `head` is the first `head_size` bytes of a file of `file_size` bytes. Chunks
// before "data" are skipped by size; each chunk header must lie inside `head`.
// A data chunk running past the end of the file (truncated recording) is cut
// to the whole frames that exist.
inline WavError ParseWav(const uint8_t* head, size_t head_size, uint32_t file_size, WavInfo& out) {
    if(head_size < 12 || std::memcmp(head, "RIFF", 4) || std::memcmp(head + 8, "WAVE", 4)) return WavError::NotWav;
    WavInfo info;
    bool have_format = false;
    size_t at = 12;
    while(true) {
        if(at + 8 > head_size) return at >= file_size ? WavError::NotWav : WavError::HeaderTooLarge;
        const uint8_t* chunk = head + at;
        const uint32_t size = ReadLe32(chunk + 4);
        if(!std::memcmp(chunk, "fmt ", 4)) {
            if(size < 16 || at + 8 + 16 > head_size) return WavError::NotWav;
            const uint8_t* f = chunk + 8;
            uint16_t tag = ReadLe16(f);
            if(tag == 0xfffe) {                       // WAVE_FORMAT_EXTENSIBLE: sub-format GUID's first word
                if(size < 40 || at + 8 + 40 > head_size) return WavError::NotWav;
                tag = ReadLe16(f + 24);
            }
            const uint16_t channels = ReadLe16(f + 2), align = ReadLe16(f + 12), bits = ReadLe16(f + 14);
            info.rate = ReadLe32(f + 4);
            info.channels = static_cast<uint8_t>(channels);
            info.bytes_per_sample = static_cast<uint8_t>(bits / 8);
            info.is_float = tag == 3;
            const bool pcm = tag == 1 && (bits == 8 || bits == 16 || bits == 24);
            const bool flt = tag == 3 && bits == 32;
            if(!(pcm || flt) || channels < 1 || channels > 2 || bits % 8 || align != channels * (bits / 8)
               || info.rate < 8000 || info.rate > 96000) return WavError::Unsupported;
            have_format = true;
        } else if(!std::memcmp(chunk, "data", 4)) {
            if(!have_format) return WavError::NotWav;
            info.data_offset = static_cast<uint32_t>(at + 8);
            const uint32_t available = file_size > info.data_offset ? file_size - info.data_offset : 0;
            const uint32_t bytes = size < available ? size : available;
            info.data_size = bytes - bytes % info.BlockAlign();
            out = info;
            return WavError::None;
        }
        const uint64_t next = uint64_t(at) + 8 + size + (size & 1);   // chunks are word-aligned
        if(next >= file_size) return WavError::NotWav;
        at = static_cast<size_t>(next);
    }
}

// Convert `frames` whole frames of raw data to int16, keeping the channel count.
inline void ConvertFrames(const WavInfo& info, const uint8_t* src, uint32_t frames, int16_t* dst) {
    const uint32_t count = frames * info.channels;
    for(uint32_t i = 0; i < count; ++i) {
        int32_t s;
        switch(info.bytes_per_sample) {
            case 1: s = (int32_t(src[i]) - 128) * 256; break;
            case 2: s = static_cast<int16_t>(ReadLe16(src + 2 * i)); break;
            case 3: {
                const uint8_t* p = src + 3 * i;
                const int32_t v = static_cast<int32_t>((uint32_t(p[0]) << 8) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 24)) >> 8;
                s = (v + 128) >> 8;                       // round to 16 bits
                break;
            }
            default: {
                const uint32_t bits = ReadLe32(src + 4 * i);
                float f; std::memcpy(&f, &bits, 4);
                if(!(f == f)) f = 0.f;                    // NaN
                s = static_cast<int32_t>(f * 32767.f + (f >= 0.f ? 0.5f : -0.5f));
                break;
            }
        }
        dst[i] = static_cast<int16_t>(s > 32767 ? 32767 : s < -32768 ? -32768 : s);
    }
}

// TAPE's file format: 44-byte header, 16-bit PCM, stereo, 48 kHz.
constexpr size_t kWavHeaderSize = 44;
inline void WriteWavHeader(uint32_t frames, uint8_t (&out)[kWavHeaderSize]) {
    const uint32_t data = frames * 4;
    std::memcpy(out, "RIFF", 4); WriteLe32(out + 4, 36 + data); std::memcpy(out + 8, "WAVEfmt ", 8);
    WriteLe32(out + 16, 16); WriteLe16(out + 20, 1); WriteLe16(out + 22, 2);
    WriteLe32(out + 24, 48000); WriteLe32(out + 28, 48000 * 4); WriteLe16(out + 32, 4); WriteLe16(out + 34, 16);
    std::memcpy(out + 36, "data", 4); WriteLe32(out + 40, data);
}
} // namespace forge
