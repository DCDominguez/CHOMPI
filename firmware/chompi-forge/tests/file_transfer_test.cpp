#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../core/file_transfer.h"

using namespace forge;

// In-memory card: files by name, one temp file being written. The set-aside follows the
// bootloader's rule (any visible root name containing ".bin"/".BIN").
struct MemoryCard : UploadFiles {
    std::map<std::string, std::vector<uint8_t>> files;
    std::vector<uint8_t> temp; bool open = false, ready = true, fail_append = false, corrupt_write = false, fail_rename = false;
    bool Ready() override { return ready; }
    bool Open(const char*) override { if(!ready) return false; temp.clear(); open = true; return true; }
    bool Append(const uint8_t* d, uint32_t n) override {
        if(!open || fail_append) return false;
        temp.insert(temp.end(), d, d + n);
        if(corrupt_write) temp.back() ^= 0x10;                          // the card stored something else
        return true;
    }
    bool Finish(const char*, const char* final_path, bool verify, uint32_t size, uint32_t crc) override {
        if(!open) return false;
        open = false;
        if(verify && (temp.size() != size || Crc32(0, temp.data(), temp.size()) != crc)) return false;
        files[final_path] = temp; return true;
    }
    void Abort(const char*) override { open = false; temp.clear(); }
    bool Exists(const char* path) override { return files.count(path) > 0; }
    bool SetAsideOtherFirmware() override {
        std::vector<std::string> names;
        for(auto& f : files) if(!IsFirmwareName(f.first.c_str()) && BootloaderMatches(f.first.c_str())) names.push_back(f.first);
        for(auto& n : names) {
            if(fail_rename) return false;
            char target[268]; unsigned attempt = 0;
            for(; attempt <= 9; ++attempt) if(SetAsideName(n.c_str(), attempt, target, sizeof target) && !files.count(target)) break;
            if(attempt > 9) return false;
            files[target] = files[n]; files.erase(n);
        }
        return true;
    }
    // What the bootloader would flash: the visible root names containing ".bin".
    std::vector<std::string> BootloaderSees() const {
        std::vector<std::string> seen;
        for(auto& f : files) if(BootloaderMatches(f.first.c_str())) seen.push_back(f.first);
        return seen;
    }
};
// A minimal image the bootloader accepts: stack at the top of DTCM, Thumb entry inside.
std::vector<uint8_t> Firmware(size_t size, uint8_t seed) {
    std::vector<uint8_t> image(size);
    for(size_t i = 0; i < size; ++i) image[i] = uint8_t(i * 31 + seed);
    const uint32_t words[2] = {0x20020000u, kAppStart + 0x1a1u};
    for(unsigned w = 0; w < 2; ++w) for(unsigned b = 0; b < 4; ++b) image[4 * w + b] = uint8_t(words[w] >> (8 * b));
    return image;
}

std::vector<uint8_t> Message(FileOp op, std::vector<uint8_t> body, uint16_t sequence = 7) {
    std::vector<uint8_t> m(8); Header(m.data(), kFileOpcode, sequence); m[7] = static_cast<uint8_t>(op);
    m.insert(m.end(), body.begin(), body.end()); m.push_back(0); m.back() = Checksum(m.data(), m.size() - 1);
    return m;
}
std::vector<uint8_t> Word(uint32_t v) { std::vector<uint8_t> w(5); Write35(w.data(), v); return w; }
std::vector<uint8_t> Begin(const std::string& name, uint32_t size) {
    auto body = Word(size); body.push_back(static_cast<uint8_t>(name.size()));
    body.insert(body.end(), name.begin(), name.end());
    return Message(FileOp::Begin, body);
}
std::vector<uint8_t> Data(uint32_t offset, const uint8_t* raw, size_t n) {
    auto body = Word(offset);
    for(size_t g = 0; g < n; g += 7) {
        const size_t count = std::min<size_t>(7, n - g);
        uint8_t high = 0;
        for(size_t i = 0; i < count; ++i) high |= uint8_t((raw[g + i] >> 7) << i);
        body.push_back(high);
        for(size_t i = 0; i < count; ++i) body.push_back(raw[g + i] & 127);
    }
    return Message(FileOp::Data, body);
}
FileRequest Decode(const std::vector<uint8_t>& m, Error expect = Error::None) {
    FileRequest r; assert(DecodeFileRequest(m.data(), m.size(), r) == expect); return r;
}

void CodecAndNames() {
    const uint8_t check[] = {'1','2','3','4','5','6','7','8','9'};
    assert(Crc32(0, check, 9) == 0xcbf43926u);                         // the standard CRC-32 check value
    assert(Crc32(Crc32(0, check, 4), check + 4, 5) == 0xcbf43926u);     // streaming
    for(const char* ok : {"FORGE.bin", "forge.BIN", "jammi_a1.wav", "CUBBI_E14.WAV"}) assert(AllowedUploadName(ok));
    for(const char* bad : {"jammi_a1_double.wav", "jammi_a15.wav", "cubbi_f1.wav", "TAPE.bin", "FORGE/B1S01.FPR", "x.wav", ""})
        assert(!AllowedUploadName(bad));
    assert(kFileRequest == 270 && kFilePacked == 256);
    auto r = Decode(Begin("jammi_b3.wav", 123456));
    assert(r.op == FileOp::Begin && r.size == 123456 && std::string(r.name) == "jammi_b3.wav");
    Decode(Begin("../evil.wav", 10), Error::Patch);
    Decode(Begin("FORGE.bin", kFirmwareMax + 1), Error::Patch);
    Decode(Begin("FORGE.bin", 0), Error::Patch);
    Decode(Begin("jammi_a1.wav", kSampleFileMax), Error::None);
    // Every byte value survives the 7-bit packing, at every length up to a full chunk.
    std::vector<uint8_t> raw(kFileChunk); for(size_t i = 0; i < raw.size(); ++i) raw[i] = uint8_t(i * 37 + 200);
    for(size_t n : {size_t(1), size_t(6), size_t(7), size_t(8), size_t(100), kFileChunk}) {
        auto m = Data(4096, raw.data(), n); auto d = Decode(m);
        assert(d.offset == 4096 && d.length == n && std::equal(raw.begin(), raw.begin() + n, d.data));
        if(n == kFileChunk) assert(m.size() == kFileRequest);
    }
    auto big = Data(0, raw.data(), kFileChunk); big.insert(big.end() - 1, 0); big.back() = 0; big.back() = Checksum(big.data(), big.size() - 1);
    Decode(big, Error::Length);                                         // longer than a chunk
    auto lonely = Data(0, raw.data(), 7); lonely.insert(lonely.end() - 1, 5); lonely.back() = 0; lonely.back() = Checksum(lonely.data(), lonely.size() - 1);
    Decode(lonely, Error::Length);                                      // a high-bit byte with no data byte
    auto stray = Data(0, raw.data(), 3); stray[13] = 0x70; stray.back() = 0; stray.back() = Checksum(stray.data(), stray.size() - 1);
    Decode(stray, Error::Patch);                                        // high bits for absent bytes
    auto broken = Data(0, raw.data(), 10); broken[15] ^= 1; Decode(broken, Error::Checksum);
    Decode(Message(FileOp::Status, {}));
    Decode(Message(static_cast<FileOp>(9), {}), Error::Patch);
    uint8_t reply[16]; assert(EncodeFileReply(7, FileOp::Data, 5, 300000, reply) == 16 && Checksum(reply, 16) == 0 && Read35(reply + 10) == 300000);
}

void Upload(FileTransfer& t, MemoryCard& card, const std::string& name, const std::vector<uint8_t>& file, Error end = Error::None, uint32_t crc_xor = 0) {
    assert(t.Begin(card, Decode(Begin(name, static_cast<uint32_t>(file.size())))) == Error::None && t.Active());
    for(size_t at = 0; at < file.size(); at += kFileChunk)
        assert(t.Data(card, Decode(Data(static_cast<uint32_t>(at), file.data() + at, std::min(kFileChunk, file.size() - at)))) == Error::None);
    auto e = Message(FileOp::End, Word(Crc32(0, file.data(), file.size()) ^ crc_xor));
    assert(t.End(card, Decode(e)) == end && !t.Active());
}

void TransferAndInstall() {
    MemoryCard card; FileTransfer t; InstallGate gate;
    std::vector<uint8_t> sample(5000); for(size_t i = 0; i < sample.size(); ++i) sample[i] = uint8_t(i * 7);
    Upload(t, card, "cubbi_a1.wav", sample);
    assert(card.files["cubbi_a1.wav"] == sample && t.SampleWritten() && !t.SampleWritten());
    // A wrong CRC, a missing chunk or a repeated chunk never replaces the file.
    std::vector<uint8_t> other(sample.rbegin(), sample.rend());
    Upload(t, card, "cubbi_a1.wav", other, Error::Checksum, 1);
    assert(card.files["cubbi_a1.wav"] == sample && !t.SampleWritten());
    assert(t.Begin(card, Decode(Begin("cubbi_a1.wav", 5000))) == Error::None);
    assert(t.Data(card, Decode(Data(0, other.data(), kFileChunk))) == Error::None);
    assert(t.Data(card, Decode(Data(0, other.data(), kFileChunk))) == Error::Patch && t.Offset() == kFileChunk);   // resend: rejected
    assert(t.Data(card, Decode(Data(2 * kFileChunk, other.data(), kFileChunk))) == Error::Patch);                // gap: rejected
    assert(t.End(card, Decode(Message(FileOp::End, Word(0)))) == Error::Length && card.files["cubbi_a1.wav"] == sample);
    assert(t.Data(card, Decode(Data(0, other.data(), 10))) == Error::Empty);                                       // after End
    // Write failure aborts; no card refuses.
    assert(t.Begin(card, Decode(Begin("jammi_a2.wav", 300))) == Error::None);
    card.fail_append = true; assert(t.Data(card, Decode(Data(0, other.data(), 100))) == Error::Storage && !t.Active());
    card.fail_append = false; card.ready = false;
    assert(t.Begin(card, Decode(Begin("jammi_a2.wav", 300))) == Error::Storage); card.ready = true;
    assert(!card.Exists("jammi_a2.wav"));
    // Firmware: needs FORGE.bin; other root images are set aside; then the panel press.
    assert(t.Install(card, gate, 0) == Error::Empty && !gate.Armed());
    card.files["CHOMPI_TAPEv2_0.bin"] = {1, 2, 3};
    Upload(t, card, "forge.bin", Firmware(5000, 1));
    assert(card.Exists("FORGE.bin") && !t.SampleWritten());
    assert(t.Flags(card, gate) == kFileFirmwareStaged);
    uint64_t keys = 1u << 5; gate.Filter(keys, 5); assert(keys == 1u << 5);   // idle: CHOMPI passes (and is held)
    assert(t.Install(card, gate, 1000) == Error::None && gate.Armed());
    assert(card.Exists("CHOMPI_TAPEv2_0_bin.old") && !card.Exists("CHOMPI_TAPEv2_0.bin") && card.Exists("cubbi_a1.wav"));
    assert(card.BootloaderSees() == std::vector<std::string>{"FORGE.bin"});
    assert(t.Flags(card, gate) == (kFileFirmwareStaged | kFileInstallPending));
    // CHOMPI held when armed does not count; it never reaches the panel while armed.
    keys = 1u << 5; gate.Filter(keys, 5); assert(keys == 0 && !gate.Confirmed());
    keys = 0; gate.Filter(keys, 5);
    keys = (1u << 5) | (1u << 15); gate.Filter(keys, 5); assert(keys == (1u << 15) && gate.Confirmed());
    assert(gate.Poll(2000) && t.Flags(card, gate) == (kFileFirmwareStaged | kFileRestarting));
    // Timeout disarms; the press that was in progress stays swallowed until released.
    InstallGate g2; g2.Arm(0); keys = 0; g2.Filter(keys, 5);
    assert(!g2.Poll(InstallGate::kTimeoutMs + 1) && !g2.Armed());
    keys = 1u << 5; g2.Filter(keys, 5); assert(keys == 1u << 5);       // disarmed: CHOMPI works normally again
    InstallGate g3; g3.Arm(0); keys = 0; g3.Filter(keys, 5); keys = 1u << 5; g3.Filter(keys, 5); g3.Cancel();
    keys = 1u << 5; g3.Filter(keys, 5); assert(keys == 0);             // still the confirming press
    keys = 0; g3.Filter(keys, 5); keys = 1u << 5; g3.Filter(keys, 5); assert(keys == 1u << 5);
    // Finishing a sample while the loader is busy: refused, nothing lost; then it completes.
    {
        FileTransfer u; std::vector<uint8_t> f(300, 9); uint8_t reply[32];
        auto begin = Begin("jammi_a4.wav", 300); assert(ServeFileRequest(begin.data(), begin.size(), u, card, gate, 0, reply) == 16);
        auto data = Data(0, f.data(), 224); ServeFileRequest(data.data(), data.size(), u, card, gate, 0, reply);
        data = Data(224, f.data(), 76); ServeFileRequest(data.data(), data.size(), u, card, gate, 0, reply);
        auto end = Message(FileOp::End, Word(Crc32(0, f.data(), f.size())));
        assert(ServeFileRequest(end.data(), end.size(), u, card, gate, 0, reply, true) == 9 && reply[7] == uint8_t(Error::StorageBusy));
        assert(u.Active() && !card.Exists("jammi_a4.wav"));
        assert(ServeFileRequest(end.data(), end.size(), u, card, gate, 0, reply, false) == 16 && card.Exists("jammi_a4.wav"));
    }
    // 0.11: low battery on a weak supply -> Install refused (Error::Power) before anything is set aside;
    // every reply carries the power flag so the host can stop before uploading.
    {
        FileTransfer p; InstallGate pg; uint8_t reply[32];
        auto install = Message(FileOp::Install, {}), status = Message(FileOp::Status, {});
        assert(ServeFileRequest(status.data(), status.size(), p, card, pg, 0, reply, false, false) == 16 && (reply[9] & kFilePowerLow));
        assert(ServeFileRequest(status.data(), status.size(), p, card, pg, 0, reply, false, true) == 16 && !(reply[9] & kFilePowerLow));
        assert(ServeFileRequest(install.data(), install.size(), p, card, pg, 0, reply, false, false) == 9
               && reply[7] == uint8_t(Error::Power) && !pg.Armed());
        assert(p.Install(card, pg, 0, true) == Error::None && pg.Armed());
        pg.Cancel();
    }
    // Install refuses while an upload is half done.
    assert(t.Begin(card, Decode(Begin("jammi_a3.wav", 100))) == Error::None);
    InstallGate g4; assert(t.Install(card, g4, 0) == Error::StorageBusy && !g4.Armed());
    t.Abort(card); assert(!t.Active() && !card.open);
}

// The bootloader's strstr(".bin") rule: what it would flash and how a file is set aside.
void BootloaderNames() {
    for(const char* hit : {"TAPE.bin", "x.BIN", "FORGE.bin.old", "song.binaural.wav", "a.bin.b"}) assert(BootloaderMatches(hit));
    for(const char* miss : {"TAPE.Bin", "TAPE_bin.old", "jammi_a1.wav", "bin", ".bi", "FORGE/UPLOAD.TMP"}) assert(!BootloaderMatches(miss));
    char out[32];
    auto aside = [&](const char* in, unsigned attempt) { assert(SetAsideName(in, attempt, out, sizeof out)); assert(!BootloaderMatches(out)); return std::string(out); };
    assert(aside("TAPE.bin", 0) == "TAPE_bin.old" && aside("x.BIN", 0) == "x_BIN.old");
    assert(aside("FORGE.bin.old", 0) == "FORGE_bin.old" && aside("song.binaural.wav", 0) == "song_binaural.wav");
    assert(aside("TAPE.bin", 3) == "3_TAPE_bin.old" && !SetAsideName("TAPE.bin", 10, out, sizeof out));
    assert(!SetAsideName("averyveryverylongname.bin", 0, out, 16) && !SetAsideName("", 0, out, sizeof out));
    // A card full of things the bootloader would pick before FORGE.bin; one name already taken.
    MemoryCard card; FileTransfer t; InstallGate gate;
    for(const char* n : {"CHOMPI_TAPEv2_0.bin", "FORGE.bin.old", "TEMPO.BIN", "notes.binary.txt", "TAPE.Bin", "jammi_a1.wav", "CHOMPI_TAPEv2_0_bin.old"})
        card.files[n] = {uint8_t(n[0])};
    Upload(t, card, "FORGE.bin", Firmware(2000, 2));
    assert(t.Install(card, gate, 0) == Error::None && gate.Armed());
    assert(card.BootloaderSees() == std::vector<std::string>{"FORGE.bin"});
    assert(card.files.at("1_CHOMPI_TAPEv2_0_bin.old") == std::vector<uint8_t>{'C'} && card.files.at("CHOMPI_TAPEv2_0_bin.old") == std::vector<uint8_t>{'C'});
    assert(card.Exists("FORGE_bin.old") && card.Exists("TEMPO_BIN.old") && card.Exists("notes_binary.txt") && card.Exists("TAPE.Bin") && card.Exists("jammi_a1.wav"));
    assert(card.files.size() == 8);                                     // nothing lost
    // A rename that fails refuses the install: the bootloader might still find another image.
    MemoryCard stuck; stuck.files["TAPE.bin"] = {1}; stuck.fail_rename = true; FileTransfer t2; InstallGate g2;
    Upload(t2, stuck, "FORGE.bin", Firmware(600, 3));
    assert(t2.Install(stuck, g2, 0) == Error::Storage && !g2.Armed());
}

// Firmware is checked the way the bootloader checks it, and read back before it replaces FORGE.bin.
void FirmwareGuards() {
    MemoryCard card; FileTransfer t; InstallGate gate;
    const auto good = Firmware(3000, 4);
    Upload(t, card, "FORGE.bin", good);
    std::vector<uint8_t> text(3000, 'A');                               // a renamed text file
    Upload(t, card, "FORGE.bin", text, Error::Patch);
    auto qspi = Firmware(3000, 5); qspi[7] = 0x90;                      // entry in QSPI (a stock BOOT_QSPI build)
    Upload(t, card, "FORGE.bin", qspi, Error::Patch);
    auto even = Firmware(3000, 6); even[4] &= 0xfe;                     // not a Thumb address
    Upload(t, card, "FORGE.bin", even, Error::Patch);
    auto outside = Firmware(3000, 7); outside[5] = 0x40;                // entry beyond the image's end
    Upload(t, card, "FORGE.bin", outside, Error::Patch);
    auto stack = Firmware(3000, 8); stack[3] = 0x90;                    // stack pointer in QSPI
    Upload(t, card, "FORGE.bin", stack, Error::Patch);
    assert(card.files.at("FORGE.bin") == good);
    card.corrupt_write = true;                                          // the card stores the bytes wrongly
    Upload(t, card, "FORGE.bin", Firmware(3000, 9), Error::Storage);
    assert(card.files.at("FORGE.bin") == good);
    card.corrupt_write = false;
    // Samples are not images: any content goes.
    Upload(t, card, "jammi_c2.wav", text);
    // The reply to End is lost: the host sends End again and gets the same answer.
    FileTransfer u; uint8_t reply[32];
    const auto image = Firmware(500, 10);
    auto send = [&](const std::vector<uint8_t>& m, uint32_t now = 0) { return ServeFileRequest(m.data(), m.size(), u, card, gate, now, reply); };
    auto begin = Begin("FORGE.bin", 500); assert(send(begin) == 16);
    for(uint32_t at = 0; at < 500; at += kFileChunk) { auto d = Data(at, image.data() + at, std::min<size_t>(kFileChunk, 500 - at)); assert(send(d) == 16); }
    auto end = Message(FileOp::End, Word(Crc32(0, image.data(), 500)));
    assert(send(end) == 16 && send(end) == 16 && card.files.at("FORGE.bin") == image);
    auto wrong = Message(FileOp::End, Word(Crc32(0, image.data(), 500) ^ 1));
    assert(send(wrong) == 9 && reply[7] == uint8_t(Error::Empty));       // a different file: nothing to end
    auto abort = Message(FileOp::Abort, {}); send(abort);
    assert(send(end) == 9);                                              // forgotten after an abort
    // A host that vanished mid-upload: Install waits kStaleTransferMs, then takes over.
    assert(send(Begin("jammi_a1.wav", 1000), 100) == 16);
    auto install = Message(FileOp::Install, {});
    assert(send(install, 100 + kStaleTransferMs - 1) == 9 && reply[7] == uint8_t(Error::StorageBusy) && u.Active());
    assert(send(install, 100 + kStaleTransferMs) == 16 && !u.Active() && gate.Armed());
}

// Random bytes, mutated requests and random operation orders: the decoder never reads out of
// bounds (run under ASan/UBSan) and the card only ever holds complete, CRC-matched uploads.
struct Rng { uint64_t s; uint32_t operator()() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return uint32_t(s); } };
void Fuzz(unsigned rounds) {
    Rng rng{0x5eed1234abcdull};
    MemoryCard card; FileTransfer t; InstallGate gate; uint8_t reply[32];
    std::map<std::string, std::vector<uint8_t>> sent;                   // what each name could legitimately hold
    const auto image = Firmware(2000, 11);
    sent["FORGE.bin"] = image;
    std::vector<uint8_t> sample(1500); for(auto& b : sample) b = uint8_t(rng());
    sent["cubbi_b2.wav"] = sample;
    std::vector<std::string> names; for(const auto& f : sent) names.push_back(f.first);
    size_t honest = 0; unsigned completed = 0, refused = 0;              // an honest host keeps uploading in between
    std::vector<std::vector<uint8_t>> valid;
    for(const auto& f : sent) {
        valid.push_back(Begin(f.first, uint32_t(f.second.size())));
        for(size_t at = 0; at < f.second.size(); at += kFileChunk)
            valid.push_back(Data(uint32_t(at), f.second.data() + at, std::min(kFileChunk, f.second.size() - at)));
        valid.push_back(Message(FileOp::End, Word(Crc32(0, f.second.data(), f.second.size()))));
    }
    valid.push_back(Message(FileOp::Status, {})); valid.push_back(Message(FileOp::Abort, {})); valid.push_back(Message(FileOp::Install, {}));
    std::vector<uint8_t> m;
    for(unsigned i = 0; i < rounds; ++i) {
        const unsigned kind = rng() % 8;
        bool ending = false;
        if(kind >= 5) {                                                  // the honest host's next step
            const auto& file = sent[names[honest]];
            if(!t.Active()) m = Begin(names[honest], uint32_t(file.size()));
            else if(t.Offset() < file.size()) m = Data(t.Offset(), file.data() + t.Offset(), std::min<size_t>(kFileChunk, file.size() - t.Offset()));
            else { m = Message(FileOp::End, Word(Crc32(0, file.data(), file.size()))); ending = true; }
        }
        else if(kind == 0) { m.resize(rng() % (kMaxSysEx + 1)); for(auto& b : m) b = uint8_t(rng() & (rng() % 4 ? 127 : 255)); }
        else {
            m = valid[rng() % valid.size()];
            if(kind == 1 && !m.empty()) m[rng() % m.size()] = uint8_t(rng() & 255);                    // any byte
            if(kind == 2 && !m.empty()) { m[rng() % m.size()] = uint8_t(rng() & 127); m.back() = 0; m.back() = Checksum(m.data(), m.size() - 1); }   // a valid-looking lie
            if(kind == 3) m.resize(rng() % (m.size() + 1));                                           // truncated
            if(kind == 4) { for(unsigned k = rng() % 20; k; --k) m.push_back(uint8_t(rng() & 127)); } // padded
        }
        const bool active = t.Active();
        const size_t n = ServeFileRequest(m.data(), m.size(), t, card, gate, i, reply, rng() % 5 == 0);
        assert(n == 9 || n == 16);
        if(ending && active && n == 16) { ++completed; honest = (honest + 1) % names.size(); }
        if(ending && n == 9) ++refused;
        assert(Checksum(reply, n) == 0);
        for(const auto& f : card.files) {
            auto it = sent.find(f.first);
            if(it != sent.end()) { assert(f.second == it->second); continue; }
            // A lie that kept the checksum valid can still never write a file whose CRC the host did not send
            // for that exact content: any other file must have come through Begin with its own name.
            assert(AllowedUploadName(f.first.c_str()));
        }
        if(card.Exists("FORGE.bin")) assert(PlausibleFirmware(card.files.at("FORGE.bin").data(), uint32_t(card.files.at("FORGE.bin").size())));
        if(gate.Poll(i)) gate.Cancel();
    }
    // The success paths really ran, both files landed, and the busy-loader refusal was seen.
    assert(completed > rounds / 400 && refused > 0 && card.files.at("FORGE.bin") == image && card.files.at("cubbi_b2.wav") == sample);
    std::cout << "fuzz: " << rounds << " requests, " << completed << " complete uploads, " << refused << " refused Ends\n";
}

// A host like forge_card.py (windows of 4 Data requests, resume from the Status offset) over a
// link that drops, duplicates and delays requests and replies. Every upload ends with the exact file.
void LossyLink() {
    Rng rng{42};
    for(unsigned trial = 0; trial < 40; ++trial) {
        MemoryCard card; FileTransfer t; InstallGate gate; uint8_t reply[32];
        std::vector<uint8_t> file = trial % 2 ? Firmware(3000 + trial * 97, uint8_t(trial)) : std::vector<uint8_t>(2000 + trial * 61);
        if(!(trial % 2)) for(auto& b : file) b = uint8_t(rng());
        const std::string name = trial % 2 ? "FORGE.bin" : "jammi_d7.wav";
        const unsigned loss = 5 + trial % 30;                           // percent lost each way
        std::vector<std::vector<uint8_t>> late;                          // requests delivered out of order later
        auto deliver = [&](const std::vector<uint8_t>& m, uint8_t* out) -> size_t {   // 0: no reply came back
            if(rng() % 100 < loss) return 0;
            if(rng() % 100 < 5) late.push_back(m);
            if(rng() % 100 < 5) ServeFileRequest(m.data(), m.size(), t, card, gate, 0, reply);   // duplicated
            if(!late.empty() && rng() % 100 < 10) { auto old = late.back(); late.pop_back(); ServeFileRequest(old.data(), old.size(), t, card, gate, 0, reply); }
            const size_t n = ServeFileRequest(m.data(), m.size(), t, card, gate, 0, out);
            return rng() % 100 < loss ? 0 : n;
        };
        auto reliable = [&](const std::vector<uint8_t>& m, uint8_t* out) {    // the host retries until it hears back
            for(unsigned tries = 0; tries < 200; ++tries) if(size_t n = deliver(m, out)) return n;
            assert(false); return size_t(0);
        };
        uint8_t got[32];
        bool done = false;
        for(unsigned attempt = 0; attempt < 50 && !done; ++attempt) {
            if(reliable(Begin(name, uint32_t(file.size())), got) != 16) continue;
            uint32_t offset = 0; unsigned failures = 0;
            while(offset < file.size() && failures < 20) {
                bool all = true; uint32_t next = offset;
                for(uint32_t at = offset; at < file.size() && at < offset + 4 * kFileChunk; at += kFileChunk) {
                    const size_t n = deliver(Data(at, file.data() + at, std::min<size_t>(kFileChunk, file.size() - at)), got);
                    if(n != 16) all = false;
                    else if(all) next = at + uint32_t(std::min<size_t>(kFileChunk, file.size() - at));
                }
                if(all) { offset = next; continue; }
                ++failures;
                if(reliable(Message(FileOp::Status, {}), got) == 16) offset = Read35(got + 10);
                if(!(got[9] & kFileActive)) break;                      // a resent Begin raced us: start again
            }
            if(offset < file.size()) continue;
            const auto end = Message(FileOp::End, Word(Crc32(0, file.data(), file.size())));
            const size_t n = reliable(end, got);
            done = n == 16;
        }
        assert(done && card.files.at(name == "FORGE.bin" ? "FORGE.bin" : name) == file);
    }
}

int main() {
    CodecAndNames(); TransferAndInstall(); BootloaderNames(); FirmwareGuards(); LossyLink(); Fuzz(200000);
    std::cout << "PASS: file transfer CRC/packing/names, staged replace, resend/gap/CRC rejection, bootloader-safe set-aside,"
                 " firmware header and read-back checks, repeated End, stale takeover, lossy link, 200k fuzzed requests, install gate, install power check\n";
}
