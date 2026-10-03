#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "../core/wav.h"

using namespace forge;

namespace {
// Build a WAV file: optional extra chunks before "fmt " and between fmt and data.
std::vector<uint8_t> MakeWav(uint16_t tag, uint16_t channels, uint16_t bits, uint32_t rate,
                             const std::vector<uint8_t>& data, bool extensible = false, uint32_t junk_before = 0,
                             uint32_t list_between = 0) {
    std::vector<uint8_t> f;
    auto put = [&f](const char* s) { f.insert(f.end(), s, s + 4); };
    auto u32 = [&f](uint32_t v) { for(int i = 0; i < 4; ++i) f.push_back((v >> (8 * i)) & 255); };
    auto u16 = [&f](uint16_t v) { f.push_back(v & 255); f.push_back(v >> 8); };
    put("RIFF"); u32(0); put("WAVE");
    if(junk_before) { put("JUNK"); u32(junk_before); f.insert(f.end(), junk_before + (junk_before & 1), 0); }
    put("fmt "); u32(extensible ? 40 : 16);
    u16(extensible ? 0xfffe : tag); u16(channels); u32(rate);
    u32(rate * channels * bits / 8); u16(channels * bits / 8); u16(bits);
    if(extensible) { u16(22); u16(bits); u32(3); u16(tag); f.insert(f.end(), 14, 0); }
    if(list_between) { put("LIST"); u32(list_between); f.insert(f.end(), list_between + (list_between & 1), 'x'); }
    put("data"); u32(static_cast<uint32_t>(data.size())); f.insert(f.end(), data.begin(), data.end());
    const uint32_t riff = static_cast<uint32_t>(f.size() - 8);
    for(int i = 0; i < 4; ++i) f[4 + i] = (riff >> (8 * i)) & 255;
    return f;
}
WavError Parse(const std::vector<uint8_t>& f, WavInfo& info, size_t head = 4096) {
    return ParseWav(f.data(), f.size() < head ? f.size() : head, static_cast<uint32_t>(f.size()), info);
}

void WavFormats() {
    // 16-bit stereo 48 kHz: TAPE's own format, data at 44.
    std::vector<uint8_t> pcm16 = {0x00, 0x80, 0xff, 0x7f, 0x01, 0x00, 0xff, 0xff};
    WavInfo info;
    auto f = MakeWav(1, 2, 16, 48000, pcm16);
    assert(Parse(f, info) == WavError::None && info.data_offset == 44 && info.Frames() == 2 && info.channels == 2);
    int16_t out[8];
    ConvertFrames(info, f.data() + info.data_offset, 2, out);
    assert(out[0] == -32768 && out[1] == 32767 && out[2] == 1 && out[3] == -1);
    // 24-bit mono 44.1 kHz with JUNK and LIST chunks (odd size padding): rounds to 16 bits.
    std::vector<uint8_t> pcm24 = {0x80, 0x00, 0x00, 0xff, 0xff, 0x7f, 0x00, 0x00, 0x80, 0x7f, 0x00, 0x00};
    f = MakeWav(1, 1, 24, 44100, pcm24, false, 27, 5);
    assert(Parse(f, info) == WavError::None && info.channels == 1 && info.rate == 44100 && info.Frames() == 4);
    ConvertFrames(info, f.data() + info.data_offset, 4, out);
    assert(out[0] == 1 && out[1] == 32767 && out[2] == -32768 && out[3] == 0);  // 0x000080 rounds up; 0x7fffff clamps; 127 rounds to 0
    // 32-bit float stereo via WAVE_FORMAT_EXTENSIBLE; clamps and NaN -> 0.
    const float floats[] = {0.5f, -1.5f, NAN, 1.f};
    std::vector<uint8_t> fl(16); std::memcpy(fl.data(), floats, 16);
    f = MakeWav(3, 2, 32, 96000, fl, true);
    assert(Parse(f, info) == WavError::None && info.is_float && info.Frames() == 2);
    ConvertFrames(info, f.data() + info.data_offset, 2, out);
    assert(out[0] == 16384 && out[1] == -32768 && out[2] == 0 && out[3] == 32767);
    // 8-bit unsigned.
    f = MakeWav(1, 1, 8, 22050, {0, 128, 255});
    assert(Parse(f, info) == WavError::None && info.Frames() == 3);
    ConvertFrames(info, f.data() + info.data_offset, 3, out);
    assert(out[0] == -32768 && out[1] == 0 && out[2] == 32512);
}

void WavRejections() {
    WavInfo info;
    assert(Parse(MakeWav(2, 2, 16, 48000, {0, 0, 0, 0}), info) == WavError::Unsupported);   // ADPCM
    assert(Parse(MakeWav(1, 3, 16, 48000, {0, 0, 0, 0, 0, 0}), info) == WavError::Unsupported); // 3 channels
    assert(Parse(MakeWav(1, 2, 16, 4000, {0, 0, 0, 0}), info) == WavError::Unsupported);   // rate
    assert(Parse(MakeWav(1, 2, 32, 48000, std::vector<uint8_t>(8)), info) == WavError::Unsupported); // 32-bit int
    assert(Parse(MakeWav(3, 2, 16, 48000, {0, 0, 0, 0}), info) == WavError::Unsupported);  // 16-bit float
    auto f = MakeWav(1, 2, 16, 48000, {0, 0, 0, 0});
    f[8] = 'X'; assert(Parse(f, info) == WavError::NotWav);
    assert(ParseWav(f.data(), 8, 8, info) == WavError::NotWav);
    // A huge chunk before fmt pushes the next header beyond the head buffer.
    f = MakeWav(1, 2, 16, 48000, {0, 0, 0, 0}, false, 5000);
    assert(Parse(f, info, 4096) == WavError::HeaderTooLarge && Parse(f, info, 8192) == WavError::None);
    // No data chunk.
    f = MakeWav(1, 2, 16, 48000, {}); f.resize(f.size() - 8);
    assert(Parse(f, info) == WavError::NotWav);
    // Truncated file: data size beyond the file is cut to whole frames.
    f = MakeWav(1, 2, 16, 48000, std::vector<uint8_t>(400)); f.resize(f.size() - 202);
    assert(Parse(f, info) == WavError::None && info.Frames() == 49 && info.data_size == 196);
}

void HeaderRoundTrip() {
    uint8_t header[kWavHeaderSize];
    WriteWavHeader(12345, header);
    std::vector<uint8_t> f(header, header + kWavHeaderSize);
    f.resize(kWavHeaderSize + 12345 * 4);
    WavInfo info;
    assert(Parse(f, info) == WavError::None && info.data_offset == 44 && info.Frames() == 12345
           && info.rate == 48000 && info.channels == 2 && info.bytes_per_sample == 2 && !info.is_float);
    assert(ReadLe32(header + 4) == f.size() - 8);
}

// The factory TAPE card (when present in the repo): every file parses as
// TAPE's 16-bit stereo 48 kHz format with data at byte 44.
void FactoryTapeFiles() {
    const char* names[][2] = {{"cubbi_a1.wav", "576000"}, {"jammi_a11.wav", "385023"}, {"cubbi_a1_double.wav", "288000"}};
    unsigned checked = 0;
    for(auto& n : names) {
        const std::string path = std::string("../card-profiles/tape-2.0/") + n[0];
        FILE* file = std::fopen(path.c_str(), "rb");
        if(!file) continue;
        std::fseek(file, 0, SEEK_END); const long size = std::ftell(file); std::fseek(file, 0, SEEK_SET);
        uint8_t head[4096]; const size_t got = std::fread(head, 1, sizeof head, file); std::fclose(file);
        WavInfo info;
        assert(ParseWav(head, got, static_cast<uint32_t>(size), info) == WavError::None);
        assert(info.data_offset == 44 && info.channels == 2 && info.rate == 48000 && info.bytes_per_sample == 2);
        assert(info.Frames() == static_cast<uint32_t>(std::stoul(n[1])));
        ++checked;
    }
    std::cout << "  factory TAPE files checked: " << checked << "\n";
}
} // namespace

int main() {
    WavFormats(); WavRejections(); HeaderRoundTrip(); FactoryTapeFiles();
    std::cout << "PASS: WAV formats/rejections/header, factory TAPE files\n";
}
