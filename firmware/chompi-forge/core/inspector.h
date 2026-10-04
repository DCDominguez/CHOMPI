#pragma once
// One development telemetry model. No wire formatting or IO in the audio owner.
#ifdef FORGE_TEST_HOOKS
#include "command_queue.h"
#include "protocol.h"

namespace forge {
constexpr uint8_t kInspectorSchema = 1;
struct InspectorVoice {
    uint8_t note = 0, source = 0, stage = 0, slot = 127, flags = 0;
    float envelope = 0;
    uint32_t age = 0;
};
struct InspectorAudio {
    Parameters patch;
    uint64_t physical_keys = 0, logical_keys = 0;
    uint32_t raw_turns[6]{}, turns[6]{}; // wrapping signed encoder accumulators
    uint32_t block = 0, time_ms = 0, menu = 0, record_frames = 0;
    uint8_t physical_flags = 0, logical_flags = 0, record_source = 0, knob_pages = 0;
    bool recording = false, locked = false;
    float cpu_average = 0, cpu_peak = 0, cutoff = 0, lfo = 0, wheel = 0;
    float mix = 0, feedback = 0, level = 0, delay_samples = 0, reverb_mix = 0;
    float bend[3]{1,1,1}; uint8_t pedals = 0;
    InspectorVoice voices[7];
    // Looper: bits 0-2 state, 3 overdub, 4 effects before the loop, 5 locked (saving).
    uint8_t loop_flags = 0; uint32_t loop_length = 0;
    float loop_position = 0, loop_speed = 1, loop_feedback = 1;
};
// Main requests a refresh at <=20 Hz. Audio writes cheap scalar state into
// this exclusive slot, then publishes; main makes the actual Inspector snapshot.
// Ownership, not a seqlock over non-atomic memory: no data races or retry loops.
class InspectorMailbox {
public:
    InspectorAudio* AudioBegin() {
        unsigned expected = 1;
        return owner_.compare_exchange_strong(expected, 2, std::memory_order_acquire) ? &state_ : nullptr;
    }
    void AudioEnd() { owner_.store(3, std::memory_order_release); }
    bool Read(InspectorAudio& out) {
        if(owner_.load(std::memory_order_acquire) != 3) return false;
        out = state_;
        owner_.store(0, std::memory_order_release);
        return true;
    }
    void RequestRefresh() { unsigned expected = 0; owner_.compare_exchange_strong(expected, 1, std::memory_order_release); }
private:
    InspectorAudio state_;
    std::atomic<unsigned> owner_{0}; // idle/requested/audio writing/main ready
};
enum class InspectorEventKind : uint8_t {
    KeyDown = 1, KeyUp, Knob, VoiceStart, VoiceStop, PatchApply,
    SampleSelection, RecordingStart, RecordingStop, Card, QueueError, StorageError, SampleLoaded, SampleJobDone
};
struct InspectorEvent {
    uint32_t serial = 0, time_ms = 0, value = 0;
    InspectorEventKind kind = InspectorEventKind::QueueError;
    uint8_t id = 0;
};
// Main-loop-only retained log; readers use a cursor and never consume events.
class InspectorLog {
public:
    static constexpr unsigned kCapacity = 64;
    void Add(InspectorEvent e) { e.serial = ++latest_; events_[(latest_ - 1) % kCapacity] = e; if(count_ < kCapacity) ++count_; else ++overwritten_; }
    unsigned After(uint32_t cursor, InspectorEvent (&out)[3]) const {
        unsigned n = 0;
        for(unsigned i = count_; i && n < 3; --i) {
            const auto& e = events_[(latest_ - i) % kCapacity];
            const uint32_t delta = e.serial - cursor;
            if(delta && delta < 0x80000000u) out[n++] = e;
        }
        return n;
    }
    uint32_t Latest() const { return latest_; }
    uint32_t Overwritten() const { return overwritten_; }
private:
    InspectorEvent events_[kCapacity]; uint32_t latest_ = 0, overwritten_ = 0; unsigned count_ = 0;
};
struct InspectorSystem {
    uint32_t uptime_ms = 0, rx[2]{}, tx[2]{}, tx_errors[2]{}, ingress_drops[2]{};
    uint32_t dropped = 0, rejected = 0, panel_drops = 0, sample_drops = 0, event_drops = 0, emergencies = 0;
    bool simulated = false;
};
struct InspectorStorage {
    bool present = false, mounted = false, busy = false, loading = false;
    uint32_t loaded_selection = 0, file_loaded_frames = 0, file_frames = 0;
    uint32_t pool_used_bytes = 0, pool_capacity_bytes = 0, record_capacity_frames = 0;
    uint32_t errors = 0;
    uint8_t pending = 0, job = 127, last_error = 0, partial_slots = 0;
};
struct InspectorSnapshot {
    uint32_t generation = 0;
    InspectorAudio audio;
    InspectorSystem system;
    InspectorStorage storage;
};
// Full unsigned 32-bit values in five 7-bit chunks (unlike saturated Write21).
inline void InspectorWord(uint8_t* out, uint32_t value) { for(unsigned i = 0; i < 5; ++i) out[i] = (uint64_t(value) >> (7*i)) & 127; }
FORGE_NOINLINE inline size_t EncodeInspector(uint16_t sequence, uint8_t page, const InspectorSnapshot& s,
                                           const InspectorLog& log, uint32_t cursor, uint8_t* out) {
    Header(out, 0x46, sequence); out[7] = 0; out[8] = page; out[9] = kInspectorSchema;
    InspectorWord(out + 10, s.generation); size_t n = 15;
    auto byte = [&](uint8_t v) { out[n++] = v & 127; };
    auto word = [&](uint32_t v) { InspectorWord(out + n, v); n += 5; };
    auto unit = [&](float v) { WriteNormalized(out + n, v); n += 2; };
    const auto& a = s.audio; const auto& sys = s.system; const auto& st = s.storage;
    if(page == 2) {
        byte(kFirmwareMinor); byte(kProtocolVersion); byte(sys.simulated ? 1 : 0);
        word(sys.uptime_ms); word(a.time_ms); word(a.block);
        Write14(out+n, std::isfinite(a.cpu_average) ? unsigned(Clamp(a.cpu_average*1000,0.f,16383.f)) : 0); n+=2;
        Write14(out+n, std::isfinite(a.cpu_peak) ? unsigned(Clamp(a.cpu_peak*1000,0.f,16383.f)) : 0); n+=2;
        for(unsigned i=0;i<2;++i) { word(sys.rx[i]); word(sys.tx[i]); word(sys.tx_errors[i]); word(sys.ingress_drops[i]); }
        word(sys.dropped); word(sys.rejected);
    } else if(page == 3) {
        for(uint64_t keys : {a.physical_keys, a.logical_keys}) for(unsigned i=0;i<6;++i) byte((keys>>(7*i))&127);
        byte(a.physical_flags); byte(a.logical_flags); word(a.menu);
        for(auto v : a.raw_turns) word(v);
        for(auto v : a.turns) word(v);
        byte(a.knob_pages & 127); byte(a.knob_pages >> 7);           // knob n's page: bits 2n..2n+1
    } else if(page == 4) {
        for(const auto& v : a.voices) { byte(v.note); byte(v.source); byte(v.stage); byte(v.slot); byte(v.flags); unit(v.envelope); }
        unit(a.cutoff); unit((a.lfo+1)*.5f); unit(a.wheel); byte(a.pedals);
        for(float b : a.bend) { Write14(out+n, unsigned(Clamp(b*4096,0.f,16383.f))); n+=2; }
        unit(a.mix); unit(a.feedback/.85f); unit(a.level); unit(a.delay_samples/48000); unit(a.reverb_mix);
    } else if(page == 5) {
        byte((st.present?1:0)|(st.mounted?2:0)|(st.busy?4:0)|(st.loading?8:0)|(a.recording?16:0)|(a.locked?32:0));
        byte(a.record_source); byte(st.pending); byte(st.job); byte(st.last_error); byte(st.partial_slots);
        word(st.loaded_selection); word(st.file_loaded_frames); word(st.file_frames); word(st.pool_used_bytes);
        word(st.pool_capacity_bytes); word(a.record_frames); word(st.record_capacity_frames); word(st.errors);
        word(sys.event_drops); word(sys.emergencies); word(sys.panel_drops); word(sys.sample_drops);
        byte(a.loop_flags); word(a.loop_length); unit(a.loop_position); unit((a.loop_speed + 2.f) * .25f); unit(a.loop_feedback);
    } else if(page == 6) {
        word(log.Latest()); word(log.Overwritten());
        InspectorEvent events[3]; const unsigned count = log.After(cursor, events); byte(count);
        for(unsigned i=0;i<count;++i) { const auto& e=events[i]; word(e.serial); word(e.time_ms); byte(static_cast<uint8_t>(e.kind)); byte(e.id); word(e.value); }
    } else if(page == 7) n += EncodePatchData(a.patch, out+n);
    else return EncodeError(sequence, Error::Patch, out);
    out[n] = Checksum(out,n); return n+1;
}
} // namespace forge
#endif
