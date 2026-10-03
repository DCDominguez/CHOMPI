#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>
#include "../core/command_queue.h"
#include "../core/engine.h"

using namespace forge;
constexpr float kRate = 48000.f;
constexpr size_t kCapacity = 48002;
bool Near(float a, float b, float tolerance = 0.0001f) { return std::abs(a - b) < tolerance; }

struct Fixture {
    std::vector<float> left, right;
    Engine engine;
    Fixture() : left(kCapacity, 99.f), right(kCapacity, -99.f) {
        assert(engine.Init(kRate, left.data(), right.data(), kCapacity));
        for(size_t i = 0; i < kCapacity; ++i) assert(left[i] == 0.f && right[i] == 0.f);
    }
    void Settle() {
        float l, r;
        for(unsigned i = 0; i < 48000; ++i) engine.Process(0.f, 0.f, l, r);
    }
};

void ParametersAndMidi() {
    Parameters p;
    assert(p.Apply({Parameter::Mix, 20.f}) && p.mix == 1.f);
    assert(p.Apply({Parameter::Level, -1.f}) && p.level == 0.f);
    assert(!p.Apply({Parameter::Mix, std::numeric_limits<float>::quiet_NaN()}));
    assert(!p.Apply({Parameter::Mix, std::numeric_limits<float>::infinity()}));
    assert(!p.Apply({static_cast<Parameter>(255), 0.f}));
    assert(p.mix == 1.f);
    Command command{Parameter::Mix, -1.f};
    for(uint8_t cc = 20; cc <= 24; ++cc) {
        assert(DecodeCC(0, cc, 0, command) && command.value == 0.f);
        assert(DecodeCC(0, cc, 127, command) && command.value == 1.f);
        assert(static_cast<unsigned>(command.parameter) == cc - 20u);
    }
    assert(!DecodeCC(1, 20, 127, command));
    assert(DecodeCC(0, 26, 127, command) && command.parameter == Parameter::Resonance);
    assert(DecodeCC(0, 27, 127, command) && command.parameter == Parameter::ReverbMix);
    assert(!DecodeCC(0, 28, 127, command));
    // CC26/27 are v3-only: a v1/v2 patch cannot report them in status, so they are refused.
    Parameters legacy; assert(DecodeCC(0, 27, 100, command) && !legacy.Apply(command) && legacy.reverb_mix == 0.f);
    Parameters v3; v3.version = 3; assert(v3.Apply(command) && v3.reverb_mix > 0.78f);
    assert(!DecodeCC(0, 20, 128, command));
    assert(DecodeCC(0, 24, 63, command) && p.Apply(command) && !p.bypass);
    assert(DecodeCC(0, 24, 64, command) && p.Apply(command) && p.bypass);
}

void QueueBoundariesAndConcurrency() {
    CommandQueue<4> small;
    Command command;
    assert(!small.Pop(command));
    for(unsigned cycle = 0; cycle < 100; ++cycle) {
        for(unsigned i = 0; i < 3; ++i) assert(small.Push({Parameter::Mix, float(i)}));
        assert(!small.Push({Parameter::Mix, 99.f}));
        for(unsigned i = 0; i < 3; ++i) assert(small.Pop(command) && command.value == float(i));
        assert(!small.Pop(command));
    }
    CommandQueue<64> concurrent;
    std::thread producer([&] {
        for(unsigned i = 0; i < 100000; ++i)
            while(!concurrent.Push({Parameter::Time, float(i)})) std::this_thread::yield();
    });
    for(unsigned i = 0; i < 100000; ++i) {
        while(!concurrent.Pop(command)) std::this_thread::yield();
        assert(command.parameter == Parameter::Time && command.value == float(i));
    }
    producer.join();
}

void InitializationAndDryStereo() {
    Engine uninitialized;
    float l = 9.f, r = 9.f;
    uninitialized.Process(1.f, 1.f, l, r);
    assert(l == 0.f && r == 0.f);
    Fixture f;
    assert(!uninitialized.Init(kRate, nullptr, f.right.data(), kCapacity));
    assert(!uninitialized.Init(kRate, f.left.data(), f.right.data(), 10));
    assert(!uninitialized.Init(0.f, f.left.data(), f.right.data(), kCapacity));
    assert(!uninitialized.Init(kRate, f.left.data(), f.left.data(), kCapacity));
    f.Settle();
    f.engine.Process(0.8f, -0.4f, l, r);
    assert(Near(l, 0.2f) && Near(r, -0.1f));
    f.engine.Apply({Parameter::Level, 0.f});
    f.Settle();
    f.engine.Process(1.f, 1.f, l, r);
    assert(Near(l, 0.f) && Near(r, 0.f));
}

void DelayImpulseAndBypass() {
    Fixture f;
    f.engine.Apply({Parameter::Mix, 1.f});
    f.engine.Apply({Parameter::Feedback, 0.f});
    f.engine.Apply({Parameter::Level, 1.f});
    // Default time 0.25 gives 257.5 ms, exactly 12360 samples at 48 kHz.
    f.Settle();
    float l, r;
    for(unsigned i = 0; i <= 12361; ++i) {
        f.engine.Process(i == 0 ? 0.5f : 0.f, 0.f, l, r);
        assert(Near(l, i == 12360 ? 0.5f : 0.f));
        assert(Near(r, 0.f));
    }
    f.engine.Apply({Parameter::Bypass, 1.f});
    f.Settle();
    f.engine.Process(0.3f, -0.7f, l, r);
    assert(Near(l, 0.3f) && Near(r, -0.7f));
}

void StressAndSmoothing() {
    Fixture f;
    f.Settle();
    float l, r, previous = 0.25f;
    f.engine.Apply({Parameter::Level, 1.f});
    for(unsigned i = 0; i < 500; ++i) {
        f.engine.Process(1.f, 1.f, l, r);
        assert(l >= previous && l - previous < 0.001f);
        previous = l;
    }
    f.engine.Apply({Parameter::Mix, 1.f});
    f.engine.Apply({Parameter::Feedback, 1.f});
    for(unsigned i = 0; i < 480000; ++i) {
        if(i % 5000 == 0) f.engine.Apply({Parameter::Time, (i / 5000) % 2 ? 1.f : 0.f});
        const float input = i % 101 == 0 ? std::numeric_limits<float>::quiet_NaN() : 1.f;
        f.engine.Process(input, -input, l, r);
        assert(std::isfinite(l) && std::isfinite(r));
        assert(std::abs(l) <= 1.f && std::abs(r) <= 1.f);
    }
}

int main() {
    ParametersAndMidi();
    QueueBoundariesAndConcurrency();
    InitializationAndDryStereo();
    DelayImpulseAndBypass();
    StressAndSmoothing();
    std::cout << "PASS: MIDI/validation, queue/concurrency, init/stereo, impulse/bypass, stress/smoothing\n";
}
