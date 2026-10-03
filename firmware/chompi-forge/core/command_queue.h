#pragma once
#include <atomic>
#include <cstddef>
#include "parameters.h"

namespace forge {
// Exactly one producer (main loop) and one consumer (audio callback).
// Full queue rejects the newest command, never overwrites unread data.
template<typename T, unsigned Capacity> class SpscQueue {
    static_assert(Capacity >= 2, "Queue needs a spare slot");
    static_assert(ATOMIC_INT_LOCK_FREE == 2, "Audio requires lock-free indices");
public:
#ifdef FORGE_TEST_HOOKS
    unsigned Size() const {
        const unsigned head=head_.load(std::memory_order_acquire), tail=tail_.load(std::memory_order_acquire);
        return (head+Capacity-tail)%Capacity;
    }
#endif
    bool Empty() const {
        return tail_.load(std::memory_order_acquire) == head_.load(std::memory_order_acquire);
    }
    bool HasSpace() const {
        return (head_.load(std::memory_order_relaxed) + 1) % Capacity
            != tail_.load(std::memory_order_acquire);
    }
    bool Push(const T& command) {
        const unsigned head = head_.load(std::memory_order_relaxed);
        const unsigned next = (head + 1) % Capacity;
        if(next == tail_.load(std::memory_order_acquire)) return false;
        entries_[head] = command;
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool Pop(T& command) {
        const unsigned tail = tail_.load(std::memory_order_relaxed);
        if(tail == head_.load(std::memory_order_acquire)) return false;
        command = entries_[tail];
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    T entries_[Capacity]{};
    std::atomic<unsigned> head_{0}, tail_{0};
};
template<unsigned Capacity> using CommandQueue = SpscQueue<Command, Capacity>;
} // namespace forge
