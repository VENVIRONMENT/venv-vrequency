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

int PulseVoice::lengthSamples(const PulsePad &p, float sr) {
    return std::max(1, (int)std::lround(p.lengthMs * 0.001 * sr));
}

// The band-pass (a TPT state-variable filter) that metal, snap and clap colour their source with.
void PulseVoice::setFilters() {
    float fc = tone, Q = 1.0f;
    if (type == PT_METAL) { fc = tone * 20.0f; Q = 1.2f; }
    else if (type == PT_SNAP) { fc = tone * 25.0f; Q = 0.8f; }
    else if (type == PT_CLAP) { fc = tone; Q = 1.5f; }
    else if (type == PT_SCRATCH) { fc = tone * 6.0f; Q = 1.0f; }
    fc = std::clamp(fc, 20.0f, 0.45f * sr);
    const float g = std::tan(kPi * fc / sr);
    svK = 1.0f / Q;
    svA1 = 1.0f / (1.0f + g * (g + svK)); svA2 = g * svA1; svA3 = g * svA2;
    svNorm = std::min(12.0f, std::sqrt(Q * sr / (kPi * fc))); // white noise in, about the same loudness out
    hpA = std::exp(-2.0f * kPi * std::min(tone * 10.0f, 0.4f * sr) / sr); // metal: cuts below ten times TONE
}

float PulseVoice::band(float x) {
    const float v3 = x - ic2;
    const float v1 = svA1 * ic1 + svA2 * v3, v2 = ic2 + svA2 * ic1 + svA3 * v3;
    ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
    return svK * v1;
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
    ph = 0; ic1 = ic2 = 0;
    for (float &h : hp) h = 0;
    for (double &q : sq) q = 0;
    rng.s = kSeed;
    setFilters();
    const float sec = len / sr;
    float tau = sec / 4.0f, tau2 = sec / 4.0f; // each kind's decays, fitted to its LENGTH
    switch (type) {
    case PT_CLICK: tau = sec / 5.0f; break;
    case PT_METAL: tau = sec / 5.0f; break;
    case PT_SNAP: tau = sec / 6.0f; tau2 = sec / 3.5f; break;
    case PT_TOM: tau = sec / 4.0f; tau2 = 0.002f; break;
    case PT_CLAP: tau = sec / 3.5f; break;
    case PT_BOOM: tau = sec / 3.0f; break;
    default: break;
    }
    decay = decay2 = 1;
    decayCoef = std::exp(-1.0f / (std::max(tau, 1e-5f) * sr));
    decay2Coef = std::exp(-1.0f / (std::max(tau2, 1e-5f) * sr));
}

void PulseVoice::retune(const PulsePad &p0, float rateHz) {
    PulsePad p = p0;
    pulseClamp(p);
    if (p.type != type) { release(); return; }   // another kind: this one fades, the latch starts the new one
    tone = std::min(p.tone, 0.45f * sr); level = p.level;
    rate = std::clamp(rateHz, kPulseRateMin, kPulseRateMax);
    setFilters();
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
    if (pos == 0) { onset = true; dur = type == PT_CLICK ? 0 : len; }
    const float t = pos / sr; // seconds into the hit
    float y = 0;
    switch (type) {
    case PT_CLICK: // a cosine starts at its peak: the first sample is the impulse, the ring follows
        y = (float)std::cos(kTwoPi * ph) * decay; ph = wrap(ph + tone / sr);
        break;
    case PT_METAL: { // the 808's six clashing ratios, as squares, through a high band
        static const float R[6] = {1.0f, 1.3420f, 1.2312f, 1.6532f, 1.9523f, 2.1523f};
        float x = 0;
        for (int k = 0; k < 6; ++k) { x += sq[k] < 0.5 ? 1.0f : -1.0f; sq[k] = wrap(sq[k] + std::min(tone * R[k], 0.45f * sr) / sr); }
        float b = band(x / 6.0f);
        for (int k = 0; k < 2; ++k) { const float o = hpA * (hp[2 * k + 1] + b - hp[2 * k]); hp[2 * k] = b; hp[2 * k + 1] = o; b = o; }
        y = clampf(b * svNorm * 6.0f, -1, 1) * decay;
        break;
    }
    case PT_SNAP: { // a body dropping from twice TONE in 8 ms, under bright noise
        const float body = sine(ph) * decay;
        ph = wrap(ph + tone * (1.0f + std::exp(-t / 0.008f)) / sr);
        const float hiss = clampf(band(rng.bipolar()) * svNorm * 0.7f, -1, 1) * decay2;
        y = 0.45f * body + 0.55f * hiss;
        decay2 *= decay2Coef;
        break;
    }
    case PT_SCRATCH: { // two strokes: forward (up to 1.6 times speed), then back (up to 1.2)
        const float x = pos / (float)len;
        const float speed = x < 0.5f ? 1.6f * std::sin(kPi * x / 0.5f) : 1.2f * std::sin(kPi * (x - 0.5f) / 0.5f);
        const double step = std::min(tone * speed, 0.15f * sr) / sr;
        const float voice = sine(ph) * 0.6f + sine(2 * ph) * 0.3f + sine(3 * ph) * 0.15f; // a rough, vocal-ish tone
        ph = wrap(ph + step);
        const float grit = clampf(band(rng.bipolar()) * svNorm * 0.5f, -1, 1);
        y = (voice + 0.25f * grit) * std::min(1.0f, speed * 1.3f);
        break;
    }
    case PT_CLAP: { // bursts at 0, 11 and 22 ms (3.5 ms each), then a tail from 33 ms
        const float x = clampf(band(rng.bipolar()) * svNorm, -1, 1);
        float env;
        if (t < 0.033f) env = std::exp(-(t - std::floor(t / 0.011f) * 0.011f) / 0.0035f);
        else { env = 0.75f * decay; decay *= decayCoef; }
        y = x * env;
        break;
    }
    case PT_TOM: { // falls from 2.5 times TONE in about 20 ms; a 2 ms noise strike on top
        y = 0.78f * sine(ph) * decay + 0.22f * rng.bipolar() * decay2;
        ph = wrap(ph + tone * (1.0f + 1.5f * std::exp(-t / 0.02f)) / sr);
        decay2 *= decay2Coef;
        break;
    }
    case PT_THUMP: { // the pitch falls from four times the tone to the tone, time constant 15 ms
        const float drop = std::exp(-t / 0.015f);
        y = sine(ph) * decay;
        ph = wrap(ph + tone * (1.0f + 3.0f * drop) / sr);
        break;
    }
    case PT_BOOM: { // glides down from twice TONE (40 ms), then rings long; soft saturation gives it weight
        const float x = sine(ph) * decay;
        y = std::tanh(1.8f * x) / std::tanh(1.8f);
        ph = wrap(ph + tone * (1.0f + std::exp(-t / 0.04f)) / sr);
        break;
    }
    }
    if (type != PT_CLAP) decay *= decayCoef;
    if (!runOn) { // a short fade at the end, so every hit stops in silence
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
