// venv-vrequency — pulse pads: names, defaults, limits and the tempo divisions SYNC snaps to.
#include "core/pulse/pulse.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace td {

namespace {
const char *const kNames[kPulseTypes] = {"CLICK", "TICK", "NOISE BURST", "SINE BLIP", "GATED TONE", "PULSE TRAIN", "SUB THUMP", "HISS"};
const char *const kKeys[kPulseTypes] = {"click", "tick", "noise", "blip", "gated", "train", "thump", "hiss"};
const char *const kLines[kPulseTypes] = {
    "A SHORT RINGING CLICK", "ONE TO THREE SAMPLES, NOTHING ELSE", "WHITE NOISE THAT DIES AT ONCE", "A SHORT PURE SINE",
    "A SINE SWITCHED ON AND OFF", "CLICKS AT A STEADY RATE", "A LOW SINE THAT DROPS IN PITCH", "NOISE THAT SWELLS AND FADES"};
int clampType(int t) { return t < 0 ? 0 : (t >= kPulseTypes ? kPulseTypes - 1 : t); }
} // namespace

PulsePad pulseDefault(int pad) {
    //                       type      tone    length  rate  level
    static const PulsePad D[kPulsePads] = {
        {PT_CLICK, 3000, 3, 4, 0.8f},   {PT_TICK, 12000, 1, 8, 0.7f},     {PT_NOISE, 9000, 60, 2, 0.6f},
        {PT_BLIP, 1000, 50, 2, 0.5f},   {PT_GATED, 2000, 500, 12, 0.4f},  {PT_TRAIN, 1000, 250, 40, 0.8f},
        {PT_THUMP, 45, 350, 1, 0.9f},   {PT_HISS, 6000, 1500, 0.5f, 0.45f}};
    return D[pad < 0 ? 0 : (pad >= kPulsePads ? kPulsePads - 1 : pad)];
}

void pulseClamp(PulsePad &p) {
    auto fin = [](float v, float lo, float hi, float dflt) { return std::isfinite(v) ? std::clamp(v, lo, hi) : dflt; };
    p.type = clampType(p.type);
    p.tone = fin(p.tone, kPulseToneMin, kPulseToneMax, 1000);
    p.lengthMs = fin(p.lengthMs, kPulseLenMin, kPulseLenMax, 100);
    p.rate = fin(p.rate, kPulseRateMin, kPulseRateMax, 4);
    p.level = fin(p.level, 0, 1, 0.8f);
}

bool samePulse(const PulsePad &a, const PulsePad &b) {
    return a.type == b.type && a.tone == b.tone && a.lengthMs == b.lengthMs && a.rate == b.rate && a.level == b.level;
}

const char *pulseTypeName(int t) { return kNames[clampType(t)]; }
const char *pulseTypeKey(int t) { return kKeys[clampType(t)]; }
const char *pulseTypeLine(int t) { return kLines[clampType(t)]; }
int pulseTypeFromKey(const char *k) {
    for (int i = 0; i < kPulseTypes; ++i) if (std::strcmp(k, kKeys[i]) == 0) return i;
    return -1;
}
bool pulseContinuous(int t) { return t == PT_GATED || t == PT_TRAIN; }
bool pulseUsesLength(int t) { return t != PT_TICK; }

float pulseSyncRate(float rate, float bpm, float *perBeat) {
    static const float K[] = {0.0625f, 0.125f, 0.25f, 0.5f, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128};
    const float beatHz = std::max(bpm, 1.0f) / 60.0f;
    float best = K[0], dist = 1e9f;
    for (float k : K) {
        if (k * beatHz > kPulseRateMax * 1.05f) break;
        float d = std::fabs(std::log2(std::max(rate, 1e-3f) / (k * beatHz)));
        if (d < dist) { dist = d; best = k; }
    }
    if (perBeat) *perBeat = best;
    return best * beatHz;
}

} // namespace td
