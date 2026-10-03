#pragma once
#include <cstddef>
#include <cstdint>

namespace forge {
// Largest SysEx payload accepted (F0/F7 excluded); v3 apply requests are 69.
constexpr unsigned kMaxSysEx = 72;
struct MidiFrame {
    enum class Kind : uint8_t { CC, SysEx, NoteOn, NoteOff, PitchBend, ProgramChange };
    Kind kind = Kind::CC;
    uint8_t data[kMaxSysEx]{};
    uint8_t size = 0;
};

// Only the messages Forge needs. Real-time bytes may legally interrupt any
// MIDI message, including SysEx and running-status CC. Oversize SysEx is
// discarded in full; a new status resynchronizes without applying a prefix.
class MidiFramer {
public:
    void Reset() { sysex_ = overflow_ = false; used_ = status_ = cc_used_ = 0; }
    bool Feed(uint8_t byte, MidiFrame& out) {
        if(byte >= 0xf8) return false;
        if(byte == 0xf0) {
            Reset(); sysex_ = true; return false;
        }
        if(byte == 0xf7) {
            const bool complete = sysex_ && !overflow_ && used_ > 0;
            if(complete) {
                out.kind = MidiFrame::Kind::SysEx; out.size = used_;
                for(unsigned i = 0; i < used_; ++i) out.data[i] = bytes_[i];
            }
            Reset(); return complete;
        }
        if(byte & 0x80) {
            Reset();
            const uint8_t type = byte & 0xf0;
            if(type == 0xb0 || type == 0x90 || type == 0x80 || type == 0xe0 || type == 0xc0) status_ = byte;
            return false;
        }
        if(sysex_) {
            if(used_ < sizeof(bytes_)) bytes_[used_++] = byte;
            else overflow_ = true;
            return false;
        }
        if(status_) {
            cc_[cc_used_++] = byte;
            const uint8_t type = status_ & 0xf0;
            if(type == 0xc0) {                 // program change: one data byte
                out.kind = MidiFrame::Kind::ProgramChange; out.size = 2;
                out.data[0] = status_ & 0x0f; out.data[1] = cc_[0]; out.data[2] = 0;
                cc_used_ = 0; return true;
            }
            if(cc_used_ == 2) {
                out.kind = type == 0xb0 ? MidiFrame::Kind::CC : type == 0x90 ? MidiFrame::Kind::NoteOn
                    : type == 0xe0 ? MidiFrame::Kind::PitchBend : MidiFrame::Kind::NoteOff;
                out.size = 3;
                out.data[0] = status_ & 0x0f;
                out.data[1] = cc_[0]; out.data[2] = cc_[1];
                cc_used_ = 0; return true;
            }
        }
        return false;
    }
private:
    uint8_t bytes_[kMaxSysEx]{}, cc_[2]{};
    uint8_t used_ = 0, status_ = 0, cc_used_ = 0;
    bool sysex_ = false, overflow_ = false;
};
} // namespace forge
