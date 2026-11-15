// venv-vrequency — the pulse generator (see pulse_voice.h). Allocation-free; safe on the audio thread.
#include "core/pulse/pulse_voice.h"
#include <algorithm>
#include <cmath>

namespace td {

namespace {
constexpr double kTwoPi = 6.283185307179586;
constexpr uint32_t kSeed = 0x5EED1234u;
inline float sine(double ph) { return (float)std::sin(kTwoPi * ph); }
inline double wrap(double ph) { return ph >= 1.0 ? ph - 1.0 : ph; }
} // namespace

int PulseVoice::tickWidth(float tone, float sr) {
    return std::clamp((int)(0.5f * sr / std::max(tone, 1.0f)), 1, 3);
}

int PulseVoice::lengthSamples(const PulsePad &p, float sr) {
    if (p.type == PT_TICK) return tickWidth(p.tone, sr);
    return std::max(1, (int)std::lround(p.lengthMs * 0.001 * sr));
}

void PulseVoice::setFilters() {
    const float fc = std::min(tone, 0.45f * sr);
    lpA = 1.0f - std::exp(-2.0f * kPi * fc / sr);              // noise burst: one pole
    lpNorm = std::min(8.0f, std::sqrt((2.0f - lpA) / lpA));    // keeps its loudness when the tone is low
    const float g = std::tan(kPi * fc / sr), Q = 2.0f;          // hiss: a band-pass (TPT state-variable filter)
    svK = 1.0f / Q;
    svA1 = 1.0f / (1.0f + g * (g + svK)); svA2 = g * svA1; svA3 = g * svA2;
    svNorm = std::min(12.0f, std::sqrt(Q * sr / (kPi * fc)));
    gateK = 1.0f - std::exp(-1.0f / (0.0003f * sr));           // gate edges of about a third of a millisecond
    clickTau = std::min(3.0f / fc, 0.2f / rate);               // one click of a train: three cycles of its ring
}

void PulseVoice::start(int pad, const PulsePad &p0, float sampleRate, float rateHz, bool endlessRun, int delay) {
    PulsePad p = p0;
    pulseClamp(p);
    on = true; runOn = endlessRun && pulseContinuous(p.type); fading = false;
    padNo = pad; type = p.type; sr = sampleRate;
    pos = 0; wait = std::max(0, delay); relGain = 1;
    tone = std::min(p.tone, 0.45f * sr); level = p.level;
    rate = std::clamp(rateHz, kPulseRateMin, kPulseRateMax);
    len = lengthSamples(p, sr);
    fadeLen = std::max(1, std::min((int)(0.001f * sr), len / 4));
    ph = 0; gatePh = 0; gate = 0; clickAt = 0; lp = 0; ic1 = ic2 = 0;
    rng.s = kSeed;
    setFilters();
    decay = 1;
    const float sec = len / sr;
    const float tau = type == PT_CLICK ? sec / 5.0f : (type == PT_TRAIN ? clickTau : sec / 4.0f);
    decayCoef = std::exp(-1.0f / (std::max(tau, 1e-5f) * sr));
}

void PulseVoice::retune(const PulsePad &p0, float rateHz) {
    PulsePad p = p0;
    pulseClamp(p);
    if (p.type != type) { release(); return; }   // another kind: this one fades, the latch starts the new one
    tone = std::min(p.tone, 0.45f * sr); level = p.level;
    rate = std::clamp(rateHz, kPulseRateMin, kPulseRateMax);
    setFilters();
    if (type == PT_TRAIN) decayCoef = std::exp(-1.0f / (std::max(clickTau, 1e-5f) * sr));
}

void PulseVoice::release(float ms) {
    if (!on) return;
    fading = true;
    relCoef = std::exp(-1.0f / (std::max(ms, 0.1f) * 0.001f * sr / 4.6f));
}

float PulseVoice::next(bool &onset, int &dur) {
    onset = false; dur = 0;
    if (!on) return 0;
    if (wait > 0) { --wait; return 0; }
    if (!runOn && pos >= len) { on = false; return 0; }
    if (pos == 0) { onset = true; dur = type == PT_CLICK || type == PT_TICK ? 0 : len; }
    float y = 0;
    switch (type) {
    case PT_CLICK: // a cosine starts at its peak: the first sample is the impulse, the ring follows
        y = (float)std::cos(kTwoPi * ph) * decay; decay *= decayCoef; ph = wrap(ph + tone / sr);
        break;
    case PT_TICK:
        y = 1.0f;
        break;
    case PT_NOISE:
        lp += lpA * (rng.bipolar() - lp);
        y = clampf(lp * lpNorm * decay, -1, 1); decay *= decayCoef;
        break;
    case PT_BLIP:
        y = sine(ph) * std::min(1.0f, pos / (0.0005f * sr)); ph = wrap(ph + tone / sr);
        break;
    case PT_GATED: {
        if (pos == 0 || gatePh < rate / sr) { onset = true; dur = (int)(0.5f * sr / rate); }
        gate += gateK * ((gatePh < 0.5 ? 1.0f : 0.0f) - gate);
        y = sine(ph) * gate;
        ph = wrap(ph + tone / sr); gatePh = wrap(gatePh + rate / sr);
        break;
    }
    case PT_TRAIN:
        if (pos == 0 || gatePh < rate / sr) { onset = true; dur = 0; decay = 1; clickAt = 0; }
        y = (float)std::cos(kTwoPi * tone * clickAt / sr) * decay;
        decay *= decayCoef; ++clickAt;
        gatePh = wrap(gatePh + rate / sr);
        break;
    case PT_THUMP: { // the pitch falls from four times the tone to the tone, time constant 15 ms
        const float drop = std::exp(-(pos / sr) / 0.015f);
        y = sine(ph) * decay; decay *= decayCoef;
        ph = wrap(ph + tone * (1.0f + 3.0f * drop) / sr);
        break;
    }
    case PT_HISS: { // swells for the first 35 % of its length, then fades
        const float v0 = rng.bipolar() * 0.6f, v3 = v0 - ic2;
        const float v1 = svA1 * ic1 + svA2 * v3, v2 = ic2 + svA2 * ic1 + svA3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        const float x = pos / (float)len;
        const float s = x < 0.35f ? std::sin(0.5f * kPi * x / 0.35f) : std::cos(0.5f * kPi * (x - 0.35f) / 0.65f);
        y = clampf(svK * v1 * svNorm, -1, 1) * s * s;
        break;
    }
    }
    if (!runOn && type != PT_TICK) { // a short fade at the end, so every hit stops in silence
        const int left = len - pos;
        if (left < fadeLen) y *= (float)left / fadeLen;
    }
    if (fading) {
        relGain *= relCoef;
        if (relGain < 1e-4f) on = false;
    }
    ++pos;
    if (runOn && pos > (1 << 30)) pos = 1 << 29; // an endless voice never runs its counter out
    return y * level * relGain;
}

int pulseRender(const PulsePad &p, float sr, float rateHz, float *out, int n) {
    PulseVoice v;
    v.start(0, p, sr, rateHz, false);
    int onsets = 0;
    bool on; int dur;
    for (int i = 0; i < n; ++i) {
        out[i] = v.next(on, dur);
        if (on) ++onsets;
    }
    return onsets;
}

} // namespace td
