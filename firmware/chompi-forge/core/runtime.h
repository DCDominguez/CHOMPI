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
} // namespace forge
