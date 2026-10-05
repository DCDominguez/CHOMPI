// Every knob page on every starter preset must change what you hear (DC, 2026-10-05:
// "check the audio, not just the values"). For each preset (as CHOMPI stores it: v5),
// knob and page, two fresh panels play the same phrase (C4 held 1.2 s from the keybed,
// or a line-in tone on effects-only patches); one first turns that page 10 clicks.
// The renders must differ: spectrum, loudness, envelope or stereo balance. A control
// that only works together with another one (LFO speed without LFO depth, detune
// without the second oscillator...) is reported n/a with the reason, not failed.
// Input: build/starter_patches.txt (tests/dump_starter_patches.py).
#include <cassert>
#include <cmath>
#include <complex>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "../core/panel_controller.h"
#include "../core/runtime.h"

using namespace forge;

namespace {
struct Sink : PanelSink {
    bool PresetAction(const MenuAction&, const Parameters&) override { return true; }
    bool SampleJob(const forge::SampleJob&) override { return true; }
    void Flash(bool) override {}
};
constexpr uint8_t kKeyC4 = 18;                        // KEY_8, MIDI 60
constexpr unsigned kHoldBlocks = 2400, kTailBlocks = 4000;  // 1.2 s held + 2 s after, in 24-frame blocks
constexpr uint32_t kSampleFrames = 3 * 48000;
std::vector<int16_t> SawSample() {                    // 3 s stereo 220 Hz saw: rich harmonics, outlasts the held note
    std::vector<int16_t> s(2 * kSampleFrames);
    for(unsigned i = 0; i < kSampleFrames; ++i) {
        const float t = std::fmod(i * 220.f / 48000.f, 1.f);
        s[2 * i] = s[2 * i + 1] = static_cast<int16_t>(9000.f * (2.f * t - 1.f));
    }
    return s;
}
struct Rig {
    std::vector<float> l = std::vector<float>(48002), r = std::vector<float>(48002), rv = std::vector<float>(Reverb::Required(48000));
    std::vector<float> warble = std::vector<float>(2 * tape::Warble::kLength);
    std::vector<int16_t> rec = std::vector<int16_t>(2 * kSampleFrames);
    SampleTable table; Engine engine; Recorder recorder; PanelController panel; Sink sink; PanelInput hw;
    explicit Rig(const Parameters& patch, const std::vector<int16_t>& sample) {
        assert(engine.Init(48000.f, l.data(), r.data(), l.size(), rv.data(), rv.size()));
        engine.SetWarbleMemory(warble.data());
        recorder.Init(rec.data(), kSampleFrames, &table.slots[kRamSlot], 48000.f);
        for(auto& slot : table.slots) {               // every slot (and the recording) holds the saw
            slot.data = sample.data(); slot.frames = kSampleFrames; slot.channels = 2; slot.rate_ratio = 1.f; slot.gain = 1.f;
            slot.loaded.store(kSampleFrames);
        }
        engine.SetSamples(&table);
        assert(engine.ApplyPatch(patch));
        hw.frames = 24; hw.toggle_up = true;           // menu position: CHOMPI is never pressed here
        Block(0.f, nullptr);
    }
    void Block(float in, std::vector<float>* out) {
        panel.Block(hw, engine, recorder, sink);
        for(int i = 0; i < 24; ++i) {
            float a, b; engine.Process(in, in, a, b);
            if(out) { out->push_back(a); out->push_back(b); }
        }
        for(auto& t : hw.turns) t = 0;
    }
    void Tap(uint8_t button) { hw.keys |= uint64_t(1) << button; Block(0.f, nullptr); hw.keys &= ~(uint64_t(1) << button); Block(0.f, nullptr); }
    std::vector<float> Phrase(bool line_in) {
        std::vector<float> out; unsigned n = 0;
        auto tone = [&]() { return line_in && n < kHoldBlocks * 24 ? .2f * std::sin(6.2831853f * 440.f * n / 48000.f) : 0.f; };
        if(!line_in) hw.keys |= uint64_t(1) << kKeyC4;
        for(unsigned b = 0; b < kHoldBlocks + kTailBlocks; ++b) {
            if(b == kHoldBlocks) hw.keys &= ~(uint64_t(1) << kKeyC4);
            panel.Block(hw, engine, recorder, sink);
            for(int i = 0; i < 24; ++i, ++n) { float a, c; const float x = tone(); engine.Process(x, x, a, c); out.push_back(a); out.push_back(c); }
        }
        return out;
    }
};
void Fft(std::vector<std::complex<double>>& a) {
    const size_t n = a.size();
    for(size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for(; j & bit; bit >>= 1) j ^= bit; j ^= bit; if(i < j) std::swap(a[i], a[j]); }
    for(size_t len = 2; len <= n; len <<= 1) {
        const std::complex<double> w(std::cos(-2 * M_PI / len), std::sin(-2 * M_PI / len));
        for(size_t i = 0; i < n; i += len) {
            std::complex<double> wn(1);
            for(size_t j = 0; j < len / 2; ++j, wn *= w) { auto u = a[i + j], v = a[i + j + len / 2] * wn; a[i + j] = u + v; a[i + j + len / 2] = u - v; }
        }
    }
}
struct Features { std::vector<double> spectrum, envelope; double rms = 0, balance = 0; };
Features Measure(const std::vector<float>& stereo) {
    Features f; const size_t frames = stereo.size() / 2;
    std::vector<std::complex<double>> x(131072);
    double el = 0, er = 0;
    for(size_t i = 0; i < frames; ++i) {
        const double l = stereo[2 * i], r = stereo[2 * i + 1];
        if(i < x.size()) x[i] = l + r;
        el += l * l; er += r * r;
        if(i % 960 == 0) f.envelope.push_back(0);   // 20 ms frames
        f.envelope.back() += l * l + r * r;
    }
    f.rms = std::sqrt((el + er) / (2.0 * frames)); f.balance = (el - er) / (el + er + 1e-18);
    Fft(x);
    for(size_t i = 0; i < x.size() / 2; ++i) f.spectrum.push_back(std::abs(x[i]));
    return f;
}
// Relative change: the largest of spectrum, envelope, loudness (dB / 6) and balance.
double Change(const Features& a, const Features& b, std::string& what) {
    double ds = 0, ss = 0, de = 0, se = 0;
    for(size_t i = 0; i < a.spectrum.size(); ++i) { ds += std::fabs(a.spectrum[i] - b.spectrum[i]); ss += a.spectrum[i]; }
    for(size_t i = 0; i < a.envelope.size(); ++i) { de += std::fabs(std::sqrt(a.envelope[i]) - std::sqrt(b.envelope[i])); se += std::sqrt(a.envelope[i]); }
    const double spectrum = ds / (ss + 1e-18), envelope = de / (se + 1e-18);
    const double loudness = std::fabs(20 * std::log10((b.rms + 1e-12) / (a.rms + 1e-12))) / 6.0, balance = std::fabs(a.balance - b.balance);
    double best = spectrum; what = "spectrum";
    if(envelope > best) { best = envelope; what = "envelope"; }
    if(loudness > best) { best = loudness; what = "loudness"; }
    if(balance > best) { best = balance; what = "balance"; }
    return best;
}
// Controls that need another one to be heard; the reason, or nullptr.
const char* Dependency(Parameter p, const Parameters& s) {
    using P = Parameter;
    const bool lfo = s.lfo_pitch > 0 || s.lfo_filter > 0 || s.lfo_amp > 0;
    if(p == P::LfoRate && !lfo) return "LFO speed needs an LFO depth (pitch, filter or amp)";
    if((p == P::LfoRate || p == P::LfoFilter || p == P::LfoPitch || p == P::LfoAmp) && s.lfo_wheel)
        return "the patch puts its LFO on the mod wheel (MIDI CC 1); CHOMPI's panel has no wheel";
    if(p == P::SampleEnd && s.Sampler() && !s.sample_loop) return "loop off: the end point is heard only if the note lasts that long (test sample 3 s)";
    if(p == P::Decay && s.sustain >= .7f) return "decay is subtle when sustain is high";
    if((p == P::Time || p == P::Feedback) && (s.mix == 0 || s.bypass)) return "delay time/feedback need delay mix";
    if(p == P::Osc2Detune && s.osc2_level == 0) return "detune needs the second oscillator (osc2 level 0)";
    if(p == P::SampleXfade && !s.sample_loop) return "loop crossfade needs loop on";
    if((p == P::ReverbSize || p == P::ReverbDamping) && s.reverb_mix == 0) return "reverb size/damping need reverb mix";
    if(p == P::FilterAmount && s.cutoff > .95f && s.filter_sustain == 0)
        return "filter envelope on a wide-open filter acts only on the attack";
    if((p == P::Resonance || p == P::FilterAmount || p == P::LfoFilter) && !s.Sampler() && s.waveform == 0 && s.osc2_level == 0 && s.noise == 0 && s.cutoff > 0.75f)
        return "filter controls on a pure sine with the filter wide open";
    return nullptr;
}
const char* Name(Parameter p) {
    static const char* const names[] = {"delay mix", "delay time", "feedback", "level", "bypass", "cutoff", "resonance", "reverb mix",
        "sample pitch", "sample start", "sample end", "knob1", "knob2", "knob3", "knob4", "filter env", "attack", "decay", "sustain",
        "release", "LFO rate", "LFO pitch", "LFO filter", "LFO amp", "osc2 level", "osc2 detune", "noise", "glide", "reverb size",
        "reverb damping", "loop crossfade", "TAPE pitch", "voice gain", "pan", "space", "saturation", "warble", "DJ filter",
        "DJ resonance", "compressor", "input gain"};
    return names[static_cast<unsigned>(p)];
}
} // namespace

int main() {
    std::ifstream in("build/starter_patches.txt");
    assert(in && "run: python3 tests/dump_starter_patches.py > build/starter_patches.txt");
    const std::vector<int16_t> saw = SawSample();
    static const char* const kSwitch[4] = {"SW4", "SW1", "SW2", "SW3"};
    unsigned checked = 0, dependent = 0; std::vector<std::string> silent;
    for(std::string line; std::getline(in, line);) {
        std::istringstream s(line); std::string name; s >> name; std::vector<uint8_t> bytes; int v; while(s >> v) bytes.push_back(uint8_t(v));
        Request request; assert(DecodeRequest(bytes.data(), bytes.size(), request) == Error::None);
        const Parameters& patch = request.patch;
        const bool line_in = !patch.synth;
        for(unsigned knob = 0; knob < 4; ++knob) for(unsigned page = 0; page < knobs::Pages(knob, patch); ++page) {
            Rig plain(patch, saw), turned(patch, saw);
            for(unsigned i = 0; i < page; ++i) { plain.Tap(panel::kKnobEncoder[knob]); turned.Tap(panel::kKnobEncoder[knob]); }
            const Parameter target = knobs::Target(knob, page, patch);
            const int direction = turned.engine.Value(target) < .5f ? 1 : -1;
            turned.hw.turns[panel::kKnobEncoder[knob]] = static_cast<int16_t>(10 * direction); turned.Block(0.f, nullptr);
            plain.Block(0.f, nullptr);
            std::string what; const double change = Change(Measure(plain.Phrase(line_in)), Measure(turned.Phrase(line_in)), what);
            const char* depends = Dependency(patch.Resolve(target), patch);
            std::cout << name << " " << kSwitch[knob] << " p" << page + 1 << " " << Name(patch.Resolve(target)) << ": ";
            if(change > 0.05) { std::cout << int(change * 100 + .5) << "% (" << what << ")\n"; ++checked; }
            else if(depends) { std::cout << "n/a (" << depends << ")\n"; ++dependent; }
            else { std::cout << "SILENT " << change * 100 << "%\n"; silent.push_back(name + " " + kSwitch[knob] + " p" + std::to_string(page + 1)); }
        }
    }
    for(const auto& s : silent) std::cout << "inaudible: " << s << "\n";
    assert(silent.empty() && checked > 100);
    std::cout << "PASS: knob pages change the sound (" << checked << " audible, " << dependent << " need another control)\n";
}
