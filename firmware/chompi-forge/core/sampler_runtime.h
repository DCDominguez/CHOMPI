#pragma once
#include "protocol.h"
#include "recorder.h"
#include "sample_loader.h"
#include "slot_settings.h"

namespace forge {
// Sampler plumbing shared by the firmware and the offline harness (forge_probe).

// TAPE: choosing a slot (or finishing a recording) makes the instrument play
// it. A patch that was not already a sampler patch becomes one with an open
// filter, full sustain and a short attack, so the sample sounds as recorded.
inline Parameters SelectSample(Parameters p, uint8_t mode, uint8_t bank, uint8_t slot) {
    if(!p.Sampler()) {
        if(p.version < 4) p.version = 4;
        p.synth = true; p.source = 1;
        p.cutoff = 1.f; p.resonance = 0.f; p.filter_amount = 0.5f;
        p.attack = 0.f; p.decay = 0.1f; p.sustain = 1.f; p.release = 0.02f;
        p.lfo_pitch = p.lfo_filter = p.lfo_amp = 0.f; p.glide = 0.f;
    }
    p.sample_mode = mode < 2 ? mode : 0;
    p.sample_bank = bank < kSampleBanks ? bank : 0;
    if(p.sample_mode == 0 && slot < kSampleSlots) p.sample_slot = slot;
    return p;
}

// Main loop, after a finished sample job: TAPE's menu moves the per-slot settings with
// the file (Save: the recording's settings to the slot; Copy: the source's; Erase: none).
// A loop saved from the looper has no settings of its own: the slot's are cleared.
inline void FollowSampleJob(SlotSettings& settings, const SampleEvent& event) {
    if(!event.ok) return;
    const SampleJob& j = event.job;
    if(j.kind == SampleJob::Kind::Save) {
        if(j.from_loop) settings.Invalidate(j.mode, j.bank, j.slot);
        else settings.Copy(0, j.bank, kRamSlot, j.mode, j.bank, j.slot);
    } else if(j.kind == SampleJob::Kind::Copy) settings.Copy(j.mode, j.bank, j.slot, j.to_mode, j.to_bank, j.to_slot);
    else settings.Invalidate(j.mode, j.bank, j.slot);
}

// Audio owner: a host "save recording" locks the take so it cannot be
// overwritten while the main loop writes it. The response carries its length
// and gain (SampleSnapshot), or Busy (recording now) / Empty (no take).
inline Response LockForSave(Recorder& recorder, const Request& request) {
    Response r; r.sequence = request.sequence; r.source = request.source; r.action = SampleAction::Save;
    r.mode = request.mode; r.bank = request.bank; r.slot = request.slot;
    if(recorder.Recording()) { r.error = Error::Busy; return r; }
    if(!recorder.Lock()) { r.error = Error::Empty; return r; }
    r.kind = ResponseKind::SampleSnapshot; r.frames = recorder.Length(); r.gain = recorder.Gain();
    return r;
}
inline SampleJob SaveJob(const Response& snapshot) {
    SampleJob job; job.kind = SampleJob::Kind::Save; job.mode = snapshot.mode; job.bank = snapshot.bank; job.slot = snapshot.slot;
    job.frames = snapshot.frames; job.gain = snapshot.gain; job.sequence = snapshot.sequence; job.source = snapshot.source;
    return job;
}
// Erase/copy requests become loader jobs directly (main loop). Copying from
// an empty slot is refused up front.
inline Error FileJob(const SampleLoader& loader, const Request& request, SampleJob& job) {
    job = SampleJob{};
    job.kind = request.action == SampleAction::Copy ? SampleJob::Kind::Copy : SampleJob::Kind::Erase;
    job.mode = request.mode; job.bank = request.bank; job.slot = request.slot;
    job.to_mode = request.to_mode; job.to_bank = request.to_bank; job.to_slot = request.to_slot;
    job.sequence = request.sequence; job.source = request.source;
    if(!loader.Scanned()) return Error::Storage;
    if(job.kind == SampleJob::Kind::Copy && !((loader.Occupancy(job.mode, job.bank) >> job.slot) & 1u)) return Error::Empty;
    return Error::None;
}
inline Response SampleDoneReply(const SampleEvent& event) {
    Response r; r.sequence = event.job.sequence; r.source = event.job.source; r.kind = ResponseKind::SampleDone;
    const bool copy = event.job.kind == SampleJob::Kind::Copy;
    r.action = event.job.kind == SampleJob::Kind::Save ? SampleAction::Save : copy ? SampleAction::Copy : SampleAction::Erase;
    r.mode = copy ? event.job.to_mode : event.job.mode;
    r.bank = copy ? event.job.to_bank : event.job.bank;
    r.slot = copy ? event.job.to_slot : event.job.slot;
    if(!event.ok) r.error = Error::Storage;
    return r;
}
// Opcode 08 reply: occupancy of every bank, card/busy flags, recording length.
inline Response SampleListReply(const SampleLoader& loader, bool card_ready, const SampleSlot& recording,
                                uint32_t capacity_frames, const Request& request) {
    Response r; r.sequence = request.sequence; r.source = request.source; r.kind = ResponseKind::SampleOccupancy;
    for(uint8_t m = 0; m < 2; ++m) for(uint8_t b = 0; b < kSampleBanks; ++b) r.samples[m][b] = loader.Occupancy(m, b);
    // Only the atomic count: frames/channels of the recording belong to the audio owner.
    const uint32_t frames = recording.loaded.load(std::memory_order_acquire);
    r.flags = static_cast<uint8_t>((card_ready && loader.Scanned() ? kSampleCardReady : 0)
                                   | (frames ? kSampleRecording : 0) | (loader.Busy() ? kSampleBusy : 0));
    r.record_ms = static_cast<uint32_t>(uint64_t(frames) * 1000 / 48000);
    r.capacity_ms = static_cast<uint32_t>(uint64_t(capacity_frames) * 1000 / 48000);
    return r;
}
} // namespace forge
