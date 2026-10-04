#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include "../core/file_transfer.h"

using namespace forge;

// In-memory card: files by name, one temp file being written.
struct MemoryCard : UploadFiles {
    std::map<std::string, std::vector<uint8_t>> files;
    std::vector<uint8_t> temp; bool open = false, ready = true, fail_append = false;
    bool Ready() override { return ready; }
    bool Open(const char*) override { if(!ready) return false; temp.clear(); open = true; return true; }
    bool Append(const uint8_t* d, uint32_t n) override { if(!open || fail_append) return false; temp.insert(temp.end(), d, d + n); return true; }
    bool Finish(const char*, const char* final_path) override { if(!open) return false; open = false; files[final_path] = temp; return true; }
    void Abort(const char*) override { open = false; temp.clear(); }
    bool Exists(const char* path) override { return files.count(path) > 0; }
    bool SetAsideOtherFirmware(const char* keep) override {
        std::map<std::string, std::vector<uint8_t>> next;
        for(auto& f : files) {
            const std::string& n = f.first;
            const bool bin = n.size() > 4 && (n.substr(n.size() - 4) == ".bin" || n.substr(n.size() - 4) == ".BIN");
            next[bin && n != keep ? n + ".old" : n] = f.second;
        }
        files = next; return true;
    }
};

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
    Upload(t, card, "forge.bin", sample);
    assert(card.Exists("FORGE.bin") && !t.SampleWritten());
    assert(t.Flags(card, gate) == kFileFirmwareStaged);
    uint64_t keys = 1u << 5; gate.Filter(keys, 5); assert(keys == 1u << 5);   // idle: CHOMPI passes (and is held)
    assert(t.Install(card, gate, 1000) == Error::None && gate.Armed());
    assert(card.Exists("CHOMPI_TAPEv2_0.bin.old") && !card.Exists("CHOMPI_TAPEv2_0.bin") && card.Exists("cubbi_a1.wav"));
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
    // Install refuses while an upload is half done.
    assert(t.Begin(card, Decode(Begin("jammi_a3.wav", 100))) == Error::None);
    InstallGate g4; assert(t.Install(card, g4, 0) == Error::StorageBusy && !g4.Armed());
    t.Abort(card); assert(!t.Active() && !card.open);
}

int main() {
    CodecAndNames(); TransferAndInstall();
    std::cout << "PASS: file transfer CRC/packing/names, staged replace, resend/gap/CRC rejection, firmware set-aside, install confirmation gate\n";
}
