#pragma once
#include <atomic>
#include <cstdint>
#include <cstring>
#include "recorder.h"
#include "sample_table.h"
#include "wav.h"
#include "inspector.h"

namespace forge {
// ---- TAPE file names: <jammi|cubbi>_<a-e><1-14>[_double].wav in the SD root ----
inline void SamplePath(uint8_t mode, uint8_t bank, uint8_t slot, bool twice, char (&out)[24]) {
    // Hand-formatted: snprintf would pull ~1.5 KB of newlib into the firmware.
    std::memcpy(out, mode ? "cubbi_" : "jammi_", 6);
    size_t n = 6;
    out[n++] = static_cast<char>('a' + bank);
    const unsigned number = slot + 1u;
    if(number >= 10) out[n++] = static_cast<char>('0' + number / 10);
    out[n++] = static_cast<char>('0' + number % 10);
    if(twice) { std::memcpy(out + n, "_double", 7); n += 7; }
    std::memcpy(out + n, ".wav", 5);
}
inline bool ParseSampleName(const char* name, uint8_t& mode, uint8_t& bank, uint8_t& slot) {
    char lower[32];
    size_t n = 0;
    for(; name[n] && n < sizeof lower - 1; ++n) lower[n] = (name[n] >= 'A' && name[n] <= 'Z') ? char(name[n] + 32) : name[n];
    if(name[n]) return false;
    lower[n] = 0;
    if(n < 11 || std::strcmp(lower + n - 4, ".wav") || lower[5] != '_') return false;
    if(!std::strncmp(lower, "jammi", 5)) mode = 0; else if(!std::strncmp(lower, "cubbi", 5)) mode = 1; else return false;
    if(lower[6] < 'a' || lower[6] >= 'a' + kSampleBanks) return false;
    bank = static_cast<uint8_t>(lower[6] - 'a');
    const size_t digits = n - 4 - 7;                       // between the bank letter and ".wav"
    if(digits < 1 || digits > 2 || lower[7] == '0') return false;
    unsigned value = 0;
    for(size_t i = 0; i < digits; ++i) {
        if(lower[7 + i] < '0' || lower[7 + i] > '9') return false;
        value = value * 10 + unsigned(lower[7 + i] - '0');
    }
    if(value < 1 || value > kSampleSlots - 1) return false;    // slot 15 is the recording, never a file
    slot = static_cast<uint8_t>(value - 1);
    return true;
}

// File access for samples (main loop only). One read handle and one write
// handle may be open at the same time.
class SampleFiles {
public:
    virtual ~SampleFiles() = default;
    virtual bool Ready() = 0;
    virtual bool OpenRead(const char* path, uint32_t& size) = 0;
    virtual bool ReadAt(uint32_t offset, uint8_t* buffer, uint32_t size, uint32_t& got) = 0;
    virtual void CloseRead() = 0;
    virtual bool OpenWrite(const char* temp_path) = 0;                       // create or truncate
    virtual bool Append(const uint8_t* data, uint32_t size) = 0;
    virtual bool FinishWrite(const char* temp_path, const char* final_path) = 0; // sync, close, replace
    virtual void AbortWrite(const char* temp_path) = 0;
    virtual bool Remove(const char* path) = 0;                               // a missing file is fine
    virtual bool ListRoot(void (*visit)(void* context, const char* name), void* context) = 0;
};

// Lock-free handshake before the main loop rewrites file slots 0-13. The
// audio owner calls AudioBlock once per block: while a rewrite is pending it
// refuses new file-slot notes, fades out the ones sounding and acknowledges
// once none remain. The main loop rewrites the slots only after Detached(),
// then Publish()es; loaded counters keep advancing afterwards.
class SampleHandoff {
public:
    void RequestDetach() { request_.fetch_add(1, std::memory_order_acq_rel); }
    bool Detached() const { return ack_.load(std::memory_order_acquire) == request_.load(std::memory_order_relaxed); }
    void Publish() { published_.store(request_.load(std::memory_order_relaxed), std::memory_order_release); }
    template<class Voices> bool AudioBlock(Voices& voices) {
        const uint32_t request = request_.load(std::memory_order_acquire);
        if(published_.load(std::memory_order_acquire) == request) return true;
        voices.ReleaseSampleVoices(false);
        if(!voices.SampleVoicesActive(false)) ack_.store(request, std::memory_order_release);
        return false;
    }
private:
    std::atomic<uint32_t> request_{0}, ack_{0}, published_{0};
};

// What the audio owner wants loaded, packed into one word it publishes each block.
inline uint32_t PackSelection(const Parameters& p) {
    if(!p.Sampler()) return 0;
    return 1u | (uint32_t(p.sample_mode) << 1) | (uint32_t(p.sample_bank) << 2) | (uint32_t(p.sample_slot) << 5);
}

struct SampleJob {
    enum class Kind : uint8_t { Save, Copy, Erase } kind = Kind::Save;
    uint8_t mode = 0, bank = 0, slot = 0, to_mode = 0, to_bank = 0, to_slot = 0;
    uint32_t frames = 0; float gain = 1.f;         // Save: the locked recording (or loop)
    bool from_loop = false;                        // Save: from the looper instead of the recording
    uint16_t sequence = 0; uint8_t source = 0xff;  // host request to answer (0xff = panel)
};
struct SampleEvent {
    SampleJob job;
    bool ok = false;
};

// Main-loop state machine: scans the card, loads the wanted chromatic slot or
// kit bank into the pool, and runs save/copy/erase jobs. Each Poll does at
// most one file operation of at most `scratch_size` bytes, so the main loop
// keeps serving MIDI between steps.
class SampleLoader {
public:
    static constexpr unsigned kJobs = 4;
    void Init(SampleTable* table, SampleHandoff* handoff, int16_t* pool, uint32_t pool_samples,
              uint8_t* scratch, uint32_t scratch_size) {
        table_ = table; handoff_ = handoff; pool_ = pool; pool_samples_ = pool_samples;
        scratch_ = scratch; scratch_size_ = scratch_size;
        state_ = State::Idle; current_ = kNone; scanned_ = false; head_ = tail_ = 0;
        std::memset(occupancy_, 0, sizeof occupancy_);
    }
    bool Queue(const SampleJob& job) {
        const unsigned next = (head_ + 1) % kJobs;
        if(next == tail_) return false;
        jobs_[head_] = job; head_ = next;
        return true;
    }
    // One step. `wanted` comes from PackSelection; `recording` is the RAM
    // slot's data for Save jobs. Returns true with `event` when a job ends.
    FORGE_NOINLINE bool Poll(SampleFiles& files, uint32_t wanted, const int16_t* recording, SampleEvent& event,
                             const int16_t* loop = nullptr) {
        loop_ = loop;
        const bool ready = files.Ready();
        if(!ready) {
            if(scanned_) { std::memset(occupancy_, 0, sizeof occupancy_); scanned_ = false; }
            if(state_ == State::Streaming || state_ == State::Headers) { files.CloseRead(); open_ = false; state_ = State::Idle; current_ = kNone; }
            if(state_ == State::Job) return Fail(files, event);
            if(state_ == State::Idle && head_ != tail_) { job_ = jobs_[tail_]; tail_ = (tail_ + 1) % kJobs; return Finish(files, false, event); }
        }
        if(ready && !scanned_) { Scan(files); current_ = kNone; return false; }
        // A different file selection restarts loading. Selecting the recording
        // (or no sampler) leaves the loaded files in place.
        const uint32_t files_wanted = FileSelection(wanted);
        if(state_ == State::Headers || state_ == State::Streaming || state_ == State::Detaching) {
            if(files_wanted && files_wanted != target_) { files.CloseRead(); open_ = false; state_ = State::Idle; current_ = kNone; }
        }
        switch(state_) {
            case State::Idle:
                if(head_ != tail_) { StartJob(files, recording, event); return state_ == State::Idle; }
                if(ready && files_wanted && files_wanted != current_) {
                    target_ = files_wanted; handoff_->RequestDetach(); state_ = State::Detaching;
                }
                return false;
            case State::Detaching:
                if(!handoff_->Detached()) return false;
                for(uint8_t s = 0; s < kRamSlot; ++s) Clear(table_->slots[s]);
                count_ = 0; cursor_ = 0;
                if(target_ & 1u) {
                    const uint8_t mode = (target_ >> 1) & 1u, bank = (target_ >> 2) & 7u, slot = (target_ >> 5) & 15u;
                    for(uint8_t s = 0; s < kRamSlot; ++s)
                        if((occupancy_[mode][bank] >> s) & 1u && (mode == 1 || s == slot)) list_[count_++] = s;
                }
                index_ = 0; state_ = State::Headers;
                return false;
            case State::Headers:
                if(index_ < count_) { Header(files, list_[index_++]); return false; }
                handoff_->Publish();
                index_ = 0; open_ = false; state_ = count_ ? State::Streaming : State::Idle;
                if(!count_) current_ = target_;
                return false;
            case State::Streaming: Stream(files); return false;
            case State::Job: return StepJob(files, recording, event);
        }
        return false;
    }
    bool Busy() const { return state_ != State::Idle || head_ != tail_; }
    bool Loading() const { return state_ == State::Detaching || state_ == State::Headers || state_ == State::Streaming; }
    uint16_t Occupancy(uint8_t mode, uint8_t bank) const { return mode < 2 && bank < kSampleBanks ? occupancy_[mode][bank] : 0; }
    bool Scanned() const { return scanned_; }
#ifdef FORGE_TEST_HOOKS
    void Inspect(InspectorStorage& s) const {
        s.busy=Busy(); s.loading=Loading(); s.loaded_selection=current_;
        s.pending=static_cast<uint8_t>((head_+kJobs-tail_)%kJobs);
        s.job=state_==State::Job?static_cast<uint8_t>(job_.kind):127;
        s.pool_used_bytes=cursor_*2; s.pool_capacity_bytes=pool_samples_*2;
        s.errors=inspector_errors_; s.last_error=inspector_errors_?8:0;
        s.file_frames=0; s.file_loaded_frames=0; s.partial_slots=0;
        for(unsigned i=0;i<kRamSlot;++i) { const auto& slot=table_->slots[i];
            s.file_frames+=slot.frames; s.file_loaded_frames+=slot.loaded.load(std::memory_order_acquire);
            if(slot.partial) ++s.partial_slots;
        }
    }
#endif
private:
#ifdef FORGE_TEST_HOOKS
    uint32_t inspector_errors_=0;
#endif
    const int16_t* loop_ = nullptr;                 // looper memory (loop saves)
    enum class State : uint8_t { Idle, Detaching, Headers, Streaming, Job };
    static constexpr uint32_t kNone = 0xffffffffu;
    // Only file slots matter: the recording slot needs no loading.
    static uint32_t FileSelection(uint32_t wanted) {
        if(!(wanted & 1u)) return 0;
        const uint32_t mode = (wanted >> 1) & 1u, slot = (wanted >> 5) & 15u;
        if(mode == 0 && slot == kRamSlot) return 0;
        return mode ? (wanted & 0x1fu) : wanted;                   // kit: the whole bank, any slot
    }
    static void Clear(SampleSlot& s) {
        s.loaded.store(0, std::memory_order_release);
        s.channels = 0; s.frames = 0; s.partial = false; s.gain = 1.f; s.rate_ratio = 1.f; s.data = nullptr;
    }
    FORGE_NOINLINE void Scan(SampleFiles& files) {
        std::memset(occupancy_, 0, sizeof occupancy_);
        scanned_ = files.ListRoot([](void* context, const char* name) {
            auto* self = static_cast<SampleLoader*>(context);
            uint8_t mode, bank, slot;
            if(ParseSampleName(name, mode, bank, slot)) self->occupancy_[mode][bank] |= uint16_t(1u << slot);
        }, this);
#ifdef FORGE_TEST_HOOKS
        if(!scanned_) ++inspector_errors_;
#endif
    }
    FORGE_NOINLINE void Header(SampleFiles& files, uint8_t slot) {
        const uint8_t mode = (target_ >> 1) & 1u, bank = (target_ >> 2) & 7u;
        char path[24]; SamplePath(mode, bank, slot, false, path);
        uint32_t size = 0, got = 0;
        WavInfo info;
        const bool ok = files.OpenRead(path, size) && files.ReadAt(0, scratch_, scratch_size_ < 4096 ? scratch_size_ : 4096, got)
                        && ParseWav(scratch_, got, size, info) == WavError::None && info.Frames();
        files.CloseRead();
        info_[slot] = info;
        if(!ok) {
#ifdef FORGE_TEST_HOOKS
            ++inspector_errors_;
#endif
            return;                                            // unreadable or unsupported: slot stays empty
        }
        SampleSlot& s = table_->slots[slot];
        const uint32_t room = (pool_samples_ - cursor_) / info.channels;
        const uint32_t frames = info.Frames() < room ? info.Frames() : room;
        if(!frames) return;
        s.data = pool_ + cursor_; s.frames = frames; s.channels = info.channels;
        s.partial = frames < info.Frames(); s.rate_ratio = info.rate / 48000.f; s.gain = 1.f;
        cursor_ += frames * info.channels;
    }
    FORGE_NOINLINE void Stream(SampleFiles& files) {
        while(index_ < count_ && !table_->slots[list_[index_]].frames) ++index_;   // skipped headers
        if(index_ >= count_) { state_ = State::Idle; current_ = target_; return; }
        const uint8_t slot = list_[index_];
        SampleSlot& s = table_->slots[slot];
        const WavInfo& info = info_[slot];
        const uint32_t done = s.loaded.load(std::memory_order_relaxed);
        if(!open_) {
            const uint8_t mode = (target_ >> 1) & 1u, bank = (target_ >> 2) & 7u;
            char path[24]; SamplePath(mode, bank, slot, false, path);
            uint32_t size;
            if(!files.OpenRead(path, size)) {
#ifdef FORGE_TEST_HOOKS
                ++inspector_errors_;
#endif
                s.partial = true; NextSlot(files); return;
            }
            open_ = true;
        }
        const uint32_t align = info.BlockAlign();
        uint32_t frames = scratch_size_ / align;
        if(frames > s.frames - done) frames = s.frames - done;
        uint32_t got = 0;
        if(!frames || !files.ReadAt(info.data_offset + done * align, scratch_, frames * align, got) || got < align) {
#ifdef FORGE_TEST_HOOKS
            ++inspector_errors_;
#endif
            s.partial = true;                  // frames past `loaded` play as silence
            NextSlot(files); return;
        }
        frames = got / align;
        ConvertFrames(info, scratch_, frames, const_cast<int16_t*>(s.data) + static_cast<size_t>(done) * info.channels);
        s.loaded.store(done + frames, std::memory_order_release);
        if(done + frames >= s.frames) NextSlot(files);
    }
    void NextSlot(SampleFiles& files) { files.CloseRead(); open_ = false; ++index_; }
    FORGE_NOINLINE void StartJob(SampleFiles& files, const int16_t* recording, SampleEvent& event) {
        job_ = jobs_[tail_]; tail_ = (tail_ + 1) % kJobs;
        if(job_.kind == SampleJob::Kind::Save && job_.from_loop) recording = loop_;
        progress_ = 0;
        if(job_.kind == SampleJob::Kind::Erase) {
            char path[24], twice[24];
            SamplePath(job_.mode, job_.bank, job_.slot, false, path); SamplePath(job_.mode, job_.bank, job_.slot, true, twice);
            const bool ok = files.Remove(path) && files.Remove(twice);
            if(ok) occupancy_[job_.mode][job_.bank] &= uint16_t(~(1u << job_.slot));
            Finish(files, ok, event);
            return;
        }
        if(job_.kind == SampleJob::Kind::Copy) {
            char path[24]; SamplePath(job_.mode, job_.bank, job_.slot, false, path);
            if(!((occupancy_[job_.mode][job_.bank] >> job_.slot) & 1u) || !files.OpenRead(path, size_)) { Finish(files, false, event); return; }
        } else if(!recording || !job_.frames) { Finish(files, false, event); return; }
        if(!files.OpenWrite(kTemp)) { files.CloseRead(); Finish(files, false, event); return; }
        if(job_.kind == SampleJob::Kind::Save) {
            uint8_t header[kWavHeaderSize]; WriteWavHeader(job_.frames, header);
            if(!files.Append(header, kWavHeaderSize)) { Fail(files, event); return; }
        }
        state_ = State::Job;
    }
    FORGE_NOINLINE bool StepJob(SampleFiles& files, const int16_t* recording, SampleEvent& event) {
        uint32_t bytes = 0;
        if(job_.from_loop) recording = loop_;
        if(job_.kind == SampleJob::Kind::Save) {
            const uint32_t frames_left = job_.frames - progress_;
            uint32_t frames = scratch_size_ / 4; if(frames > frames_left) frames = frames_left;
            const int16_t* from = recording + 2 * static_cast<size_t>(progress_);
            for(uint32_t i = 0; i < 2 * frames; ++i) {
                float v = from[i] * job_.gain;
                v = v > 32767.f ? 32767.f : v < -32768.f ? -32768.f : v;
                const int16_t x = static_cast<int16_t>(v);
                scratch_[2 * i] = uint8_t(x & 255); scratch_[2 * i + 1] = uint8_t((uint16_t(x) >> 8) & 255);
            }
            bytes = frames * 4;
            if(!files.Append(scratch_, bytes)) return Fail(files, event);
            progress_ += frames;
            if(progress_ < job_.frames) return false;
        } else {
            uint32_t got = 0;
            const uint32_t want = size_ - progress_ < scratch_size_ ? size_ - progress_ : scratch_size_;
            if(want && (!files.ReadAt(progress_, scratch_, want, got) || got != want || !files.Append(scratch_, got)))
                return Fail(files, event);
            progress_ += got;
            if(progress_ < size_) return false;
            files.CloseRead();
        }
        const uint8_t mode = job_.kind == SampleJob::Kind::Save ? job_.mode : job_.to_mode;
        const uint8_t bank = job_.kind == SampleJob::Kind::Save ? job_.bank : job_.to_bank;
        const uint8_t slot = job_.kind == SampleJob::Kind::Save ? job_.slot : job_.to_slot;
        char path[24], twice[24];
        SamplePath(mode, bank, slot, false, path); SamplePath(mode, bank, slot, true, twice);
        if(!files.FinishWrite(kTemp, path)) return Finish(files, false, event);
        files.Remove(twice);                     // TAPE regenerates a missing _double at its next boot
        occupancy_[mode][bank] |= uint16_t(1u << slot);
        return Finish(files, true, event);
    }
    bool Fail(SampleFiles& files, SampleEvent& event) {
        files.CloseRead(); files.AbortWrite(kTemp);
        return Finish(files, false, event);
    }
    bool Finish(SampleFiles&, bool ok, SampleEvent& event) {
        event.job = job_; event.ok = ok;
#ifdef FORGE_TEST_HOOKS
        if(!ok) ++inspector_errors_;
#endif
        // A changed file of the loaded bank/slot is reloaded.
        if(ok && job_.kind != SampleJob::Kind::Copy) Invalidate(job_.mode, job_.bank);
        if(ok && job_.kind == SampleJob::Kind::Copy) Invalidate(job_.to_mode, job_.to_bank);
        state_ = State::Idle;
        return true;
    }
    void Invalidate(uint8_t mode, uint8_t bank) {
        if(current_ != kNone && (current_ & 1u) && ((current_ >> 1) & 1u) == mode && ((current_ >> 2) & 7u) == bank) current_ = kNone;
    }
    static constexpr const char* kTemp = "FORGE_TMP.WAV";
    SampleTable* table_ = nullptr;
    SampleHandoff* handoff_ = nullptr;
    int16_t* pool_ = nullptr;
    uint32_t pool_samples_ = 0, cursor_ = 0;
    uint8_t* scratch_ = nullptr;
    uint32_t scratch_size_ = 0;
    State state_ = State::Idle;
    uint32_t current_ = kNone, target_ = 0, progress_ = 0, size_ = 0;
    bool open_ = false;
    uint8_t list_[kSampleSlots]{}, count_ = 0, index_ = 0;
    WavInfo info_[kSampleSlots];
    bool scanned_ = false;
    uint16_t occupancy_[2][kSampleBanks]{};
    SampleJob jobs_[kJobs], job_;
    unsigned head_ = 0, tail_ = 0;
};
} // namespace forge
