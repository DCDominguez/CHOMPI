// Offline harness: raw hex MIDI on stdin -> one hex reply per request.
// Optional --render writes four seconds of a synthetic stereo pluck through
// the SAME Engine as firmware. Diagnostics here are synthetic zero readings.
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#include "../core/midi_framer.h"
#include "../core/runtime.h"
#include "../core/sampler_runtime.h"
#include "../tests/sample_card.h"

// Simulated sample card: a few TAPE-named files and a one-second recording, so
// host tools can list, save, copy and erase samples. Synthetic content only.
std::vector<uint8_t> SineWav(uint32_t frames, float hz) {
    std::vector<uint8_t> f(forge::kWavHeaderSize + frames * 4);
    uint8_t header[forge::kWavHeaderSize]; forge::WriteWavHeader(frames, header);
    std::copy(header, header + forge::kWavHeaderSize, f.begin());
    for(uint32_t i = 0; i < frames; ++i) {
        const int16_t v = static_cast<int16_t>(12000.f * std::sin(6.2831853f * hz * i / 48000.f));
        for(int c = 0; c < 2; ++c) { f[44 + 4 * i + 2 * c] = v & 255; f[45 + 4 * i + 2 * c] = (uint16_t(v) >> 8) & 255; }
    }
    return f;
}
// In-memory SD card for device-preset requests (lives as long as the probe).
struct MemoryCard : forge::Storage {
    std::map<std::string, std::vector<uint8_t>> files;
    bool Ready() override { return true; }
    bool Read(const char* path, uint8_t* buffer, size_t capacity, size_t& size) override {
        auto it = files.find(path);
        if(it == files.end() || it->second.size() > capacity) return false;
        size = it->second.size(); std::memcpy(buffer, it->second.data(), size); return true;
    }
    bool Write(const char* path, const uint8_t* data, size_t size) override { files[path].assign(data, data + size); return true; }
    bool Remove(const char* path) override { files.erase(path); return true; }
};
void Word(std::ostream& out, uint32_t value, unsigned bytes) {
    for(unsigned i = 0; i < bytes; ++i) out.put(static_cast<char>((value >> (8 * i)) & 255));
}
void Render(forge::Engine& engine, const char* path) {
    std::ofstream out(path, std::ios::binary);
    if(!out) throw std::runtime_error("Cannot open WAV output");
    constexpr unsigned rate = 48000, frames = rate * 4, data_size = frames * 4;
    out.write("RIFF", 4); Word(out, 36 + data_size, 4); out.write("WAVEfmt ", 8);
    Word(out, 16, 4); Word(out, 1, 2); Word(out, 2, 2); Word(out, rate, 4);
    Word(out, rate * 4, 4); Word(out, 4, 2); Word(out, 16, 2);
    out.write("data", 4); Word(out, data_size, 4);
    float left, right;
    // Settle parameter ramps before the test signal.
    for(unsigned i = 0; i < rate; ++i) engine.Process(0, 0, left, right);
    if(engine.GetParameters().synth) { engine.Note(60, 100, 2); engine.Note(64, 90, 2); engine.Note(67, 90, 2); }
    for(unsigned i = 0; i < frames; ++i) {
        if(i == rate) { engine.Note(60, 0, 2); engine.Note(64, 0, 2); engine.Note(67, 0, 2); }
        const float t = float(i) / rate;
        const float envelope = i < rate / 2 ? std::exp(-t * 12.f) : 0.f;
        const float l = 0.6f * envelope * std::sin(6.2831853f * 220.f * t);
        const float r = 0.6f * envelope * std::sin(6.2831853f * 330.f * t);
        engine.Process(l, r, left, right);
        Word(out, static_cast<uint16_t>(static_cast<int16_t>(left * 32767.f)), 2);
        Word(out, static_cast<uint16_t>(static_cast<int16_t>(right * 32767.f)), 2);
    }
    if(!out) throw std::runtime_error("Failed writing WAV");
}
int main(int argc, char** argv) {
    if(argc != 1 && !(argc == 3 && std::string(argv[1]) == "--render")) return 2;
    forge::Engine engine; std::vector<float> l(48002), r(48002), reverb(forge::Reverb::Required(48000));
    if(!engine.Init(48000, l.data(), r.data(), l.size(), reverb.data(), reverb.size())) return 2;
    MemoryCard card; forge::PresetStore store(card); store.Rescan();
    // Sampler: same loader, handoff and recorder as firmware, on an in-memory card.
    static forge::SampleTable table;
    static std::vector<int16_t> pool(1u << 22), recording(2 * 48000 * 4);
    static std::vector<uint8_t> scratch(16384);
    SampleCard samples; forge::SampleHandoff handoff; forge::SampleLoader loader; forge::Recorder recorder;
    samples.files["jammi_a1.wav"] = SineWav(24000, 220.f);
    samples.files["cubbi_a1.wav"] = SineWav(4800, 110.f);
    samples.files["cubbi_a2.wav"] = SineWav(4800, 330.f);
    engine.SetSamples(&table);
    loader.Init(&table, &handoff, pool.data(), static_cast<uint32_t>(pool.size()), scratch.data(), static_cast<uint32_t>(scratch.size()));
    recorder.Init(recording.data(), 48000 * 4, &table.slots[forge::kRamSlot], 48000.f);
    recorder.Start(); for(int i = 0; i < 48000; ++i) recorder.Write(0.3f * std::sin(i * 0.05f), 0.3f * std::sin(i * 0.05f)); recorder.Stop();
    // Runs the loader (and the audio side of the handoff) until idle; returns a finished job, if any.
    auto settle = [&](forge::SampleEvent& event) {
        bool finished = false;
        for(int i = 0; i < 100000; ++i) {
            forge::SampleEvent e;
            if(loader.Poll(samples, forge::PackSelection(engine.GetParameters()), recording.data(), e)) { event = e; finished = true; }
            engine.SetSampleFilesAvailable(handoff.AudioBlock(engine));
            float l, r; for(int k = 0; k < 24; ++k) engine.Process(0, 0, l, r);
            if(!loader.Busy() && i > 2) break;
        }
        return finished;
    };
    forge::SampleEvent ignored; settle(ignored);
    forge::MidiFramer parser; forge::MidiFrame frame;
    unsigned byte, replies = 0;
    while(std::cin >> std::hex >> byte) {
        if(byte > 255) return 2;
        if(!parser.Feed(static_cast<uint8_t>(byte), frame)) continue;
        if(frame.kind != forge::MidiFrame::Kind::SysEx || !forge::IsRequest(frame.data, frame.size)) continue;
        forge::Request request; uint8_t reply[forge::kMaxReply]; size_t size;
        auto error = forge::DecodeRequest(frame.data, frame.size, request);
        forge::Response response;
        // Device presets: the same main-loop helpers the firmware uses.
        if(error == forge::Error::None && request.kind == forge::RequestKind::Recall) {
            forge::Request apply;
            error = forge::RecallRequest(store, request, apply);
            if(error == forge::Error::None) request = apply;
        }
        if(error == forge::Error::None && request.kind == forge::RequestKind::SampleList) {
            settle(ignored);
            response = forge::SampleListReply(loader, samples.Ready(), table.slots[forge::kRamSlot], 48000 * 4, request);
        } else if(error == forge::Error::None && request.kind == forge::RequestKind::SampleJob) {
            forge::SampleJob job;
            if(request.action == forge::SampleAction::Save) {
                response = forge::LockForSave(recorder, request);
                if(response.kind == forge::ResponseKind::SampleSnapshot) job = forge::SaveJob(response);
                else error = response.error;
            } else error = forge::FileJob(loader, request, job);
            forge::SampleEvent event;
            if(error == forge::Error::None && !(loader.Queue(job) && settle(event))) error = forge::Error::StorageBusy;
            if(request.action == forge::SampleAction::Save) recorder.Unlock();
            if(error == forge::Error::None) { response = forge::SampleDoneReply(event); error = response.error; }
        } else if(error == forge::Error::None && request.kind == forge::RequestKind::Erase) response = forge::EraseReply(store, request);
        else if(error == forge::Error::None && request.kind == forge::RequestKind::List) response = forge::ListReply(store, request);
        else if(error == forge::Error::None) {
            forge::ExecuteRequest(request, engine, response);
            settle(ignored);                       // a sampler patch loads its sample(s), as the device does
            if(response.kind == forge::ResponseKind::Snapshot)
                response = forge::StoreReply(store, response.sequence, 0, response.bank, response.slot, response.patch);
        }
        if(error != forge::Error::None) size = forge::EncodeError(forge::Read14(frame.data + 5), error, reply);
        else size = forge::EncodeResponse(response, 0, 0, reply);
        for(size_t i = 0; i < size; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(reply[i]) << ' ';
        std::cout << std::endl; ++replies; // flush: lets a host keep one stateful probe open
    }
    if(!std::cin.eof() || !replies) return 2;
    try { if(argc == 3) Render(engine, argv[2]); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
