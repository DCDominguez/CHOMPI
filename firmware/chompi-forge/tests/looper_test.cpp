// The TAPE-style looper (core/looper.h): gestures, first take and seam,
// overdub/feedback, pause/clear/panic fades, varispeed/reverse/scrub,
// auto-close, limiter, CC buttons, and a randomised run for the sanitizers.
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <vector>
#include "../core/looper.h"

using namespace forge;

namespace {
constexpr float kRate = 48000.f;
constexpr unsigned kBlock = 48;
constexpr float kTwoPi = 6.2831853f;

struct Rig {
    std::vector<int16_t> memory;
    Looper looper;
    uint64_t t = 0;                                   // frames processed
    std::vector<float> out_l, out_r;                  // what the last Run produced
    explicit Rig(uint32_t capacity = 4 * 48000) : memory(2 * capacity, 0x5a5a) { looper.Init(memory.data(), capacity, kRate); }
    // Runs `frames` frames of input(t) through the looper, ticking every block.
    template<typename F> void Run(uint32_t frames, F input) {
        out_l.assign(frames, 0.f); out_r.assign(frames, 0.f);
        for(uint32_t i = 0; i < frames; ++i) {
            if(t % kBlock == 0) looper.Tick(kBlock);
            float l = input(t), r = l;
            looper.Process(l, r);
            out_l[i] = l; out_r[i] = r; ++t;
        }
    }
    void Silence(uint32_t frames) { Run(frames, [](uint64_t) { return 0.f; }); }
    void Press(bool play, bool down) { if(play) looper.Play(down); else looper.Loop(down); }
    void Tap(bool play) { Press(play, true); Silence(kBlock); Press(play, false); Silence(kBlock); }
};
float Sine(uint64_t t, float hz, float amp = 0.5f) { return amp * std::sin(kTwoPi * hz * t / kRate); }
float MaxStep(const std::vector<float>& x) {
    float m = 0.f;
    for(size_t i = 1; i < x.size(); ++i) m = std::fmax(m, std::fabs(x[i] - x[i - 1]));
    return m;
}
float Rms(const std::vector<float>& x, size_t from = 0, size_t to = 0) {
    if(!to) to = x.size();
    double s = 0;
    for(size_t i = from; i < to; ++i) s += x[i] * x[i];
    return static_cast<float>(std::sqrt(s / (to - from)));
}
// Records a first take of `frames` of input, closing it with PLAY (plain playback) or LOOP (overdub).
template<typename F> void Record(Rig& rig, uint32_t frames, F input, bool close_with_play = true) {
    rig.Press(false, true); rig.Silence(kBlock); rig.Press(false, false);      // LOOP tap starts the take
    assert(rig.looper.GetState() == Looper::State::FirstTake);
    rig.Run(frames, input);
    rig.Press(close_with_play, true); rig.Run(kBlock, input); rig.Press(close_with_play, false);
    assert(rig.looper.GetState() == Looper::State::Playing);
}

void FirstTakeSeamAndPlayback() {
    Rig rig;
    Record(rig, 24000, [](uint64_t t) { return Sine(t, 440.f); });
    const uint32_t length = rig.looper.Length();
    assert(length >= 24000 && length <= 24000 + 3 * kBlock);
    assert(!rig.looper.Overdubbing());                                  // closed with PLAY: plain playback
    rig.Silence(3 * length);
    // The loop repeats: one loop period later the output is the same.
    for(uint32_t i = 1000; i < 1000 + length; i += 97) assert(std::fabs(rig.out_l[i] - rig.out_l[i + length]) < 1e-3f);
    // Seam: the 5 ms fades are in the loop itself, so there is no jump anywhere.
    assert(MaxStep(rig.out_l) < 0.08f);                                 // a 440 Hz 0.5 sine steps <= 0.029
    assert(Rms(rig.out_l) > 0.3f);
}

void LoopClosesIntoOverdubAndFeedback() {
    Rig rig;
    Record(rig, 12000, [](uint64_t t) { return Sine(t, 300.f, 0.3f); }, /*close_with_play=*/false);
    assert(rig.looper.Overdubbing());                                   // TAPE: LOOP ends the take into overdub
    const uint32_t length = rig.looper.Length();
    rig.Run(length, [](uint64_t t) { return Sine(t, 700.f, 0.3f); });   // one pass of overdub
    rig.Tap(true);                                                      // PLAY: overdub off, keeps playing
    assert(!rig.looper.Overdubbing() && rig.looper.GetState() == Looper::State::Playing);
    rig.Silence(2 * length);
    const float both = Rms(rig.out_l, length, 2 * length);
    assert(both > 0.27f && both < 0.33f);                               // two 0.3 sines: rms ~0.3
    // Feedback 50 %: an overdub pass with silence halves the loop.
    rig.looper.AdjustFeedback(-0.5f); rig.Silence(5000);                // feedback slews
    rig.Tap(false); assert(rig.looper.Overdubbing());
    rig.Silence(length); rig.Tap(false); assert(!rig.looper.Overdubbing());
    rig.Silence(2 * length);
    const float half = Rms(rig.out_l, length, 2 * length);
    assert(half > 0.4f * both && half < 0.6f * both);
}

void PlayWithoutOverdubLeavesTheLoopAlone() {
    Rig rig;
    Record(rig, 9600, [](uint64_t t) { return Sine(t, 500.f); });
    const std::vector<int16_t> before(rig.memory.begin(), rig.memory.begin() + 2 * rig.looper.Length());
    rig.Run(30000, [](uint64_t t) { return Sine(t, 1234.f, 0.9f); });  // live input while playing
    assert(std::equal(before.begin(), before.end(), rig.memory.begin()));
}

void PauseResumeJumpAndClear() {
    Rig rig;
    Record(rig, 24000, [](uint64_t t) { return Sine(t, 220.f); });
    rig.Silence(5000);
    rig.Tap(true);                                                      // pause
    assert(rig.looper.GetState() == Looper::State::Paused);
    rig.Silence(2400);                                                  // 50 ms
    assert(Rms(rig.out_l, 1200) < 1e-3f && MaxStep(rig.out_l) < 0.05f); // faded out, no click
    const float paused_at = rig.looper.Position();
    rig.Silence(9600); assert(std::fabs(rig.looper.Position() - paused_at) < 1e-6f);
    rig.Tap(true);                                                      // resume (on release)
    assert(rig.looper.GetState() == Looper::State::Playing);
    rig.Silence(4800); assert(Rms(rig.out_l, 2400) > 0.2f);
    // Hold PLAY 2 s while paused: back to the start, still paused.
    rig.Tap(true); assert(rig.looper.GetState() == Looper::State::Paused);
    rig.Press(true, true); rig.Silence(2 * 48000 + 4 * kBlock); rig.Press(true, false); rig.Silence(kBlock);
    assert(rig.looper.GetState() == Looper::State::Paused && rig.looper.Position() < 1e-4f);
    // Hold PLAY + LOOP 2 s: clear, with a fade, then empty.
    rig.Tap(true); assert(rig.looper.GetState() == Looper::State::Playing);
    rig.Press(true, true); rig.Press(false, true);
    rig.Silence(2 * 48000 + 10 * kBlock);                               // 2 s hold + the 5 ms fade
    assert(rig.looper.Empty() && !rig.looper.HasLoop());
    assert(MaxStep(rig.out_l) < 0.05f);
    rig.Press(true, false); rig.Press(false, false); rig.Silence(4800);
    assert(rig.looper.Empty() && Rms(rig.out_l) == 0.f);                // releasing the clear does nothing else
}

void CombinationsDoNotTriggerSingleKeys() {
    Rig rig;
    Record(rig, 9600, [](uint64_t t) { return Sine(t, 300.f); });
    rig.Press(true, true); rig.Silence(5); rig.Press(false, true);      // both within 10 ms
    rig.Silence(4800); rig.Press(true, false); rig.Press(false, false); rig.Silence(4800);
    assert(rig.looper.GetState() == Looper::State::Playing && !rig.looper.Overdubbing());
    // Armed from empty; the next note starts the first take; LOOP disarms.
    Rig armed;
    armed.Press(true, true); armed.Press(false, true); armed.Silence(kBlock);
    armed.Press(true, false); armed.Press(false, false); armed.Silence(kBlock);
    assert(armed.looper.GetState() == Looper::State::Armed);
    armed.Silence(48000); assert(armed.looper.GetState() == Looper::State::Armed);
    armed.looper.NoteStarted(); assert(armed.looper.GetState() == Looper::State::FirstTake);
    Rig disarm;
    disarm.Press(true, true); disarm.Press(false, true); disarm.Silence(kBlock);
    disarm.Press(true, false); disarm.Press(false, false); disarm.Silence(kBlock);
    disarm.Tap(false); assert(disarm.looper.GetState() == Looper::State::Empty);
}

float ZeroCrossingHz(const std::vector<float>& x, size_t from, size_t to) {
    unsigned n = 0; for(size_t i = from + 1; i < to; ++i) if(x[i - 1] <= 0.f && x[i] > 0.f) ++n;
    return n * kRate / (to - from);
}

void VarispeedReverseAndScrub() {
    Rig rig;
    Record(rig, 48000, [](uint64_t t) { return Sine(t, 200.f); });
    rig.Silence(9600);
    assert(std::fabs(ZeroCrossingHz(rig.out_l, 0, 9600) - 200.f) < 6.f);
    rig.looper.SetSpeed(2.f); rig.Silence(48000);                       // speed slews in ~10 ms
    assert(std::fabs(ZeroCrossingHz(rig.out_l, 4800, 48000) - 400.f) < 10.f);
    rig.looper.SetSpeed(0.5f); rig.Silence(48000);
    assert(std::fabs(ZeroCrossingHz(rig.out_l, 4800, 48000) - 100.f) < 5.f);
    // Reverse: the position runs backwards at the same pitch.
    rig.looper.SetSpeed(-1.f); rig.Silence(2400);
    const float p0 = rig.looper.Position(); rig.Silence(4800); const float p1 = rig.looper.Position();
    const float moved = p0 - p1 < 0.f ? p0 - p1 + 1.f : p0 - p1;
    assert(std::fabs(moved - 0.1f) < 0.01f);
    assert(std::fabs(ZeroCrossingHz(rig.out_l, 0, 4800) - 200.f) < 8.f);
    rig.looper.SetSpeed(2.f); rig.Silence(2400);
    rig.looper.ResetSpeed(); rig.Silence(4800);
    assert(std::fabs(ZeroCrossingHz(rig.out_l, 2400, 4800) - 200.f) < 15.f);
    // Scrub while paused moves the head and is audible; idle, it stays put.
    rig.Tap(true); rig.Silence(9600);
    const float still = rig.looper.Position();
    rig.looper.Scrub(5); rig.Silence(6000 + kBlock); rig.Silence(4800);
    assert(rig.looper.Position() != still && Rms(rig.out_l) > 0.01f);
    rig.Silence(9600); rig.Silence(9600); assert(Rms(rig.out_l) < 1e-3f);
}

void AutoCloseAndShortTakes() {
    Rig full(4800);
    full.Press(false, true); full.Silence(kBlock); full.Press(false, false);
    full.Run(10000, [](uint64_t t) { return Sine(t, 300.f); });
    assert(full.looper.GetState() == Looper::State::Playing && !full.looper.Overdubbing());
    assert(full.looper.Length() == 4800);
    Rig accident;                                                       // LOOP twice within a few ms
    accident.Press(false, true); accident.Silence(kBlock); accident.Press(false, false);
    accident.looper.Loop(true); accident.looper.Loop(false); accident.Silence(kBlock);
    assert(accident.looper.Length() >= 2 * 240 || accident.looper.Empty());
}

void PanicIsImmediateAndKeepsTheLoop() {
    Rig rig;
    Record(rig, 9600, [](uint64_t t) { return Sine(t, 300.f); }, false);
    rig.Silence(1000);
    rig.looper.Panic();
    float l = 0.25f, r = -0.25f; rig.looper.Process(l, r);
    assert(l == 0.25f && r == -0.25f);                                  // no loop in the very next sample
    assert(rig.looper.GetState() == Looper::State::Paused && rig.looper.HasLoop() && !rig.looper.Overdubbing());
    Rig take; take.Press(false, true); take.Silence(kBlock); take.Press(false, false);
    take.Run(4800, [](uint64_t t) { return Sine(t, 300.f); });
    take.looper.Panic();
    assert(take.looper.GetState() == Looper::State::Paused && take.looper.Length() >= 4800);
}

void LimiterKeepsOverdubsBounded() {
    Rig rig;
    Record(rig, 4800, [](uint64_t t) { return Sine(t, 300.f, 0.9f); }, false);
    rig.Run(20 * 4800, [](uint64_t t) { return Sine(t, 300.f, 0.9f); });   // 20 loud passes on top
    rig.Run(10 * 4800, [](uint64_t) { return 0.6f; });                  // the owner stacks up on every pass
    // Soft limiting: close to, but never pinned at, full scale (a hard clip would sit at 32767).
    int16_t top = 0; unsigned pinned = 0;
    for(uint32_t i = 0; i < 2 * rig.looper.Length(); ++i) {
        top = std::max<int16_t>(top, rig.memory[i]); pinned += rig.memory[i] >= 32700 || rig.memory[i] <= -32700;
    }
    assert(pinned == 0 && top > 29000);
    float peak = 0.f;
    rig.Tap(true); rig.Silence(4800);
    for(float v : rig.out_l) { assert(std::isfinite(v)); peak = std::fmax(peak, std::fabs(v)); }
    assert(peak <= 1.0f && peak > 0.7f);
}

void CcButtons() {
    CcButton b;
    assert(b.Edge(127) == 1 && b.Edge(127) == 0 && b.Edge(60) == 0 && b.Edge(41) == -1 && b.Edge(0) == 0);
    assert(b.Edge(84) == 0 && b.Edge(85) == 1 && b.Edge(42) == 0 && b.Edge(10) == -1);
}

void RandomUse() {
    std::mt19937 rng(7);
    Rig rig(9600);
    for(int block = 0; block < 20000; ++block) {
        switch(rng() % 40) {
            case 0: rig.looper.Play(true); break;   case 1: rig.looper.Play(false); break;
            case 2: rig.looper.Loop(true); break;   case 3: rig.looper.Loop(false); break;
            case 4: rig.looper.SetSpeed((rng() % 801) / 200.f - 2.f); break;
            case 5: rig.looper.Scrub(static_cast<int>(rng() % 21) - 10); break;
            case 6: rig.looper.AdjustFeedback(((rng() % 3) - 1.f) * 0.1f); break;
            case 7: rig.looper.Panic(); break;      case 8: rig.looper.NoteStarted(); break;
            case 9: rig.looper.SetTapeSlew(rng() & 1); break;
            default: break;
        }
        rig.Run(kBlock, [&](uint64_t t) { return Sine(t, 100.f + block % 900, 1.5f); });
        for(float v : rig.out_l) assert(std::isfinite(v) && std::fabs(v) < 3.f);
        assert(rig.looper.Length() <= 9600 && rig.looper.Position() >= 0.f && rig.looper.Position() <= 1.f);
    }
}
// 0.16 (the owner): a new first take starts at 1x whatever the speed was left at; SW5's tone filters
// the loop's playback only (a 10 kHz loop is much quieter closed, unchanged open); a clear
// opens it again.
void NewTakeAtUnitySpeedAndLoopTone() {
    Rig rig;
    rig.looper.SetSpeed(-2.f);
    Record(rig, 24000, [](uint64_t t) { return Sine(t, 10000.f); });
    assert(rig.looper.Speed() == 1.f && rig.looper.Tone() == 1.f);
    rig.Silence(4800); const float open = Rms(rig.out_l, 2400);
    rig.looper.SetTone(.3f);                                   // ~240 Hz
    rig.Silence(4800); const float closed = Rms(rig.out_l, 2400);
    assert(open > .3f && closed < open * .02f);
    rig.looper.NudgeTone(1.f); assert(rig.looper.Tone() == 1.f);
    rig.Silence(4800); assert(std::fabs(Rms(rig.out_l, 2400) - open) < open * .02f);   // open again: as before
    rig.looper.SetTone(.5f); rig.looper.SetSpeed(1.5f);
    rig.looper.Clear(); rig.Silence(4800);
    assert(rig.looper.Empty() && rig.looper.Tone() == 1.f);
    Record(rig, 9600, [](uint64_t t) { return Sine(t, 440.f); });
    assert(rig.looper.Speed() == 1.f);
}
} // namespace

// 0.20 one-level overdub undo: PLAY + LOOP tapped together takes the last overdub pass back, bit for bit
// (forwards and in reverse), restored in chunks; one level; a loop over half the memory has no undo;
// holding the combination 2 s still clears; a new take or a clear forgets the snapshot.
void OverdubUndo() {
    auto loop_of = [](Rig& rig) { const int16_t* d = rig.looper.Data(); return std::vector<int16_t>(d, d + 2 * rig.looper.Length()); };
    auto combo_tap = [](Rig& rig) {
        rig.looper.Play(true); rig.looper.Loop(true); rig.Silence(4 * kBlock);
        rig.looper.Loop(false); rig.looper.Play(false); rig.Silence(kBlock);
    };
    auto settle = [](Rig& rig) { for(int i = 0; i < 4000 && rig.looper.Restoring(); ++i) rig.Silence(kBlock); };
    {   // forwards
        Rig rig;                                                    // 4 s of memory: undo for loops up to 2 s
        Record(rig, 48000, [](uint64_t t) { return Sine(t, 220.f, .3f); });   // a 1 s loop, plain playback
        const auto before = loop_of(rig);
        assert(!rig.looper.CanUndo());
        rig.Tap(false);                                             // LOOP: overdub on
        assert(rig.looper.Overdubbing());
        rig.Run(70000, [](uint64_t t) { return Sine(t, 660.f, .2f); });       // more than a lap
        rig.Tap(false);                                             // overdub off
        assert(loop_of(rig) != before && rig.looper.CanUndo());
        combo_tap(rig);
        assert(!rig.looper.Overdubbing() && rig.looper.GetState() == Looper::State::Playing);
        settle(rig);
        assert(loop_of(rig) == before);                             // bit for bit
        assert(!rig.looper.CanUndo() && rig.looper.Undos() == 1);   // one level
        combo_tap(rig); settle(rig); assert(loop_of(rig) == before && rig.looper.Undos() == 1);
        // Undo while still overdubbing: the pass stops and goes back.
        rig.Tap(false); rig.Run(20000, [](uint64_t t) { return Sine(t, 330.f, .2f); });
        combo_tap(rig); settle(rig);
        assert(!rig.looper.Overdubbing() && loop_of(rig) == before);
    }
    {   // people press the two keys tens of ms apart (functions test, 2026-10-13): either first, 40 ms
        // ahead, and the undo still takes back the last pass with the loop playing on.
        for(bool play_first : {true, false}) {
            Rig rig;
            Record(rig, 48000, [](uint64_t t) { return Sine(t, 220.f, .3f); });
            const auto before = loop_of(rig);
            rig.Tap(false); rig.Run(60000, [](uint64_t t) { return Sine(t, 660.f, .2f); }); rig.Tap(false);
            rig.Silence(24000);
            if(play_first) { rig.looper.Play(true); rig.Silence(1920); rig.looper.Loop(true); }
            else { rig.looper.Loop(true); rig.Run(1920, [](uint64_t t) { return Sine(t, 880.f, .3f); }); rig.looper.Play(true); }
            rig.Silence(4 * kBlock); rig.looper.Loop(false); rig.looper.Play(false); rig.Silence(kBlock);
            settle(rig);
            assert(rig.looper.GetState() == Looper::State::Playing && !rig.looper.Overdubbing());
            assert(loop_of(rig) == before);
        }
        // and in reverse with LOOP first (the grace arc grows downwards)
        Rig rig;
        Record(rig, 30000, [](uint64_t t) { return Sine(t, 300.f, .3f); });
        const auto before = loop_of(rig);
        rig.looper.SetSpeed(-1.f); rig.Silence(9600);
        rig.Tap(false); rig.Run(40000, [](uint64_t t) { return Sine(t, 500.f, .25f); }); rig.Tap(false);
        rig.looper.Loop(true); rig.Run(1920, [](uint64_t t) { return Sine(t, 700.f, .3f); }); rig.looper.Play(true);
        rig.Silence(4 * kBlock); rig.looper.Loop(false); rig.looper.Play(false); rig.Silence(kBlock);
        settle(rig); assert(loop_of(rig) == before);
    }
    {   // in reverse (the head walks backwards: the saved arc grows downwards)
        Rig rig;
        Record(rig, 30000, [](uint64_t t) { return Sine(t, 300.f, .3f); });
        const auto before = loop_of(rig);
        rig.looper.SetSpeed(-1.f); rig.Silence(9600);
        rig.Tap(false); rig.Run(45000, [](uint64_t t) { return Sine(t, 500.f, .25f); }); rig.Tap(false);
        assert(loop_of(rig) != before);
        combo_tap(rig); settle(rig);
        assert(loop_of(rig) == before);
    }
    {   // the restore is spread over blocks (no stall), then playback goes on
        Rig rig(8 * 48000);
        Record(rig, 3 * 48000 + 24000, [](uint64_t t) { return Sine(t, 200.f, .3f); });   // 3.5 s, under half of 8 s
        rig.Tap(false); rig.Run(4 * 48000, [](uint64_t t) { return Sine(t, 400.f, .2f); }); rig.Tap(false);
        assert(rig.looper.CanUndo());
        combo_tap(rig);
        assert(rig.looper.Restoring());                             // 168k frames at 1024 per block: still going
        settle(rig); assert(!rig.looper.Restoring());
    }
    {   // a loop longer than half the memory: no undo (and nothing breaks)
        Rig rig;
        Record(rig, 3 * 48000, [](uint64_t t) { return Sine(t, 220.f, .3f); });
        rig.Tap(false); rig.Run(20000, [](uint64_t t) { return Sine(t, 440.f, .2f); }); rig.Tap(false);
        assert(!rig.looper.CanUndo());
        const auto after = loop_of(rig);
        combo_tap(rig); settle(rig); assert(loop_of(rig) == after && rig.looper.Undos() == 0);
    }
    {   // holding the combination 2 s still clears; a cleared looper has nothing to undo
        Rig rig;
        Record(rig, 48000, [](uint64_t t) { return Sine(t, 220.f, .3f); });
        rig.Tap(false); rig.Run(10000, [](uint64_t t) { return Sine(t, 440.f, .2f); }); rig.Tap(false);
        rig.looper.Play(true); rig.looper.Loop(true); rig.Silence(2 * 48000 + 4800); rig.looper.Loop(false); rig.looper.Play(false);
        rig.Silence(9600);
        assert(rig.looper.Empty() && !rig.looper.CanUndo() && rig.looper.Undos() == 0);
        // and on an empty looper the tap still arms (no undo)
        combo_tap(rig); assert(rig.looper.GetState() == Looper::State::Armed && rig.looper.Undos() == 0);
    }
}

int main() {
    OverdubUndo();
    FirstTakeSeamAndPlayback(); LoopClosesIntoOverdubAndFeedback(); PlayWithoutOverdubLeavesTheLoopAlone();
    PauseResumeJumpAndClear(); CombinationsDoNotTriggerSingleKeys(); VarispeedReverseAndScrub();
    AutoCloseAndShortTakes(); PanicIsImmediateAndKeepsTheLoop(); LimiterKeepsOverdubsBounded(); CcButtons(); RandomUse();
    NewTakeAtUnitySpeedAndLoopTone();
    std::cout << "PASS: looper first take/seam, overdub/feedback, play without dub, pause/resume/jump/clear, combinations/arm, "
                 "varispeed/reverse/scrub, auto-close, panic, limiter, CC buttons, random use, overdub undo, new take at 1x, loop tone\n";
}
