// venv-vrequency — the shared spectrum (see spectrum.h).
#include "app/pages/synth/spectrum.h"
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace spec {

namespace {
// The FFT's tables, made once: bit reversal, twiddles, Hann window.
struct Fft {
    static constexpr int N = kN;
    uint16_t rev[N];
    float cs[N / 2], sn[N / 2], win[N];
    Fft() {
        for (int i = 0; i < N; ++i) {
            int r = 0;
            for (int b = 0; (1 << b) < N; ++b) r |= ((i >> b) & 1) << (11 - b);
            rev[i] = (uint16_t)r;
            win[i] = 0.5f - 0.5f * std::cos(6.2831853f * i / N);
        }
        for (int i = 0; i < N / 2; ++i) { cs[i] = std::cos(6.2831853f * i / N); sn[i] = -std::sin(6.2831853f * i / N); }
    }
    void run(const float *x, float *re, float *im) const { // x: N samples, in; re/im: N, out
        for (int i = 0; i < N; ++i) { re[rev[i]] = x[i] * win[i]; im[rev[i]] = 0; }
        for (int len = 2; len <= N; len <<= 1) {
            const int half = len / 2, step = N / len;
            for (int a = 0; a < N; a += len)
                for (int k = 0; k < half; ++k) {
                    const float wr = cs[k * step], wi = sn[k * step];
                    const int p = a + k, q = p + half;
                    const float tr = re[q] * wr - im[q] * wi, ti = re[q] * wi + im[q] * wr;
                    re[q] = re[p] - tr; im[q] = im[p] - ti; re[p] += tr; im[p] += ti;
                }
        }
    }
};
} // namespace

void dbSpectrum(const float *x, std::vector<float> &db) {
    static const Fft fft;
    static float re[kN], im[kN];
    db.resize(kN / 2 + 2);
    fft.run(x, re, im);
    for (int i = 0; i <= kN / 2; ++i) // a full-scale sine reads kN/4 through the window
        db[i] = std::max(kDbLo, 20.0f * std::log10(std::sqrt(re[i] * re[i] + im[i] * im[i]) * (4.0f / kN) + 1e-6f));
    db[kN / 2 + 1] = db[kN / 2];
}

void LogTable::make(int count, float rate) {
    if (n == count && sr == rate) return;
    n = count; sr = rate; lo.resize(n); hi.resize(n);
    const float bw = rate / kN, r = std::pow(kHiHz / kLoHz, 1.0f / (n - 1));
    for (int i = 0; i < n; ++i) {
        const float f = kLoHz * std::pow(r, (float)i);
        lo[i] = f / std::sqrt(r) / bw; hi[i] = f * std::sqrt(r) / bw;
    }
}

float LogTable::at(const float *dbv, int i) const {
    if (hi[i] - lo[i] < 1.0f) {
        const float c = 0.5f * (lo[i] + hi[i]);
        const int b = (int)c;
        return dbv[b] + (dbv[b + 1] - dbv[b]) * (c - b);
    }
    float m = kDbLo;
    for (int b = (int)std::ceil(lo[i]); b <= (int)hi[i]; ++b) m = std::max(m, dbv[b]);
    return m;
}

qreal xOfHz(float f, qreal w) { return w * std::log(f / kLoHz) / std::log(kHiHz / kLoHz); }

float peakHz(const std::vector<float> &db, float sr, float *peakDb) {
    const int b0 = std::max(2, (int)std::ceil(kLoHz * kN / sr)), b1 = std::min(kN / 2 - 1, (int)(kHiHz * kN / sr));
    int best = b0;
    for (int b = b0; b <= b1; ++b) if (db[b] > db[best]) best = b;
    if (peakDb) *peakDb = db[best];
    if (db[best] <= -75.0f) return 0;
    const float a = db[best - 1], b = db[best], c = db[best + 1], den = a - 2 * b + c;
    const float off = std::fabs(den) > 1e-6f ? std::clamp(0.5f * (a - c) / den, -0.5f, 0.5f) : 0;
    return (best + off) * sr / kN;
}

} // namespace spec
