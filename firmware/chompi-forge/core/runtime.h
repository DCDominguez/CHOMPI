#pragma once
#include "engine.h"
#include "midi_framer.h"
#include "preset_store.h"
#include "protocol.h"

static_assert(forge::kMaxSysEx >= forge::kMaxRequest, "the MIDI framer must hold the largest apply request");

namespace forge {
// Audio-owner operation shared with the offline integration harness.
// True means a reply is required. CPU stats are supplied by the caller.
FORGE_COLD inline bool ExecuteRequest(const Request& request, Engine& engine, Response& response) {
    if(request.kind == RequestKind::Parameter) {
        engine.Apply(request.command); return false;
    }
    if(request.kind == RequestKind::Note) {
        engine.Note(request.note, request.velocity, request.source); return false;
    }
    if(request.kind == RequestKind::Pedal) { engine.Pedal(request.source, request.value != 0); return false; }
    if(request.kind == RequestKind::Bend) { engine.Bend(request.source, request.value); return false; }
    if(request.kind == RequestKind::ResetControllers) { engine.ResetControllers(request.source); return false; }
    if(request.kind == RequestKind::ModWheel) { engine.ModWheel(static_cast<uint8_t>(request.value)); return false; }
    if(request.kind == RequestKind::Looper) { engine.LooperControl(request.note, static_cast<uint8_t>(request.value)); return false; }
    if(request.kind == RequestKind::Patch && request.silent) {   // on-device recall: no reply
        engine.ApplyPatch(request.patch, request.recall ? SlotPolicy::Recall : SlotPolicy::Patch); return false;
    }
    response = Response{};
    response.sequence = request.sequence; response.source = request.source;
    if(request.kind == RequestKind::Store) {     // snapshot for the main loop to write
        response.kind = ResponseKind::Snapshot; response.bank = request.bank; response.slot = request.slot;
    } else if(request.kind == RequestKind::Patch) {
        if(!engine.ApplyPatch(request.patch, request.recall ? SlotPolicy::Recall : SlotPolicy::Patch)) response.error = Error::Patch;
    } else if(request.kind == RequestKind::Panic) engine.Panic();
    else if(request.kind != RequestKind::Status) response.error = Error::Opcode;
    response.patch = engine.GetParameters();
    return true;
}
inline bool IsPerformance(RequestKind kind) {
    return kind == RequestKind::Note || kind == RequestKind::Pedal || kind == RequestKind::Bend
        || kind == RequestKind::ResetControllers || kind == RequestKind::ModWheel;
}
// What the main loop does with a decoded channel message (firmware and tests).
enum class Ingress : uint8_t {
    Ignore,     // other channel or unsupported message
    Emergency,  // CC120/123: global silence, nothing queued
    Critical,   // note/pedal/reset: losing it could leave sound stuck, so a
                // full queue raises an emergency
    Control,    // CC parameter, bend or mod wheel: a full queue only counts a drop
    Storage,    // program change -> recall a device preset (main loop, SD)
};
// `channel`: the MIDI input channel, 0-15 (options.json "Midi In Channel"; default 1).
inline Ingress TranslateChannel(const MidiFrame& frame, uint8_t source, Request& request, uint8_t channel = 0) {
    request = Request{}; request.source = source;
    if(frame.kind == MidiFrame::Kind::SysEx || frame.data[0] != channel) return Ingress::Ignore;
    const uint8_t a = frame.data[1], b = frame.data[2];
    switch(frame.kind) {
        case MidiFrame::Kind::NoteOn: case MidiFrame::Kind::NoteOff:
            request.kind = RequestKind::Note; request.note = a;
            request.velocity = frame.kind == MidiFrame::Kind::NoteOff ? 0 : b;
            return Ingress::Critical;
        case MidiFrame::Kind::PitchBend:
            request.kind = RequestKind::Bend; request.value = static_cast<uint16_t>(a | (b << 7));
            return Ingress::Control;
        case MidiFrame::Kind::CC:
            if(a == 120 || a == 123) return Ingress::Emergency;
            if(a == 64) { request.kind = RequestKind::Pedal; request.value = b >= 64; return Ingress::Critical; }
            if(a == 121) { request.kind = RequestKind::ResetControllers; return Ingress::Critical; }
            if(a == 1) { request.kind = RequestKind::ModWheel; request.value = b; return Ingress::Control; }
            // CC 26 PLAY / 27 LOOP (TAPE) and CC 24 (SW5: looper transport or cutoff), decided by the engine.
            if(a == 24 || a == 26 || a == 27) {
                request.kind = RequestKind::Looper; request.note = a == 24 ? 2 : static_cast<uint8_t>(a - 26); request.value = b;
                return Ingress::Control;
            }
            if(!DecodeCC(0, a, b, request.command)) return Ingress::Ignore;
            request.kind = RequestKind::Parameter;
            return Ingress::Control;
        case MidiFrame::Kind::ProgramChange:
            // Program 0..119 = bank * 15 + slot; recalled silently from the SD card.
            if(a >= kPresetBanks * kPresetSlots) return Ingress::Ignore;
            request.kind = RequestKind::Recall; request.bank = a / kPresetSlots; request.slot = a % kPresetSlots;
            request.silent = true;
            return Ingress::Storage;
        default: return Ingress::Ignore;
    }
}
// Main-loop side of device-preset requests (firmware and offline harness).
// Store: `snapshot` is the audio owner's current patch (a Snapshot response).
inline Response StoreReply(PresetStore& store, uint16_t sequence, uint8_t source, uint8_t bank, uint8_t slot,
                           const Parameters& snapshot) {
    Response r; r.sequence = sequence; r.source = source; r.bank = bank; r.slot = slot;
    r.kind = ResponseKind::Stored; r.error = store.Save(bank, slot, snapshot);
    return r;
}
inline Response EraseReply(PresetStore& store, const Request& request) {
    Response r; r.sequence = request.sequence; r.source = request.source; r.bank = request.bank; r.slot = request.slot;
    r.kind = ResponseKind::Erased; r.error = store.Erase(request.bank, request.slot);
    return r;
}
inline Response ListReply(PresetStore& store, const Request& request) {
    Response r; r.sequence = request.sequence; r.source = request.source; r.kind = ResponseKind::Occupancy;
    if(!store.Ready()) r.error = Error::Storage;
    for(uint8_t b = 0; b < kPresetBanks; ++b) r.occupancy[b] = store.Occupancy(b);
    return r;
}
// Recall: on success `apply` is a Patch request (silent if the original was)
// to queue for the audio owner, which replies with status; otherwise the error
// to report.
inline Error RecallRequest(PresetStore& store, const Request& request, Request& apply) {
    Parameters patch;
    const Error error = store.Load(request.bank, request.slot, patch);
    if(error != Error::None) return error;
    apply = Request{}; apply.kind = RequestKind::Patch; apply.patch = patch;
    apply.sequence = request.sequence; apply.source = request.source; apply.silent = request.silent; apply.recall = true;
    return Error::None;
}
// Stuck-note recovery used by the audio callback (and host tests). The main
// loop counts emergencies (lost note data, CC120/123) and stamps every queued
// request with the count. A newer count silences the engine once; notes queued
// before it are discarded, so a stale note-on can never sound without its
// note-off, while notes queued after it play at once even if the queue never
// drains. Patches, status, panic and controls always execute and reply.
// Counts are compared modulo 256; the audio owner catches up every block.
class RecoveryGate {
public:
    // Call once per block with the main loop's current count.
    void Observe(uint8_t epoch, Engine& engine) {
        if(static_cast<int8_t>(static_cast<uint8_t>(epoch - epoch_)) > 0) { engine.Panic(); epoch_ = epoch; }
    }
    // False means drop the request.
    bool Admit(const Request& request, Engine& engine) {
        const int8_t age = static_cast<int8_t>(static_cast<uint8_t>(request.epoch - epoch_));
        if(age > 0) { engine.Panic(); epoch_ = request.epoch; }
        return !(age < 0 && IsPerformance(request.kind));
    }
    uint8_t Epoch() const { return epoch_; }
private:
    uint8_t epoch_ = 0;
};
} // namespace forge
