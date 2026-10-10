#pragma once
#include <cstddef>
#include <cstdint>
#include "protocol.h"

namespace forge {
// Device presets: 8 banks x 15 slots, one small file per slot on the SD card:
// FORGE/B1S01.FPR .. FORGE/B8S15.FPR (8.3 names; never *.bin, which the
// bootloader would try to flash). Main-loop only: never call from audio.
//
// Record: 'F' 'P' format(1) data_len DATA[data_len] crc_hi crc_lo
// DATA is the wire patch DATA (DecodePatchData), CRC-16/CCITT over all bytes
// before it. A torn or foreign file fails the CRC/decoder and reads as empty.
class Storage {
public:
    virtual ~Storage() = default;
    virtual bool Ready() = 0;                                              // card mounted
    virtual bool Read(const char* path, uint8_t* buffer, size_t capacity, size_t& size) = 0;
    virtual bool Write(const char* path, const uint8_t* data, size_t size) = 0;   // replace whole file
    virtual bool Remove(const char* path) = 0;                             // true if gone afterwards
};

constexpr uint8_t kPresetFormat = 1;
constexpr size_t kMaxPresetRecord = 4 + (kMaxRequest - 8) + 2;

inline uint16_t Crc16(const uint8_t* data, size_t size) {
    uint16_t crc = 0xffff;
    for(size_t i = 0; i < size; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for(int bit = 0; bit < 8; ++bit) crc = crc & 0x8000 ? static_cast<uint16_t>((crc << 1) ^ 0x1021) : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}
// "FORGE/B1S01.FPR" for bank 0, slot 0 (1-based in the name, as on the panel).
inline void PresetPath(uint8_t bank, uint8_t slot, char (&path)[20]) {
    const char text[] = "FORGE/B0S00.FPR";
    for(size_t i = 0; i < sizeof(text); ++i) path[i] = text[i];
    path[7] = static_cast<char>('1' + bank);
    path[9] = static_cast<char>('0' + (slot + 1) / 10);
    path[10] = static_cast<char>('0' + (slot + 1) % 10);
}
inline size_t EncodePresetRecord(const Parameters& patch, uint8_t* out) {
    out[0] = 'F'; out[1] = 'P'; out[2] = kPresetFormat;
    const size_t size = EncodePatchData(patch, out + 4);
    out[3] = static_cast<uint8_t>(size);
    const uint16_t crc = Crc16(out, 4 + size);
    out[4 + size] = static_cast<uint8_t>(crc >> 8); out[5 + size] = static_cast<uint8_t>(crc);
    return 6 + size;
}
inline Error DecodePresetRecord(const uint8_t* data, size_t size, Parameters& out) {
    if(size < 6 || data[0] != 'F' || data[1] != 'P') return Error::Storage;
    if(data[2] != kPresetFormat) return Error::Version;
    if(size != 6u + data[3]) return Error::Storage;
    const uint16_t crc = Crc16(data, 4 + data[3]);
    if(data[4 + data[3]] != (crc >> 8) || data[5 + data[3]] != (crc & 0xff)) return Error::Storage;
    Parameters patch;
    const Error error = DecodePatchData(data + 4, data[3], patch);
    if(error != Error::None) return error;
    if(!patch.Valid()) return Error::Patch;
    out = patch;
    return Error::None;
}

class PresetStore {
public:
    explicit PresetStore(Storage& storage) : storage_(storage) {}
    // Scans all slots (call once after mounting and whenever the card changes).
    FORGE_COLD void Rescan() {
        for(uint8_t b = 0; b < kPresetBanks; ++b) {
            occupancy_[b] = 0;
            for(uint8_t s = 0; s < kPresetSlots; ++s) {
                Parameters ignored;
                if(Load(b, s, ignored) == Error::None) occupancy_[b] |= static_cast<uint16_t>(1u << s);
            }
        }
    }
    bool Ready() { return storage_.Ready(); }
    uint16_t Occupancy(uint8_t bank) const { return bank < kPresetBanks ? occupancy_[bank] : 0; }
    bool Occupied(uint8_t bank, uint8_t slot) const { return slot < kPresetSlots && (Occupancy(bank) >> slot) & 1u; }

    FORGE_COLD Error Save(uint8_t bank, uint8_t slot, const Parameters& patch) {
        if(bank >= kPresetBanks || slot >= kPresetSlots || !patch.Valid()) return Error::Patch;
        if(!storage_.Ready()) return Error::Storage;
        uint8_t record[kMaxPresetRecord];
        const size_t size = EncodePresetRecord(patch, record);
        char path[20]; PresetPath(bank, slot, path);
        if(!storage_.Write(path, record, size)) return Error::Storage;
        // Read back: a card that silently drops writes must not show as saved.
        Parameters check;
        if(Load(bank, slot, check) != Error::None) { occupancy_[bank] &= static_cast<uint16_t>(~(1u << slot)); return Error::Storage; }
        occupancy_[bank] |= static_cast<uint16_t>(1u << slot);
        return Error::None;
    }
    FORGE_COLD Error Load(uint8_t bank, uint8_t slot, Parameters& out) {
        if(bank >= kPresetBanks || slot >= kPresetSlots) return Error::Patch;
        if(!storage_.Ready()) return Error::Storage;
        uint8_t record[kMaxPresetRecord + 1];
        size_t size = 0;
        char path[20]; PresetPath(bank, slot, path);
        if(!storage_.Read(path, record, sizeof(record), size)) return Error::Empty;
        return DecodePresetRecord(record, size, out) == Error::None ? Error::None : Error::Empty;
    }
    FORGE_COLD Error Erase(uint8_t bank, uint8_t slot) {
        if(bank >= kPresetBanks || slot >= kPresetSlots) return Error::Patch;
        if(!storage_.Ready()) return Error::Storage;
        char path[20]; PresetPath(bank, slot, path);
        if(!storage_.Remove(path)) return Error::Storage;
        occupancy_[bank] &= static_cast<uint16_t>(~(1u << slot));
        return Error::None;
    }
    Error Copy(uint8_t from_bank, uint8_t from_slot, uint8_t to_bank, uint8_t to_slot) {
        Parameters patch;
        const Error error = Load(from_bank, from_slot, patch);
        return error != Error::None ? error : Save(to_bank, to_slot, patch);
    }
private:
    Storage& storage_;
    uint16_t occupancy_[kPresetBanks]{};
};
} // namespace forge
