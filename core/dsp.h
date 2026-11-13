// venv vrequency — small DSP building blocks (header-only, allocation-free at run time).
#pragma once
#include <cmath>
#include <cstdint>

namespace td {

constexpr int kBlock = 32; // control-rate block size in samples
constexpr float kPi = 3.14159265358979f;

// Flush denormals to zero on the calling thread (reverb/filter tails otherwise
// decay into very slow subnormal arithmetic).
inline void enableFlushToZero() {
#if defined(__aarch64__)
    uint64_t fpcr;
    __asm__ __volatile__("mrs %0, fpcr" : "=r"(fpcr));
    fpcr |= (1ull << 24);
    __asm__ __volatile__("msr fpcr, %0" : : "r"(fpcr));
#elif defined(__SSE__)
    __builtin_ia32_ldmxcsr(__builtin_ia32_stmxcsr() | 0x8040);
#endif
}

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// sin(2*pi*ph) for ph in [0,1) — parabolic approximation, error < 0.1 %.
inline float fastSin(float ph) {
    float x = ph - 0.5f;
    float y = x * (8.0f - 16.0f * std::fabs(x));
    y = 0.225f * (y * std::fabs(y) - y) + y;
    return -y;
}

inline float fastTanh(float x) {
    if (x > 3.0f) return 1.0f;
    if (x < -3.0f) return -1.0f;
    float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

inline float polyBlep(float t, float dt) {
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}

// Band-limited oscillator sample. ph in [0,1), dt = phase increment per sample.
inline float oscSample(int wave, float ph, float dt, float pw) {
    switch (wave) {
    case 0: return fastSin(ph);
    case 1: return 1.0f - 4.0f * std::fabs(ph - 0.5f);
    case 2: return 2.0f * ph - 1.0f - polyBlep(ph, dt);
    default: {
        float v = ph < pw ? 1.0f : -1.0f;
        v += polyBlep(ph, dt);
        float t2 = ph - pw;
        if (t2 < 0) t2 += 1.0f;
        v -= polyBlep(t2, dt);
        return v - (2.0f * pw - 1.0f); // remove DC of asymmetric pulse
    }
    }
}

struct Rng {
    uint32_t s = 0x1234567u;
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float bipolar() { return (int32_t)next() * (1.0f / 2147483648.0f); }
    inline float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }
};

// Exponential-segment ADSR. Coefficients are per-sample one-pole rates.
struct Env {
    enum State { IDLE, ATTACK, DECAY, RELEASE };
    State st = IDLE;
    float v = 0;

    static float attackCoef(float seconds, float sr) {
        return 1.0f - std::exp(-1.466f / (seconds * sr));
    }
    static float decayCoef(float seconds, float sr) {
        return 1.0f - std::exp(-5.0f / (seconds * sr));
    }
    void gateOn() { st = ATTACK; }
    void gateOff() { if (st != IDLE) st = RELEASE; }
    void reset() { st = IDLE; v = 0; }
    bool active() const { return st != IDLE; }

    inline float tick(float aC, float dC, float sus, float rC) {
        switch (st) {
        case ATTACK:
            v += aC * (1.3f - v);
            if (v >= 1.0f) { v = 1.0f; st = DECAY; }
            break;
        case DECAY:
            v += dC * (sus - v);
            break;
        case RELEASE:
            v -= rC * v;
            if (v < 1e-4f) { v = 0; st = IDLE; }
            break;
        default: break;
        }
        return v;
    }
};

// Zero-delay-feedback state variable filter (Zavalishin/Cytomic form):
// stable under fast cutoff modulation.
struct Svf {
    float ic1 = 0, ic2 = 0;
    float a1 = 0, a2 = 0, a3 = 0, k = 1;
    void reset() { ic1 = ic2 = 0; }
    void set(float cutoffHz, float reso, float sr) {
        float fc = clampf(cutoffHz, 20.0f, sr * 0.45f);
        float g = std::tan(kPi * fc / sr);
        k = 2.0f - 2.0f * clampf(reso, 0.0f, 0.97f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    inline float tick(float x, int mode) {
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        if (mode == 0) return v2;
        if (mode == 1) return x - k * v1 - v2;
        return v1 * (0.6f + 0.4f * k);
    }
};

struct Smooth {
    float v = 0;
    inline float to(float target, float coef) { v += coef * (target - v); return v; }
};

} // namespace td
