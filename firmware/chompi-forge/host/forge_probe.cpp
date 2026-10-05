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
#include "../core/panel_controller.h"
#include "../core/sampler_runtime.h"
#include "../core/file_transfer.h"
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
// USB file transfer onto the simulated sample card (forge::UploadFiles).
struct CardUploads : forge::UploadFiles {
    SampleCard& card; std::vector<uint8_t> temp; bool open = false;
    explicit CardUploads(SampleCard& c) : card(c) {}
    bool Ready() override { return card.Ready(); }
    bool Open(const char*) override { temp.clear(); open = card.Ready(); return open; }
    bool Append(const uint8_t* d, uint32_t n) override { if(!open) return false; temp.insert(temp.end(), d, d + n); return true; }
    bool Finish(const char*, const char* final_path, bool verify, uint32_t size, uint32_t crc) override {
        if(!open) return false;
        open = false;
        if(verify && (temp.size() != size || forge::Crc32(0, temp.data(), temp.size()) != crc)) return false;
        card.Remove(final_path); card.files[final_path] = temp; return true;
    }
    void Abort(const char*) override { open = false; temp.clear(); }
    bool Exists(const char* path) override { return card.Has(path); }
    bool SetAsideOtherFirmware() override {      // as FatFsUploadFiles (the bootloader's ".bin" substring rule)
        std::vector<std::string> names;
        for(const auto& f : card.files)
            if(!forge::IsFirmwareName(f.first.c_str()) && forge::BootloaderMatches(f.first.c_str())) names.push_back(f.first);
        for(const auto& n : names) {
            char target[268]; unsigned attempt = 0;
            for(; attempt <= 9; ++attempt)
                if(forge::SetAsideName(n.c_str(), attempt, target, sizeof target) && !card.Has(target)) break;
            if(attempt > 9) return false;
            card.files[target] = card.files[n]; card.files.erase(n);
        }
        return true;
    }
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
    // Kit patches play white keys with samples on the simulated card (slots 1-3).
    const bool kit = engine.GetParameters().Sampler() && engine.GetParameters().sample_mode == 1;
    const uint8_t chord[3] = {static_cast<uint8_t>(kit ? 48 : 60), static_cast<uint8_t>(kit ? 50 : 64), static_cast<uint8_t>(kit ? 52 : 67)};
    if(engine.GetParameters().synth) { engine.Note(chord[0], 100, 2); engine.Note(chord[1], 90, 2); engine.Note(chord[2], 90, 2); }
    for(unsigned i = 0; i < frames; ++i) {
        if(i == rate) for(uint8_t note : chord) engine.Note(note, 0, 2);
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
    std::vector<int16_t> loop_memory(2 * 48000 * 20);            // 20 s looper (firmware: ~83 s)
    forge::Looper looper; looper.Init(loop_memory.data(), 48000 * 20, 48000.f); engine.SetLooper(&looper);
    recorder.Start(); for(int i = 0; i < 48000; ++i) recorder.Write(0.3f * std::sin(i * 0.05f), 0.3f * std::sin(i * 0.05f)); recorder.Stop();
    // Panel: the same controller as firmware, driven by development opcode 0A
    // (no hardware input). Its menu actions run here directly.
    struct ProbeSink : forge::PanelSink {
        forge::PresetStore* store; forge::Engine* engine; forge::SampleLoader* loader;
        uint8_t last_bank = 0, last_slot = forge::panel::kNoSlot;
        bool PresetAction(const forge::MenuAction& a, const forge::Parameters& snapshot) override {
            forge::Error e = forge::Error::None;
            if(a.kind == forge::MenuAction::Kind::Recall) {
                forge::Parameters patch; e = store->Load(a.bank, a.slot, patch);
                if(e == forge::Error::None) { engine->ApplyPatch(patch); last_bank = a.bank; last_slot = a.slot; return true; }
            } else if(a.kind == forge::MenuAction::Kind::Save) e = store->Save(a.bank, a.slot, snapshot);
            else if(a.kind == forge::MenuAction::Kind::Erase) e = store->Erase(a.bank, a.slot);
            else if(a.kind == forge::MenuAction::Kind::Copy) e = store->Copy(a.bank, a.slot, a.to_bank, a.to_slot);
            Flash(e == forge::Error::None);
            return true;
        }
        bool SampleJob(const forge::SampleJob& job) override { forge::SampleJob j = job; j.source = 0xff; return loader->Queue(j); }
        void Flash(bool) override {}
    } sink;
    sink.store = &store; sink.engine = &engine; sink.loader = &loader;
    forge::PanelController panel;
    CardUploads uploads(samples); forge::FileTransfer transfer; forge::InstallGate install;
    panel.SetInstallGate(&install);
    forge::InspectorSnapshot inspection; inspection.system.simulated=true;
    forge::InspectorLog event_log; forge::SpscQueue<forge::InspectorEvent,64> edges;
    std::atomic<uint32_t> event_drops{0};
    uint32_t blocks=0, revision=0, selection=0, received=0; bool was_recording=false;
    auto add=[&](forge::InspectorEventKind kind,uint32_t value) {
        event_log.Add({0,blocks/2,value,kind,0});
    };
    // One audio block plus one main-loop pass, as on the device.
    auto block = [&](forge::SampleEvent& event) {
        forge::SampleEvent e; bool finished = false;
        if(loader.Poll(samples, forge::PackSelection(engine.GetParameters()), recording.data(), e, loop_memory.data())) {
            event = e; finished = true;
            if(e.job.kind == forge::SampleJob::Kind::Save) { if(e.job.from_loop) looper.Unlock(); else recorder.Unlock(); }
            if(e.job.source == 0xff) sink.Flash(e.ok);
        }
        engine.SetSampleFilesAvailable(handoff.AudioBlock(engine));
        ++blocks;
        panel.SetInspectorEvents(&edges,&event_drops,blocks/2);
        forge::PanelInput input; input.frames = 24;               // this harness runs 24-frame blocks
        panel.Block(input, engine, recorder, sink);
        float l, r;
        for(int k = 0; k < 24; ++k) {
            engine.Process(0, 0, l, r);
            if(recorder.Recording()) { float a, b; recorder.Input(panel.Source(), 0, 0, 0, l, r, a, b); recorder.Write(a, b); }
        }
        engine.ObserveVoiceEdges(edges,event_drops,blocks/2);
        forge::InspectorEvent edge;
        while(edges.Pop(edge)) event_log.Add(edge);
        if(engine.PatchRevision()!=revision) { revision=engine.PatchRevision(); add(forge::InspectorEventKind::PatchApply,revision); }
        const uint32_t live=forge::PackSelection(engine.GetParameters());
        if(live!=selection) { selection=live; add(forge::InspectorEventKind::SampleSelection,selection); }
        if(recorder.Recording()!=was_recording) { was_recording=recorder.Recording(); add(was_recording?forge::InspectorEventKind::RecordingStart:forge::InspectorEventKind::RecordingStop,recorder.Length()); }
        return finished;
    };
    // Runs until the loader is idle; returns a finished job, if any.
    auto settle = [&](forge::SampleEvent& event) {
        bool finished = false;
        for(int i = 0; i < 100000; ++i) {
            if(block(event)) finished = true;
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
        forge::Request request; uint8_t reply[forge::kMaxReply]; size_t size = 0;
        if(frame.data[4] == forge::kFileOpcode) {                 // file transfer: the firmware's main-loop path
            settle(ignored);
            size = forge::ServeFileRequest(frame.data, frame.size, transfer, uploads, install, blocks / 2, reply, loader.Busy());
            if(transfer.SampleWritten()) { loader.Rescan(); settle(ignored); }
            install.Poll(blocks / 2);
            for(size_t i = 0; i < size; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(reply[i]) << ' ';
            std::cout << std::endl; ++replies;
            continue;
        }
        auto error = forge::DecodeRequest(frame.data, frame.size, request);
        forge::Response response;
        // Device presets: the same main-loop helpers the firmware uses.
        if(error == forge::Error::None && request.kind == forge::RequestKind::Recall) {
            forge::Request apply;
            error = forge::RecallRequest(store, request, apply);
            if(error == forge::Error::None) request = apply;
        }
        if(error == forge::Error::None && request.kind == forge::RequestKind::Panel) {
            forge::PanelEvent event;
            event.kind = static_cast<forge::PanelEvent::Kind>(request.panel_kind);
            event.id = request.panel_id; event.value = request.panel_value;
            panel.Inject(event); settle(ignored);
            size = forge::EncodePanelAck(request.sequence, reply);
        } else if(error == forge::Error::None && request.kind == forge::RequestKind::Probe) {
            ++received;
            settle(ignored);
            if(request.page>=2) {
                if(request.page==2 || !inspection.generation) {
                    ++inspection.generation; engine.Inspect(inspection.audio); panel.Inspect(inspection.audio);
                    inspection.audio.block=blocks; inspection.audio.time_ms=blocks/2;
                    inspection.audio.recording=recorder.Recording(); inspection.audio.locked=recorder.Locked();
                    inspection.audio.record_frames=recorder.Length();
                    inspection.system.uptime_ms=blocks/2; inspection.system.rx[0]=received;
                    inspection.system.event_drops=event_drops.load();
                    loader.Inspect(inspection.storage);
                    inspection.storage.present=inspection.storage.mounted=true;
                    inspection.storage.record_capacity_frames=recorder.Capacity();
                }
                size=forge::EncodeInspector(request.sequence,request.page,inspection,event_log,request.inspector_cursor,reply);
            } else {
            const uint32_t menu = panel.MenuPacked();
            {
                forge::LedView v; v.menu = menu;
                v.preset_occupancy = store.Occupancy((menu >> 4) & 7u); v.preset_card = store.Ready();
                v.last_bank = sink.last_bank; v.last_slot = sink.last_slot;
                v.sample_occupancy = loader.Occupancy((menu >> 22) & 1u, (menu >> 4) & 7u); v.sample_card = samples.Ready();
                v.recording_present = table.slots[forge::kRamSlot].loaded.load() > 0; v.recording_now = recorder.Recording();
                v.live = forge::PackSelection(engine.GetParameters()); v.blink = true;
                v.keys_down = panel.KeysDown(); v.kit_occupancy = loader.Occupancy(1, (v.live >> 2) & 7u);
                forge::Rgb keys[25], chompi; forge::ComposeLeds(v, keys, chompi);
                uint8_t leds[26][3];
                for(unsigned i = 0; i < 26; ++i) {
                    const forge::Rgb& c = i < 25 ? keys[i] : chompi;
                    const float rgb[3] = {c.r, c.g, c.b};
                    for(unsigned k = 0; k < 3; ++k) leds[i][k] = static_cast<uint8_t>(forge::Clamp(rgb[k], 0.f, 1.f) * 127.f + 0.5f);
                }
                size = forge::EncodeProbeLeds(request.sequence, leds, reply);
            }
            }
        } else if(error == forge::Error::None && request.kind == forge::RequestKind::SampleList) {
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
        else if(request.kind != forge::RequestKind::Panel && request.kind != forge::RequestKind::Probe)
            size = forge::EncodeResponse(response, 0, 0, reply);
        for(size_t i = 0; i < size; ++i) std::cout << std::hex << std::setw(2) << std::setfill('0') << unsigned(reply[i]) << ' ';
        std::cout << std::endl; ++replies; // flush: lets a host keep one stateful probe open
    }
    if(!std::cin.eof() || !replies) return 2;
    try { if(argc == 3) Render(engine, argv[2]); }
    catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
    return 0;
}
