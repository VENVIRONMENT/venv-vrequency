// venv-vrequency — the lock-free ring that carries events and patches to the audio thread.
#pragma once
#include <atomic>
#include <cstdint>

namespace td {

template <typename T, int N> // single-producer single-consumer ring, N power of two
class Spsc {
    T buf[N];
    std::atomic<uint32_t> w{0}, r{0};
public:
    bool push(const T &v) {
        uint32_t wi = w.load(std::memory_order_relaxed);
        if (wi - r.load(std::memory_order_acquire) >= (uint32_t)N) return false;
        buf[wi & (N - 1)] = v;
        w.store(wi + 1, std::memory_order_release);
        return true;
    }
    bool pop(T &v) {
        uint32_t ri = r.load(std::memory_order_relaxed);
        if (ri == w.load(std::memory_order_acquire)) return false;
        v = buf[ri & (N - 1)];
        r.store(ri + 1, std::memory_order_release);
        return true;
    }
};

} // namespace td
