// venv-vrequency — the scope's eyes: reads the newest samples, measures the frequency, finds the
// trigger and ranges the time and voltage scales (see scope.h).
#include "app/pages/synth/scope.h"
#include "audio/audio.h"
#include <QtGlobal>
#include <cmath>

using namespace td;

Scope::Scope(Shell &s) : Part(s), buf(kScopeSize, 0.0f) {}

// Frequency: autocorrelation of the last 85 ms (decimated by four). Counting zero crossings
// misreads any wave with more than one wiggle per cycle. Returns the period in samples.
float Scope::measureFrequency(const View &v) {
    measFreq = 0;
    float period = 0;
    if (v.span <= 0.004f) return period;
    const float sr = (float)sh.audio.sampleRate();
    static float d[1024];
    const float *x = buf.data() + v.total - 4096;
    float mean = 0;
    for (int i = 0; i < 1024; ++i) { d[i] = (x[4 * i] + x[4 * i + 1] + x[4 * i + 2] + x[4 * i + 3]) * 0.25f; mean += d[i]; }
    mean /= 1024;
    for (int i = 0; i < 1024; ++i) d[i] -= mean;
    static float ac[402];
    float r0 = 1e-12f;
    for (int i = 0; i < 1024; ++i) r0 += d[i] * d[i];
    float best = 0;
    for (int lag = 5; lag <= 401; ++lag) {
        float a = 0;
        const int n = 1024 - lag;
        for (int i = 0; i < n; ++i) a += d[i] * d[i + lag];
        ac[lag] = a * 1024.0f / (n * r0); // unbiased, normalised
        if (lag >= 6 && lag <= 400 && ac[lag] > best) best = ac[lag];
    }
    if (best > 0.3f) {
        for (int lag = 6; lag <= 400; ++lag) // the shortest lag that is nearly as good: the true period, not a multiple
            if (ac[lag] >= 0.92f * best && ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1]) {
                float den = ac[lag - 1] - 2 * ac[lag] + ac[lag + 1];
                float frac = std::fabs(den) > 1e-9f ? 0.5f * (ac[lag - 1] - ac[lag + 1]) / den : 0;
                period = (lag + qBound(-0.5f, frac, 0.5f)) * 4.0f;
                measFreq = sr / period;
                break;
            }
    }
    return period;
}

// Auto-range: keep four to nine cycles and a comfortable height on screen. The scales step only
// after the picture has been wrong for a third of a second, so slides do not flicker.
void Scope::autoRange(const View &v, float period) {
    if (measFreq > 0 && period > 0) {
        float cycles = v.need / period;
        if (cycles < 2.5f || cycles > 12.0f) {
            if (++rangeT > 8) {
                float want = 5.5f * 1000.0f / measFreq / kDivsX; // ms per division
                int best = 0;
                for (int i = 1; i < kTimeSteps; ++i)
                    if (std::fabs(std::log(kMsPerDiv[i] / want)) < std::fabs(std::log(kMsPerDiv[best] / want))) best = i;
                tbIdx = best; rangeT = 0;
            }
        } else rangeT = 0;
    }
    if (v.span > 0.004f) {
        float pkDiv = measPk * 0.5f / kVoltsPerDiv[vdIdx];
        if (pkDiv > 1.9f || pkDiv < 0.55f) {
            if (++rangeV > 8) {
                vdIdx = kVoltSteps - 1;
                for (int i = 0; i < kVoltSteps; ++i)
                    if (measPk * 0.5f <= kVoltsPerDiv[i] * 1.6f) { vdIdx = i; break; }
                rangeV = 0;
            }
        } else rangeV = 0;
    }
}

Scope::View Scope::acquire() {
    View v;
    const float sr = (float)sh.audio.sampleRate();
    v.need = qBound(64, (int)(kMsPerDiv[tbIdx] * kDivsX * sr / 1000.0f), kScopeSize - 2048);
    v.total = qMin((int)kScopeSize, v.need + 6000);
    inst.readScope(buf.data(), v.total);
    float lo = 1e9f, hi = -1e9f;
    for (int i = v.total - v.need; i < v.total; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    v.span = hi - lo;
    period = measureFrequency(v);

    // trigger: rising zero crossing after a deep negative swing, at most once per cycle
    float hyst = qMax(0.003f, -lo * 0.55f);
    int trig = -1, lastX = -100000;
    bool armed = false;
    for (int i = 1; i < v.total; ++i) {
        if (buf[i] < -hyst) armed = true;
        else if (armed && buf[i - 1] < 0 && buf[i] >= 0) {
            armed = false;
            if (i - lastX < period * 0.8f) continue; // hold-off
            lastX = i;
            if (i >= v.need / 2 && i <= v.total - v.need / 2) trig = i;
        }
    }
    trigd = trig >= 0 && v.span > 0.004f;
    v.start = trigd ? trig - v.need / 2 : v.total - v.need;
    lo = 1e9f; hi = -1e9f;
    for (int i = v.start; i < v.start + v.need; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    measPk = qMax(0.0f, hi - lo);
    autoRange(v, period);
    return v;
}
