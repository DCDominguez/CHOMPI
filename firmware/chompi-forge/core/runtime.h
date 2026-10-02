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
// Stuck-note recovery used by the audio callback (and host tests). After an
// emergency (lost note data, CC120/123), the engine is silenced and note events
// already queued are discarded until the request queue has drained once, so a
// stale note-on can never sound without its note-off. Patches, status and panic
// requests still execute and reply. Notes received after the drain play normally.
class RecoveryGate {
public:
    void Begin(Engine& engine) { engine.Panic(); recovering_ = true; }
    bool Skip(const Request& request) const { return recovering_ && request.kind == RequestKind::Note; }
    void EndIfDrained(bool queue_empty) { if(recovering_ && queue_empty) recovering_ = false; }
    bool Recovering() const { return recovering_; }
private:
    bool recovering_ = false;
};
} // namespace forge
