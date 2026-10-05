#pragma once
#include "preset_store.h"
#include "sequencer.h"

// Projects (0.15): a device preset plus its recorded loop, FORGE/BbSss.FSQ beside
// FORGE/BbSss.FPR. Main loop only (SD card); the audio owner hands the loop over in the
// Mailbox. A slot without a loop keeps whatever loop is playing when recalled.
namespace forge {
namespace seq {
// After a preset save of bank/slot: write the exported loop (or remove an old one when the
// loop is empty). `write` false (the preset save failed) only frees the mailbox.
FORGE_COLD inline Error SaveFile(Storage& storage, Mailbox& box, uint8_t bank, uint8_t slot, uint8_t* buffer, bool write = true) {
    if(box.state.load(std::memory_order_acquire) != Mailbox::Exported) return Error::Busy;   // saved without its loop
    Error error = Error::None;
    if(write && box.bank == bank && box.slot == slot) {
        char path[20]; FilePath(bank, slot, path);
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
    if(!storage.Read(path, buffer, kFileMax, size) || !box.Claim()) return false;
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
    if(!storage.Read(from, buffer, kFileMax, size)) return storage.Remove(to);
    return storage.Write(to, buffer, size);
}
} // namespace seq
} // namespace forge
