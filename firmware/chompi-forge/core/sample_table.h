#pragma once
#include <atomic>
#include <cstdint>
#include "parameters.h"

namespace forge {
// Samples the audio owner may read. Slots 0-13 hold the loaded chromatic
// sample or kit bank; slot 14 is the recording. The writer (main loop for
// files, audio owner for the recording) fills `data` first and then publishes
// `loaded` with release ordering; readers load it with acquire ordering and
// never read a frame at or beyond it. data/frames/channels/rate_ratio/gain are
// only changed while no voice reads the slot (see SampleHandoff).
struct SampleSlot {
    const int16_t* data = nullptr;   // interleaved, `channels` per frame
    uint32_t frames = 0;             // frames the slot will hold when complete
    std::atomic<uint32_t> loaded{0}; // frames readable now
    uint8_t channels = 0;            // 0 = empty, 1 = mono, 2 = stereo
    bool partial = false;            // file longer than the space it was given
    float rate_ratio = 1.f;          // file rate / output rate
    float gain = 1.f;                // playback gain (recording normalisation)
    uint32_t Readable() const { const uint32_t n = loaded.load(std::memory_order_acquire); return n < frames ? n : frames; }
};
struct SampleTable {
    SampleSlot slots[kSampleSlots];
};
} // namespace forge
