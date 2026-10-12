#pragma once
#include <cmath>
#include <cstdint>

// A tiny built-in bell (0.19.2) for the start-up chime when the sound CHOMPI wakes up with cannot
// play it: no instrument (the pass-through), or a sample that has not loaded yet. Four voices of a
// sine with a soft inharmonic partial (x2.76, a small bell's), decaying in about a second. Runs only
// while a voice sounds; no allocation; audio owner only.
namespace forge {

class Bell {
public:
    static constexpr unsigned kVoices = 4;
    void Init(float sample_rate) { rate_ = sample_rate; decay_ = std::exp(-1.f / (0.35f * sample_rate)); }
    void Note(uint8_t note, uint8_t velocity) {
        if(!velocity || !rate_) return;
        Voice* v = &voices_[0];
        for(Voice& c : voices_) if(c.level < v->level) v = &c;          // the quietest one
        const float hz = 440.f * std::exp2((static_cast<int>(note) - 69) / 12.f);
        v->phase = v->phase2 = 0.f; v->step = hz / rate_; v->step2 = 2.76f * hz / rate_;
        v->level = 0.22f * velocity / 127.f; active_ = true;
    }
    bool Active() const { return active_; }
    float Next() {
        float out = 0.f; bool any = false;
        for(Voice& v : voices_) {
            if(v.level < 1e-5f) { v.level = 0.f; continue; }
            any = true;
            out += v.level * (std::sin(6.2831853f * v.phase) + 0.3f * v.level * std::sin(6.2831853f * v.phase2));
            v.phase += v.step; if(v.phase >= 1.f) v.phase -= 1.f;
            v.phase2 += v.step2; if(v.phase2 >= 1.f) v.phase2 -= 1.f;
            v.level *= decay_;
        }
        active_ = any;
        return out;
    }
    void Clear() { for(Voice& v : voices_) v.level = 0.f; active_ = false; }
private:
    struct Voice { float phase = 0, phase2 = 0, step = 0, step2 = 0, level = 0; };
    Voice voices_[kVoices] = {};
    float rate_ = 0.f, decay_ = 0.f;
    bool active_ = false;
};

} // namespace forge
