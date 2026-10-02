#pragma once
#include "engine.h"
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
    response = Response{};
    response.sequence = request.sequence; response.source = request.source;
    if(request.kind == RequestKind::Patch) {
        if(!engine.ApplyPatch(request.patch)) response.error = Error::Patch;
    } else if(request.kind == RequestKind::Panic) engine.Panic();
    else if(request.kind != RequestKind::Status) response.error = Error::Opcode;
    response.patch = engine.GetParameters();
    return true;
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
        return !(age < 0 && request.kind == RequestKind::Note);
    }
    uint8_t Epoch() const { return epoch_; }
private:
    uint8_t epoch_ = 0;
};
} // namespace forge
