#pragma once
#include "preset_store.h"
#include "sequencer.h"

// Projects (0.15): a device preset plus its recorded loop, FORGE/BbSss.FSQ beside
// FORGE/BbSss.FPR. Main loop only (SD card); the audio owner hands the loop over in the
// Mailbox. A slot without a loop keeps whatever loop is playing when recalled.
namespace forge {
namespace seq {
// After a preset save of bank/slot by `owner`: write its exported loop (or remove an old
// one when the loop is empty). `write` false (the preset save failed) only frees the
// mailbox. No export of its own (the mailbox was busy): the preset went without its loop,
// so an older loop of that slot is removed (0.15.1) unless another save of the same slot
// still holds its export (that save writes it).
FORGE_COLD inline Error SaveFile(Storage& storage, Mailbox& box, uint8_t bank, uint8_t slot, uint8_t* buffer, bool write = true,
                                 uint8_t owner = Mailbox::Panel) {
    char path[20]; FilePath(bank, slot, path);
    const bool exported = box.state.load(std::memory_order_acquire) == Mailbox::Exported;
    const bool same_slot = exported && box.bank == bank && box.slot == slot;
    uint8_t expected = Mailbox::Exported;
    if(!same_slot || box.owner != owner || !box.state.compare_exchange_strong(expected, Mailbox::Main, std::memory_order_acquire)) {
        if(write && !same_slot) storage.Remove(path);
        return Error::Busy;
    }
    Error error = Error::None;
    if(write) {
        if(!box.sequence.count) { if(!storage.Remove(path)) error = Error::Storage; }
        else if(!storage.Write(path, buffer, EncodeFile(box.sequence, buffer))) error = Error::Storage;
    }
    box.Done();
    return error;
}
// A recall of bank/slot: true when its loop is in the mailbox for the audio owner
// (queue RequestKind::SequenceLoad). No file, a bad file or a busy mailbox: false.
FORGE_COLD inline bool LoadFile(Storage& storage, Mailbox& box, uint8_t bank, uint8_t slot, uint8_t* buffer) {
    char path[20]; FilePath(bank, slot, path);
    size_t size = 0;
    if(!storage.Read(path, buffer, kFileBuffer, size) || !box.Claim()) return false;
    if(!DecodeFile(buffer, size, box.sequence)) { box.Done(); return false; }
    box.Loaded();
    return true;
}
FORGE_COLD inline bool EraseFile(Storage& storage, uint8_t bank, uint8_t slot) {
    char path[20]; FilePath(bank, slot, path);
    return storage.Remove(path);
}
// A preset copy takes its loop along (or leaves none at the destination).
FORGE_COLD inline bool CopyFile(Storage& storage, uint8_t bank, uint8_t slot, uint8_t to_bank, uint8_t to_slot, uint8_t* buffer) {
    char from[20], to[20]; FilePath(bank, slot, from); FilePath(to_bank, to_slot, to);
    size_t size = 0;
    if(!storage.Read(from, buffer, kFileBuffer, size)) return storage.Remove(to);
    return storage.Write(to, buffer, size);
}
} // namespace seq
} // namespace forge
