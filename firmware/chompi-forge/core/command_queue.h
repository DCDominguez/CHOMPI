#pragma once
#include <atomic>
#include <cstring>
#include <type_traits>
#include <cstddef>
#include "parameters.h"

namespace forge {
// Exactly one producer (main loop) and one consumer (audio callback).
// Full queue rejects the newest command, never overwrites unread data.
// Entries are raw bytes copied in and out (T must be trivially copyable), so a queue
// is all zeros at start-up: it lives in .bss. With `T entries_[]{}` every entry carried
// its default values (a full patch per request) as a .data image in SRAM_EXEC, 28 KB
// in 0.13 (docs/forge/RESOURCE_LEDGER.md, first optimisation).
template<typename T, unsigned Capacity> class SpscQueue {
    static_assert(Capacity >= 2, "Queue needs a spare slot");
    static_assert(std::is_trivially_copyable<T>::value, "Queue entries are copied as bytes");
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
        std::memcpy(storage_ + head * sizeof(T), &command, sizeof(T));
        head_.store(next, std::memory_order_release);
        return true;
    }
    bool Pop(T& command) {
        const unsigned tail = tail_.load(std::memory_order_relaxed);
        if(tail == head_.load(std::memory_order_acquire)) return false;
        std::memcpy(&command, storage_ + tail * sizeof(T), sizeof(T));
        tail_.store((tail + 1) % Capacity, std::memory_order_release);
        return true;
    }
private:
    alignas(T) unsigned char storage_[Capacity * sizeof(T)] = {};
    std::atomic<unsigned> head_{0}, tail_{0};
};
template<unsigned Capacity> using CommandQueue = SpscQueue<Command, Capacity>;
} // namespace forge
