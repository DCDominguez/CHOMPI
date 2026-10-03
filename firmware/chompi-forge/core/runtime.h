#pragma once
#include "engine.h"
#include "midi_framer.h"
#include "protocol.h"

namespace forge {
// Audio-owner operation shared with the offline integration harness.
// True means a reply is required. CPU stats are supplied by the caller.
inline bool ExecuteRequest(const Request& request, Engine& engine, Response& response) {
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
    response = Response{};
    response.sequence = request.sequence; response.source = request.source;
    if(request.kind == RequestKind::Patch) {
        if(!engine.ApplyPatch(request.patch)) response.error = Error::Patch;
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
};
inline Ingress TranslateChannel(const MidiFrame& frame, uint8_t source, Request& request) {
    request = Request{}; request.source = source;
    if(frame.kind == MidiFrame::Kind::SysEx || frame.data[0] != 0) return Ingress::Ignore;
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
            if(!DecodeCC(0, a, b, request.command)) return Ingress::Ignore;
            request.kind = RequestKind::Parameter;
            return Ingress::Control;
        default: return Ingress::Ignore;
    }
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
