#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "midi_framer.h"
#include "protocol.h"
#include "sample_loader.h"

namespace forge {
// USB file transfer (opcode 0C, reply 48; docs/forge/PROTOCOL.md): the host
// writes FORGE.bin or a TAPE sample (jammi_/cubbi_<bank><slot>.wav) to the SD
// card root over MIDI, so the card never has to leave CHOMPI. Data goes to
// FORGE/UPLOAD.TMP and replaces the target only after the whole file's CRC-32
// matches. Installing firmware needs a CHOMPI key press on the panel, then a
// restart: the bootloader flashes the new FORGE.bin from the card.
// Main loop only, except InstallGate's audio-side Filter().
enum class FileOp : uint8_t { Begin, Data, End, Abort, Install, Status };
constexpr uint8_t kFileOpcode = 0x0c, kFileReply = 0x48;
constexpr size_t kFileChunk = 224;                               // raw bytes per Data request (32 groups of 7)
constexpr size_t kFilePacked = kFileChunk / 7 * 8;               // 256 SysEx bytes
constexpr size_t kFileRequest = 13 + kFilePacked + 1;            // 270: header, op, offset, data, checksum
constexpr size_t kFileNameMax = 24;
constexpr uint32_t kFirmwareMax = 480u * 1024u, kSampleFileMax = 64u * 1024u * 1024u;
constexpr uint8_t kFileActive = 1, kFileInstallPending = 2, kFileFirmwareStaged = 4, kFileRestarting = 8,   // reply flags
                  kFilePowerLow = 16;   // 0.11: an install would be refused now (power::InstallPowerOk)

static_assert(kMaxSysEx >= kFileRequest, "the MIDI framer must hold a whole file-transfer request");
// Chunks shorter than a sector always pass through FatFS's own sector buffer (in the
// FIL object, D1 SRAM): f_write never hands the SD card's DMA the request on the stack (DTCM).
static_assert(kFileChunk < 512, "Data chunks must stay below one SD sector");
constexpr uint32_t kAppStart = 0x24000000u;                      // BOOT_SRAM images run from D1 SRAM
constexpr uint32_t kStaleTransferMs = 5000;                      // Install takes over a transfer idle this long

inline uint32_t Crc32(uint32_t crc, const uint8_t* data, size_t size) {   // IEEE 802.3, as zlib.crc32
    crc = ~crc;
    for(size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for(int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
inline uint32_t Read35(const uint8_t* b) {                       // 32 bits in five 7-bit chunks, low first
    return uint32_t(b[0]) | uint32_t(b[1]) << 7 | uint32_t(b[2]) << 14 | uint32_t(b[3]) << 21 | uint32_t(b[4]) << 28;
}
inline void Write35(uint8_t* b, uint32_t v) { for(unsigned i = 0; i < 5; ++i) b[i] = (v >> (7 * i)) & 127; }
inline bool IsFirmwareName(const char* name) {
    static const char kName[] = "forge.bin";
    for(size_t i = 0; i < sizeof kName; ++i) {
        char c = name[i];
        if(c >= 'A' && c <= 'Z') c = char(c + 32);
        if(c != kName[i]) return false;
    }
    return true;
}
// The CHOMPI bootloader (v6.4 SearchBin) flashes the first visible root file whose name
// contains ".bin" or ".BIN" anywhere (strstr): FORGE.bin has to be the only such file.
inline bool BootloaderMatches(const char* name) {
    for(const char* c = name; c[0]; ++c)
        if(c[0] == '.' && ((c[1] == 'b' && c[2] == 'i' && c[3] == 'n') || (c[1] == 'B' && c[2] == 'I' && c[3] == 'N'))) return true;
    return false;
}
// The bootloader-invisible name for a file it would match: each ".bin"/".BIN" becomes
// "_bin"/"_BIN", and a name that ended in it gains ".old" (TAPE.bin -> TAPE_bin.old).
// attempt > 0 prefixes "<attempt>_" when that name is taken. False if it does not fit.
inline bool SetAsideName(const char* name, unsigned attempt, char* out, size_t capacity) {
    size_t n = 0;
    auto put = [&](char c) { if(n + 1 < capacity) out[n] = c; ++n; };
    if(attempt) { if(attempt > 9) return false; put(char('0' + attempt)); put('_'); }
    size_t length = 0; bool ends = false;
    for(const char* c = name; *c; ++c, ++length) {
        const bool dot = c[0] == '.' && ((c[1] == 'b' && c[2] == 'i' && c[3] == 'n') || (c[1] == 'B' && c[2] == 'I' && c[3] == 'N'));
        if(dot) ends = c[4] == 0;
        put(dot ? '_' : c[0]);
    }
    if(ends) for(const char* c = ".old"; *c; ++c) put(*c);
    if(n + 1 > capacity || !length) return false;
    out[n] = 0;
    return true;
}
// The bootloader's and check_firmware_layout.py's acceptance test for a BOOT_SRAM image:
// initial stack pointer in DTCM or D1 SRAM, Thumb entry point inside the image.
inline bool PlausibleFirmware(const uint8_t* head, uint32_t size) {
    const uint32_t stack = uint32_t(head[0]) | uint32_t(head[1]) << 8 | uint32_t(head[2]) << 16 | uint32_t(head[3]) << 24;
    const uint32_t entry = uint32_t(head[4]) | uint32_t(head[5]) << 8 | uint32_t(head[6]) << 16 | uint32_t(head[7]) << 24;
    const bool stack_ok = (stack >= 0x20000000u && stack <= 0x20020000u) || (stack >= 0x24000000u && stack <= 0x24080000u);
    return size >= 8 && size <= kFirmwareMax && stack_ok && (entry & 1u) && entry >= kAppStart && entry - kAppStart < size;
}
inline bool AllowedUploadName(const char* name) {
    uint8_t mode, bank, slot;
    return IsFirmwareName(name) || ParseSampleName(name, mode, bank, slot);
}

struct FileRequest {
    FileOp op = FileOp::Status;
    uint16_t sequence = 0;
    uint32_t size = 0, offset = 0, crc = 0;
    char name[kFileNameMax + 1]{};
    uint8_t data[kFileChunk]{};
    uint16_t length = 0;
};
// Validates a complete 0C request (F0/F7 excluded). Nothing changes on error.
FORGE_NOINLINE inline Error DecodeFileRequest(const uint8_t* bytes, size_t size, FileRequest& out) {
    if(!IsRequest(bytes, size) || bytes[4] != kFileOpcode || size < 9) return Error::Length;
    for(size_t i = 0; i < size; ++i) if(bytes[i] > 127) return Error::Patch;
    if(bytes[3] != kProtocolVersion) return Error::Version;
    if(Checksum(bytes, size) != 0) return Error::Checksum;
    FileRequest r;
    r.sequence = Read14(bytes + 5);
    if(bytes[7] > static_cast<uint8_t>(FileOp::Status)) return Error::Patch;
    r.op = static_cast<FileOp>(bytes[7]);
    switch(r.op) {
        case FileOp::Begin: {
            if(size < 15 || bytes[12] > 15) return Error::Length;
            const size_t length = bytes[13];
            if(length < 1 || length > kFileNameMax || size != 15 + length) return Error::Length;
            r.size = Read35(bytes + 8);
            for(size_t i = 0; i < length; ++i) {
                const uint8_t c = bytes[14 + i];
                if(c < 33 || c == '/' || c == '\\' || c == ':') return Error::Patch;
                r.name[i] = char(c);
            }
            if(!AllowedUploadName(r.name)) return Error::Patch;
            if(r.size == 0 || r.size > (IsFirmwareName(r.name) ? kFirmwareMax : kSampleFileMax)) return Error::Patch;
            break;
        }
        case FileOp::Data: {
            if(size < 15 || size > kFileRequest || bytes[12] > 15) return Error::Length;
            r.offset = Read35(bytes + 8);
            const size_t packed = size - 14;
            const size_t groups = (packed + 7) / 8;
            if(packed % 8 == 1) return Error::Length;               // a group needs its high-bit byte and one data byte
            size_t n = 0;
            for(size_t g = 0; g < groups; ++g) {
                const uint8_t* group = bytes + 13 + 8 * g;
                const size_t count = (g + 1 == groups && packed % 8) ? packed % 8 - 1 : 7;
                if(group[0] >> count) return Error::Patch;          // high bits only for bytes present
                for(size_t i = 0; i < count; ++i) r.data[n++] = uint8_t(group[1 + i] | ((group[0] >> i) & 1u) << 7);
            }
            r.length = static_cast<uint16_t>(n);
            break;
        }
        case FileOp::End:
            if(size != 14 || bytes[12] > 15) return Error::Length;
            r.crc = Read35(bytes + 8);
            break;
        default:
            if(size != 9) return Error::Length;
    }
    out = r;
    return Error::None;
}
inline size_t EncodeFileReply(uint16_t sequence, FileOp op, uint8_t flags, uint32_t offset, uint8_t* bytes) {
    Header(bytes, kFileReply, sequence);
    bytes[7] = 0; bytes[8] = static_cast<uint8_t>(op); bytes[9] = flags & 127;
    Write35(bytes + 10, offset);
    bytes[15] = Checksum(bytes, 15);
    return 16;
}

// Streaming file access on the card (main loop only).
class UploadFiles {
public:
    virtual ~UploadFiles() = default;
    virtual bool Ready() = 0;
    virtual bool Open(const char* temp) = 0;                                    // create or truncate
    virtual bool Append(const uint8_t* data, uint32_t size) = 0;
    // Sync and close; with verify, read the file back and require size and CRC-32; then replace.
    virtual bool Finish(const char* temp, const char* final_path, bool verify, uint32_t size, uint32_t crc) = 0;
    virtual void Abort(const char* temp) = 0;
    virtual bool Exists(const char* path) = 0;
    // Renames (SetAsideName) every visible root file the bootloader would match except
    // FORGE.bin; true only when FORGE.bin is then the only one left.
    virtual bool SetAsideOtherFirmware() = 0;
};

// Panel confirmation for installing firmware (audio owner calls Filter with the
// merged key mask; the main loop arms it and polls). While armed, the CHOMPI key
// belongs to the confirmation: it never reaches the menu or the record gesture.
class InstallGate {
public:
    static constexpr uint32_t kTimeoutMs = 15000;
    void Arm(uint32_t now) { armed_at_ = now; state_.store(kArmed, std::memory_order_release); }
    void Cancel() { state_.store(kIdle, std::memory_order_release); }
    bool Armed() const { return state_.load(std::memory_order_acquire) == kArmed; }
    bool Confirmed() const { return state_.load(std::memory_order_acquire) == kConfirmed; }
    void Filter(uint64_t& keys, uint8_t chompi_key) {
        const uint8_t state = state_.load(std::memory_order_acquire);
        const bool down = (keys >> chompi_key) & 1u;
        if(state == kArmed && down && !was_down_) state_.store(kConfirmed, std::memory_order_release);   // a new press
        swallow_ = state != kIdle ? down : (swallow_ && down);      // a press that began here ends here too
        if(state != kIdle || swallow_) keys &= ~(uint64_t(1) << chompi_key);
        was_down_ = down;
    }
    // Main loop: true once the operator confirmed; disarms after the timeout.
    bool Poll(uint32_t now) {
        if(Armed() && now - armed_at_ > kTimeoutMs) Cancel();
        return Confirmed();
    }
private:
    static constexpr uint8_t kIdle = 0, kArmed = 1, kConfirmed = 2;
    std::atomic<uint8_t> state_{kIdle};
    uint32_t armed_at_ = 0;
    bool was_down_ = false, swallow_ = false;
};

class FileTransfer {
public:
    static constexpr const char* kTemp = "FORGE/UPLOAD.TMP";
    bool Active() const { return active_; }
    uint32_t Offset() const { return offset_; }
    bool SampleWritten() { const bool w = sample_written_; sample_written_ = false; return w; }
    bool FinishingSample() const { return active_ && !IsFirmwareName(name_); }
    uint8_t Flags(UploadFiles& files, const InstallGate& gate, bool power_ok = true) {
        return (active_ ? kFileActive : 0) | (gate.Armed() ? kFileInstallPending : 0)
             | (files.Ready() && files.Exists("FORGE.bin") ? kFileFirmwareStaged : 0) | (gate.Confirmed() ? kFileRestarting : 0)
             | (power_ok ? 0 : kFilePowerLow);
    }
    void Touch(uint32_t now) { touched_ = now; }
    Error Begin(UploadFiles& files, const FileRequest& r) {
        if(!files.Ready()) return Error::Storage;
        if(active_) files.Abort(kTemp);
        active_ = done_ = false;
        if(!files.Open(kTemp)) return Error::Storage;
        std::memcpy(name_, r.name, sizeof name_);
        std::memset(head_, 0, sizeof head_);
        size_ = r.size; offset_ = 0; crc_ = 0; active_ = true;
        return Error::None;
    }
    Error Data(UploadFiles& files, const FileRequest& r) {
        if(!active_) return Error::Empty;
        if(r.offset != offset_ || offset_ + r.length > size_ || !r.length) return Error::Patch;   // the reply carries the offset to resume from
        if(!files.Append(r.data, r.length)) { Abort(files); return Error::Storage; }
        for(uint32_t i = offset_; i < sizeof head_ && i - offset_ < r.length; ++i) head_[i] = r.data[i - offset_];
        crc_ = Crc32(crc_, r.data, r.length); offset_ += r.length;
        return Error::None;
    }
    Error End(UploadFiles& files, const FileRequest& r) {
        // A repeated End (its reply was lost) for the file just written succeeds again.
        if(!active_) return done_ && r.crc == crc_ ? Error::None : Error::Empty;
        active_ = false;
        if(offset_ != size_ || crc_ != r.crc) { files.Abort(kTemp); return offset_ != size_ ? Error::Length : Error::Checksum; }
        const bool firmware = IsFirmwareName(name_);
        // Never stage an image the bootloader would reject or that cannot start.
        if(firmware && !PlausibleFirmware(head_, size_)) { files.Abort(kTemp); return Error::Patch; }
        // Firmware is read back from the card before it replaces FORGE.bin.
        if(!files.Finish(kTemp, firmware ? "FORGE.bin" : name_, firmware, size_, crc_)) return Error::Storage;
        if(!firmware) sample_written_ = true;
        done_ = true;
        return Error::None;
    }
    void Abort(UploadFiles& files) { if(active_) files.Abort(kTemp); active_ = done_ = false; }
    // Firmware on the card, nothing half-written and safe power (power::InstallPowerOk):
    // ask for the panel press. A transfer nobody has touched for kStaleTransferMs (a host
    // that went away) is abandoned.
    Error Install(UploadFiles& files, InstallGate& gate, uint32_t now, bool power_ok = true) {
        if(active_ && now - touched_ < kStaleTransferMs) return Error::StorageBusy;
        Abort(files);
        if(!files.Ready()) return Error::Storage;
        if(!files.Exists("FORGE.bin")) return Error::Empty;
        if(!power_ok) return Error::Power;
        if(!files.SetAsideOtherFirmware()) return Error::Storage;
        gate.Arm(now);
        return Error::None;
    }
private:
    char name_[kFileNameMax + 1]{};
    uint8_t head_[8]{};
    uint32_t size_ = 0, offset_ = 0, crc_ = 0, touched_ = 0;
    bool active_ = false, sample_written_ = false, done_ = false;
};
// One 0C request -> its reply (0x48, or 0x41 with the error). Shared by the
// firmware's main loop and the offline probe.
FORGE_NOINLINE inline size_t ServeFileRequest(const uint8_t* bytes, size_t size, FileTransfer& transfer, UploadFiles& files,
                                              InstallGate& gate, uint32_t now, uint8_t* reply, bool samples_busy = false,
                                              bool power_ok = true) {
    FileRequest r;
    Error error = DecodeFileRequest(bytes, size, r);
    // A sample may be open for loading: replacing it then would pull the file from under the
    // reader. Refuse the End (state kept); the host retries once the loader is idle.
    if(error == Error::None && r.op == FileOp::End && samples_busy && transfer.FinishingSample()) error = Error::StorageBusy;
    if(error == Error::None) switch(r.op) {
        case FileOp::Begin: error = transfer.Begin(files, r); break;
        case FileOp::Data: error = transfer.Data(files, r); break;
        case FileOp::End: error = transfer.End(files, r); break;
        case FileOp::Abort: transfer.Abort(files); break;
        case FileOp::Install: error = transfer.Install(files, gate, now, power_ok); break;
        case FileOp::Status: break;
    }
    if(error == Error::None && (r.op == FileOp::Begin || r.op == FileOp::Data)) transfer.Touch(now);
    if(error != Error::None) return EncodeError(size >= 7 ? Read14(bytes + 5) : 0, error, reply);
    return EncodeFileReply(r.sequence, r.op, transfer.Flags(files, gate, power_ok), transfer.Offset(), reply);
}
} // namespace forge
