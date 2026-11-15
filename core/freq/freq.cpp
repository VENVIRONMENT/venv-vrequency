// venv-vrequency — Freq: defaults, limits, names, and the sums behind the page's readouts (see freq.h).
#include "core/freq/freq.h"
#include "core/dsp.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace td {

namespace {
const char *const kWaveNames[kFreqWaves] = {"SINE", "TRI", "SQUARE", "SAW", "NOISE"};
const char *const kWaveKeys[kFreqWaves] = {"sine", "tri", "square", "saw", "noise"};
const char *const kModNames[kFreqMods] = {"OFF", "FM", "RING"};
const char *const kModKeys[kFreqMods] = {"off", "fm", "ring"};
int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
float fin(float v, float lo, float hi, float d) { return std::isfinite(v) ? clampf(v, lo, hi) : d; }
int gcd(int a, int b) { while (b) { int t = a % b; a = b; b = t; } return a; }
float cents(float a, float b) { return 1200.0f * std::fabs(std::log2(a / b)); }
} // namespace

FreqState::FreqState() {
    //            on     wave     Hz      level
    tone[0] = {true, FW_SINE, 220.0f, 0.8f};
    tone[1] = {true, FW_SINE, 221.5f, 0.8f};
    tone[2] = {false, FW_SINE, 330.0f, 0.6f};
    tone[3] = {false, FW_TRI, 440.0f, 0.5f};
}

void freqClamp(FreqState &s) {
    for (FreqTone &t : s.tone) {
        t.wave = clampi(t.wave, 0, kFreqWaves - 1);
        t.hz = fin(t.hz, kFreqHzMin, kFreqHzMax, 220);
        t.level = fin(t.level, 0, 1, 0.8f);
    }
    s.mod = clampi(s.mod, 0, kFreqMods - 1);
    s.modAmt = fin(s.modAmt, 0, 1, 0.5f);
    warpClamp(s.warp);
    s.delay = fin(s.delay, 0, 1, 0);
    s.rate = fin(s.rate, kFreqRateMin, kFreqRateMax, 4);
}

bool sameFreq(const FreqState &a, const FreqState &b) {
    for (int i = 0; i < kFreqTones; ++i) {
        const FreqTone &x = a.tone[i], &y = b.tone[i];
        if (x.on != y.on || x.wave != y.wave || x.hz != y.hz || x.level != y.level) return false;
    }
    return a.mod == b.mod && a.modAmt == b.modAmt && a.warp.drive == b.warp.drive && a.warp.fold == b.warp.fold &&
           a.warp.crush == b.warp.crush && a.warp.cutoff == b.warp.cutoff && a.delay == b.delay &&
           a.keys == b.keys && a.rate == b.rate;
}

const char *freqWaveName(int w) { return kWaveNames[clampi(w, 0, kFreqWaves - 1)]; }
const char *freqWaveKey(int w) { return kWaveKeys[clampi(w, 0, kFreqWaves - 1)]; }
int freqWaveFromKey(const char *k) {
    for (int i = 0; i < kFreqWaves; ++i) if (std::strcmp(k, kWaveKeys[i]) == 0) return i;
    return -1;
}
const char *freqModName(int m) { return kModNames[clampi(m, 0, kFreqMods - 1)]; }
const char *freqModKey(int m) { return kModKeys[clampi(m, 0, kFreqMods - 1)]; }
int freqModFromKey(const char *k) {
    for (int i = 0; i < kFreqMods; ++i) if (std::strcmp(k, kModKeys[i]) == 0) return i;
    return -1;
}

float freqBeat(const FreqState &s, int *oi, int *oj) {
    int idx[kFreqTones], n = 0;
    for (int i = 0; i < kFreqTones; ++i) if (s.tone[i].on && s.tone[i].level > 0) idx[n++] = i;
    for (int a = 1; a < n; ++a) // by pitch, lowest first
        for (int b = a; b > 0 && s.tone[idx[b]].hz < s.tone[idx[b - 1]].hz; --b) { int t = idx[b]; idx[b] = idx[b - 1]; idx[b - 1] = t; }
    for (int a = 0; a + 1 < n; ++a) {
        const float d = std::fabs(s.tone[idx[a + 1]].hz - s.tone[idx[a]].hz);
        if (d <= kFreqNear && d > 0.001f) {
            if (oi) *oi = idx[a];
            if (oj) *oj = idx[a + 1];
            return d;
        }
    }
    return 0;
}

int freqRatio(float f, float f1, int &p, int &q) {
    p = q = 1;
    if (!(f > 0) || !(f1 > 0)) return 0;
    for (int pass = 0; pass < 2; ++pass) {           // within a cent, then within 30 cents
        const float tol = pass == 0 ? 1.0f : 30.0f;
        for (int sum = 2; sum <= 16; ++sum)          // the simplest first
            for (int a = 1; a < sum; ++a) {
                const int b = sum - a;
                if (a > 8 || b > 8 || gcd(a, b) != 1) continue;
                if (cents(f / f1, (float)a / b) <= tol) { p = a; q = b; return pass == 0 ? 2 : 1; }
            }
    }
    return 0;
}

float freqSnapRatio(float f, float f1) {
    static const float R[] = {1, 2, 1.5f, 4.0f / 3, 1.25f, 5.0f / 3, 1.2f, 1.6f, 3, 4, 2.5f, 1.75f, 6, 8};
    float best = f1, dist = 1e9f;
    for (float r : R)
        for (float inv : {1.0f, -1.0f}) {        // above tone 1, or the same ratio below it
            const float t = inv > 0 ? f1 * r : f1 / r;
            const float d = std::fabs(std::log2(f / t));
            if (d < dist && t >= kFreqHzMin && t <= kFreqHzMax) { dist = d; best = t; }
        }
    return best;
}

float freqSnapNote(float f) {
    const double m = std::round(69.0 + 12.0 * std::log2(f / 440.0));
    return clampf((float)(440.0 * std::pow(2.0, (m - 69.0) / 12.0)), kFreqHzMin, kFreqHzMax);
}

void freqNoteName(float f, char *out, int size) {
    static const char *N[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const int m = (int)std::lround(69.0 + 12.0 * std::log2(std::fmax(f, 1.0f) / 440.0));
    std::snprintf(out, size, "%s%d", N[((m % 12) + 12) % 12], m / 12 - 1);
}

void freqMoveRoot(FreqState &s, float hz) {
    hz = fin(hz, kFreqHzMin, kFreqHzMax, s.tone[0].hz);
    const float old = s.tone[0].hz;
    for (int i = 1; i < kFreqTones; ++i) {
        FreqTone &t = s.tone[i];
        const float d = t.hz - old;
        t.hz = clampf(std::fabs(d) <= kFreqNear ? hz + d : t.hz * hz / old, kFreqHzMin, kFreqHzMax);
    }
    s.tone[0].hz = hz;
}

} // namespace td
