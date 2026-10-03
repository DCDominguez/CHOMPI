#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include "../core/runtime.h"
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

// ---- v4 sampler voices ----
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<int16_t> memory[kSampleSlots];
    SampleTable table;
    Engine engine;
    Rig() { assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size())); engine.SetSamples(&table); }
    // Fill a slot from a function of (frame, channel) and publish it whole.
    template<class F> void Fill(uint8_t slot, uint32_t frames, uint8_t channels, F f, float ratio = 1.f) {
        memory[slot].assign(size_t(frames) * channels, 0);
        for(uint32_t i = 0; i < frames; ++i) for(uint8_t c = 0; c < channels; ++c)
            memory[slot][size_t(i) * channels + c] = static_cast<int16_t>(f(i, c));
        SampleSlot& s = table.slots[slot];
        s.data = memory[slot].data(); s.frames = frames; s.channels = channels; s.rate_ratio = ratio; s.gain = 1.f;
        s.loaded.store(frames, std::memory_order_release);
    }
    void Sine(uint8_t slot, uint32_t frames, float period, uint8_t channels = 1, float ratio = 1.f) {
        Fill(slot, frames, channels, [period](uint32_t i, uint8_t c) {
            return (c ? -20000.f : 20000.f) * std::sin(6.2831853f * i / period); }, ratio);
    }
    float Run(unsigned samples, float* last_left = nullptr, float* right_out = nullptr) {
        float peak = 0, L = 0, R = 0;
        for(unsigned i = 0; i < samples; ++i) { engine.Process(0, 0, L, R); peak = std::max(peak, std::fabs(L)); }
        if(last_left) *last_left = L;
        if(right_out) *right_out = R;
        return peak;
    }
    // Zero crossings (rising) of the left output over `samples` -> frequency.
    float Frequency(unsigned samples) {
        float L, R, prior = 0; unsigned crossings = 0;
        for(unsigned i = 0; i < samples; ++i) { engine.Process(0, 0, L, R); if(prior <= 0 && L > 0) ++crossings; prior = L; }
        return crossings * 48000.f / samples;
    }
};
Parameters SamplerPatch() {
    Parameters p; p.version = 4; p.synth = true; p.source = 1; p.level = 1.f; p.mix = 0.f;
    p.cutoff = 1.f; p.attack = 0.f; p.release = 0.f; p.sustain = 1.f; p.voices = 7;
    return p;
}

void ProtocolV4() {
    Parameters p = SamplerPatch();
    p.sample_mode = 1; p.sample_bank = 4; p.sample_slot = 14; p.sample_pitch = 0.75f; p.sample_start = 0.125f;
    p.sample_end = 0.875f; p.sample_xfade = 0.3f; p.sample_loop = true; p.sample_gate = false; p.sample_reverse = true;
    p.resonance = 0.4f; p.reverb_mix = 0.2f;
    uint8_t request[kMaxRequest]; Header(request, 1, 9);
    assert(EncodePatchData(p, request + 7) == kMaxRequest - 8);
    request[kMaxRequest - 1] = Checksum(request, kMaxRequest - 1);
    Request decoded; assert(DecodeRequest(request, kMaxRequest, decoded) == Error::None);
    const Parameters& q = decoded.patch;
    assert(q.version == 4 && q.source == 1 && q.sample_mode == 1 && q.sample_bank == 4 && q.sample_slot == 14
           && q.sample_loop && !q.sample_gate && q.sample_reverse && q.voices == 7
           && std::fabs(q.sample_pitch - 0.75f) < 1e-4f && std::fabs(q.sample_start - 0.125f) < 1e-4f
           && std::fabs(q.sample_end - 0.875f) < 1e-4f && std::fabs(q.sample_xfade - 0.3f) < 1e-4f);
    // Status echoes the same DATA bytes.
    Rig rig; Response response; assert(ExecuteRequest(decoded, rig.engine, response) && response.error == Error::None);
    uint8_t reply[kMaxReply]; assert(EncodeResponse(response, 0, 0, reply) == kMaxReply);
    for(size_t i = 7; i < kMaxRequest - 1; ++i) assert(reply[i + 1] == request[i]);
    // Byte limits, bools and start < end are enforced atomically.
    for(auto bad : std::vector<std::pair<unsigned, uint8_t>>{{68, 2}, {69, 2}, {70, 5}, {71, 15}, {78, 2}, {79, 2}, {80, 2}, {59, 8}, {59, 0}}) {
        uint8_t broken[kMaxRequest]; std::memcpy(broken, request, kMaxRequest);
        broken[bad.first] = bad.second; broken[kMaxRequest - 1] = Checksum(broken, kMaxRequest - 1);
        Request untouched; untouched.sequence = 3;
        assert(DecodeRequest(broken, kMaxRequest, untouched) == Error::Patch && untouched.sequence == 3);
    }
    uint8_t inverted[kMaxRequest]; std::memcpy(inverted, request, kMaxRequest);
    Write14(inverted + 74, 9000); Write14(inverted + 76, 9000); inverted[kMaxRequest - 1] = Checksum(inverted, kMaxRequest - 1);
    assert(DecodeRequest(inverted, kMaxRequest, decoded) == Error::Patch);
    // v3 keeps its 4-voice limit; v4 allows 7.
    Parameters v3; v3.version = 3; v3.synth = true; v3.voices = 5; assert(!v3.Valid());
    v3.version = 4; assert(v3.Valid());
}

void ChromaticPitch() {
    Rig rig; rig.Sine(0, 48000, 100.f);                     // 480 Hz at unity
    Parameters p = SamplerPatch(); p.sample_loop = true; assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0); rig.Run(480);
    assert(std::fabs(rig.Frequency(24000) - 480.f) < 6.f);
    rig.engine.Note(60, 0, 0); rig.engine.Note(72, 127, 0); rig.Run(480);   // octave up
    assert(std::fabs(rig.Frequency(24000) - 960.f) < 8.f);
    rig.engine.Apply({Parameter::Knob1, 0.25f}); rig.Run(4800);             // knob 1 = pitch, -12 st
    assert(std::fabs(rig.Frequency(24000) - 480.f) < 6.f);
    assert(std::fabs(rig.engine.GetParameters().sample_pitch - 0.25f) < 1e-6f && rig.engine.GetParameters().mix == 0.f);
    // A 24 kHz file plays at its own pitch.
    Rig half; half.Sine(0, 24000, 50.f, 1, 0.5f); assert(half.engine.ApplyPatch(p));
    half.engine.Note(60, 127, 0); half.Run(480);
    assert(std::fabs(half.Frequency(24000) - 480.f) < 6.f);
}

void KitMapping() {
    const int expected[25] = {0, -1, 1, -1, 2, 3, -1, 4, -1, 5, -1, 6, 7, -1, 8, -1, 9, 10, -1, 11, -1, 12, -1, 13, 14};
    for(int n = 0; n < 25; ++n) assert(Synth::KitSlot(static_cast<uint8_t>(48 + n)) == expected[n]);
    assert(Synth::KitSlot(47) == -1 && Synth::KitSlot(73) == -1);
    Rig rig;
    for(uint8_t s = 0; s < kSampleSlots; ++s) rig.Fill(s, 4800, 1, [s](uint32_t, uint8_t) { return 1000 * (s + 1); });
    Parameters p = SamplerPatch(); p.sample_mode = 1; assert(rig.engine.ApplyPatch(p));
    rig.Run(9600);                                                            // output level fade-in settles
    float left;
    rig.engine.Note(52, 127, 2); rig.Run(200, &left);                         // E3 -> slot 2 (3000)
    assert(std::fabs(left - 0.5f * 3000.f / 32768.f) < 1e-3f);
    rig.engine.Note(61, 127, 2); assert(rig.engine.ActiveVoices() == 1);      // black key: nothing
    rig.engine.Note(72, 127, 2); rig.Run(200, &left);                         // C5 -> slot 14 (recording)
    assert(std::fabs(left - 0.5f * (3000.f + 15000.f) / 32768.f) < 2e-3f && rig.engine.ActiveVoices() == 2);
    // An empty slot plays nothing.
    rig.table.slots[5].channels = 0; rig.engine.Note(57, 127, 2); assert(rig.engine.ActiveVoices() == 2);
}

void OneShotLoopReverse() {
    // One-shot: ends by itself, fading to zero (no jump at the end).
    Rig rig; rig.Fill(0, 2400, 1, [](uint32_t, uint8_t) { return 20000; });
    Parameters p = SamplerPatch(); assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0);
    float L, R, prior = 0, jump = 0; unsigned sounding = 0;
    for(unsigned i = 0; i < 4800; ++i) {
        rig.engine.Process(0, 0, L, R);
        if(i > 100) jump = std::max(jump, std::fabs(L - prior));
        prior = L; if(L > 1e-6f) ++sounding;
    }
    assert(rig.engine.ActiveVoices() == 0 && sounding > 2300 && sounding < 2420 && jump < 0.01f);
    // Loop with crossfade: a 15.25-period window (a quarter-period mismatch at
    // the wrap, i.e. a full-scale jump without a crossfade) stays continuous.
    Rig loop; loop.Sine(0, 48000, 100.f);
    p.sample_loop = true; p.sample_start = 0.25f; p.sample_end = 0.25f + 1525.f / 48000.f; p.sample_xfade = 0.02f; // 5 ms
    assert(loop.engine.ApplyPatch(p)); loop.engine.Note(60, 127, 0); loop.Run(200);
    prior = 0; jump = 0;
    for(unsigned i = 0; i < 9600; ++i) { loop.engine.Process(0, 0, L, R); if(i) jump = std::max(jump, std::fabs(L - prior)); prior = L; }
    const float sine_step = 0.5f * 20000.f / 32768.f * 6.2831853f / 100.f;   // steepest slope of the sine itself
    assert(loop.engine.ActiveVoices() == 1 && jump < 1.6f * sine_step);
    // ... and keeps its level through the wrap (a dip would fall to about half
    // in the period around it; two quarter-shifted sines crossfade to >= 0.71).
    float lowest = 1.f, steady = 0.f;
    for(unsigned block = 0; block < 96; ++block) {
        float peak = 0.f;
        for(unsigned i = 0; i < 100; ++i) { loop.engine.Process(0, 0, L, R); peak = std::max(peak, std::fabs(L)); }
        lowest = std::min(lowest, peak); steady = std::max(steady, peak);
    }
    assert(lowest > 0.65f * steady);
    // Same window without room for a crossfade (start at 0): a dip, still no hard jump.
    p.sample_start = 0.f; p.sample_end = 1525.f / 48000.f; assert(loop.engine.ApplyPatch(p));
    loop.engine.Note(60, 0, 0); loop.Run(4800); loop.engine.Note(60, 127, 0); loop.Run(200);
    prior = 0; jump = 0;
    for(unsigned i = 0; i < 9600; ++i) { loop.engine.Process(0, 0, L, R); if(i) jump = std::max(jump, std::fabs(L - prior)); prior = L; }
    assert(jump < 3.f * sine_step);                                           // a 2 ms dip, not a 0.3 jump
    // Reverse: a rising ramp plays falling.
    Rig rev; rev.Fill(0, 9600, 1, [](uint32_t i, uint8_t) { return static_cast<int>(i * 3) - 14400; });
    p = SamplerPatch(); p.sample_reverse = true; assert(rev.engine.ApplyPatch(p)); rev.Run(9600);
    rev.engine.Note(60, 127, 0); rev.Run(100, &L);
    float later; rev.Run(2000, &later);
    assert(later < L - 0.05f);
}

void GateAndTrigger() {
    Rig rig; rig.Sine(0, 48000, 100.f);
    Parameters p = SamplerPatch(); p.sample_loop = true; p.attack = 0.01f; p.release = 0.02f; assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0); rig.Run(9600);
    assert(rig.engine.ActiveVoices() == 1);                                  // gate: held
    rig.engine.Note(60, 0, 0); rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    p.sample_gate = false; assert(rig.engine.ApplyPatch(p));                 // trigger: releases after the attack
    rig.engine.Note(60, 127, 0); rig.Run(2400);
    assert(rig.engine.ActiveVoices() == 1);                                  // still in its ~100 ms release
    rig.Run(9600); assert(rig.engine.ActiveVoices() == 0);
    rig.engine.Note(62, 127, 0); rig.engine.Note(62, 0, 0); rig.Run(480);     // key-up ignored
    assert(rig.engine.ActiveVoices() == 1);
}

void LoadingAndHandoff() {
    Rig rig; rig.Sine(0, 48000, 100.f); rig.Sine(kRamSlot, 48000, 100.f);
    rig.table.slots[0].loaded.store(0);
    Parameters p = SamplerPatch(); p.sample_loop = true; assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0);
    assert(rig.Run(2400) == 0.f && rig.engine.ActiveVoices() == 1);          // nothing readable yet: silence
    rig.table.slots[0].loaded.store(48000, std::memory_order_release);
    assert(rig.Run(2400) > 0.2f);                                             // plays once loaded
    // Detach file voices; the recording keeps playing unless included.
    p.sample_slot = kRamSlot; assert(rig.engine.ApplyPatch(p)); rig.engine.Note(64, 127, 0);
    rig.engine.ReleaseSampleVoices(false); rig.Run(200);
    assert(!rig.engine.SampleVoicesActive(false) && rig.engine.SampleVoicesActive(true));
    rig.engine.ReleaseSampleVoices(true); rig.Run(200); assert(!rig.engine.SampleVoicesActive(true));
}

void StereoVoicesAndCompatibility() {
    Rig rig; rig.Sine(0, 48000, 100.f, 2);                                    // R = -L
    Parameters p = SamplerPatch(); p.sample_loop = true; assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0);
    float L, R; double correlation = 0;
    for(unsigned i = 0; i < 4800; ++i) { rig.engine.Process(0, 0, L, R); correlation += L * R; }
    assert(correlation < -1.0);
    for(uint8_t n = 61; n < 68; ++n) rig.engine.Note(n, 127, 0);
    assert(rig.engine.ActiveVoices() == 7);
    // Samples go through the per-voice filter: a 4.8 kHz sample at 40 Hz cutoff is nearly silent.
    Rig bright, dark; bright.Sine(0, 48000, 10.f); dark.Sine(0, 48000, 10.f);
    Parameters open = SamplerPatch(); open.sample_loop = true; Parameters closed = open; closed.cutoff = 0.f;
    assert(bright.engine.ApplyPatch(open) && dark.engine.ApplyPatch(closed));
    bright.Run(9600); dark.Run(9600); bright.engine.Note(60, 127, 0); dark.engine.Note(60, 127, 0);
    bright.Run(4800); dark.Run(4800);
    assert(dark.Run(4800) < 0.01f * bright.Run(4800));
    // v4 with the oscillator source renders exactly like the same v3 patch.
    Rig a, b; Parameters v3; v3.version = 3; v3.synth = true; v3.waveform = 2; v3.osc2_level = 0.4f; v3.resonance = 0.5f;
    v3.lfo_pitch = 0.2f; v3.reverb_mix = 0.3f; Parameters v4 = v3; v4.version = 4;
    assert(a.engine.ApplyPatch(v3) && b.engine.ApplyPatch(v4));
    a.engine.Note(57, 90, 1); b.engine.Note(57, 90, 1);
    for(unsigned i = 0; i < 9600; ++i) {
        float al, ar, bl, br; a.engine.Process(0.1f, 0.1f, al, ar); b.engine.Process(0.1f, 0.1f, bl, br);
        assert(al == bl && ar == br);
    }
    // Switching between oscillators and sampler silences (different voice architecture).
    v4.source = 1; assert(b.engine.ApplyPatch(v4) && b.engine.ActiveVoices() == 0);
    // Knobs: sampler start/end keep start < end; v3 refuses sampler parameters.
    assert(b.engine.Apply({Parameter::Knob2, 0.9f}) && b.engine.Apply({Parameter::Knob3, 0.2f}));
    assert(b.engine.GetParameters().sample_start == 0.9f && b.engine.GetParameters().sample_end > 0.9f);
    assert(!a.engine.Apply({Parameter::SampleStart, 0.5f}) && a.engine.Apply({Parameter::Knob2, 0.5f})
           && a.engine.GetParameters().time == 0.5f);
}

void RetriggerAndFuzz() {
    // Restarting a loud voice keeps a short decaying tail: no full-scale jump.
    Rig rig; rig.Fill(0, 48000, 1, [](uint32_t i, uint8_t) { return i < 24000 ? 25000 : -25000; });
    Parameters p = SamplerPatch(); assert(rig.engine.ApplyPatch(p));
    rig.engine.Note(60, 127, 0); rig.Run(30000);                              // now playing the -25000 half
    float L, R, prior; rig.engine.Process(0, 0, prior, R);
    rig.engine.Note(60, 127, 0);
    float jump = 0;
    for(unsigned i = 0; i < 480; ++i) { rig.engine.Process(0, 0, L, R); jump = std::max(jump, std::fabs(L - prior)); prior = L; }
    assert(jump < 0.1f);
    // Random patches and notes never produce NaN or out-of-range output.
    uint32_t seed = 7; auto rnd = [&seed]() { seed = seed * 1664525u + 1013904223u; return (seed >> 8) / 16777216.f; };
    Rig fuzz; fuzz.Sine(0, 3000, 37.f, 2, 0.6f); fuzz.Sine(3, 50, 7.f, 1, 2.f); fuzz.Sine(kRamSlot, 900, 11.f, 2);
    for(unsigned trial = 0; trial < 40; ++trial) {
        Parameters f = SamplerPatch();
        f.sample_mode = rnd() < 0.5f; f.sample_slot = static_cast<uint8_t>(rnd() * 15); f.sample_pitch = rnd();
        f.sample_start = rnd() * 0.9f; f.sample_end = f.sample_start + 0.01f + rnd() * (0.99f - f.sample_start);
        f.sample_xfade = rnd(); f.sample_loop = rnd() < 0.5f; f.sample_gate = rnd() < 0.5f; f.sample_reverse = rnd() < 0.5f;
        f.glide = rnd(); f.resonance = rnd(); f.lfo_pitch = rnd(); f.attack = rnd() * 0.1f; f.release = rnd() * 0.1f;
        assert(fuzz.engine.ApplyPatch(f));
        for(unsigned k = 0; k < 6; ++k) fuzz.engine.Note(static_cast<uint8_t>(40 + rnd() * 40), static_cast<uint8_t>(1 + rnd() * 126), k % 3);
        fuzz.engine.Bend(0, static_cast<uint16_t>(rnd() * 16383));
        for(unsigned i = 0; i < 2400; ++i) { fuzz.engine.Process(0, 0, L, R); assert(std::isfinite(L) && std::isfinite(R) && std::fabs(L) <= 1.f); }
    }
}
} // namespace

int main() {
    WavFormats(); WavRejections(); HeaderRoundTrip(); FactoryTapeFiles();
    ProtocolV4(); ChromaticPitch(); KitMapping(); OneShotLoopReverse(); GateAndTrigger(); LoadingAndHandoff();
    StereoVoicesAndCompatibility(); RetriggerAndFuzz();
    std::cout << "PASS: WAV formats/rejections/header, factory TAPE files, v4 protocol, chromatic pitch, kit map, "
                 "one-shot/loop/reverse, gate/trigger, loading/handoff, stereo/voices/compatibility, retrigger/fuzz\n";
}
