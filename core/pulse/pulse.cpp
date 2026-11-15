// venv-vrequency — pulse pads: names, defaults, limits and the tempo divisions SYNC snaps to.
#include "core/pulse/pulse.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace td {

namespace {
const char *const kNames[kPulseTypes] = {"CLICK", "METAL", "SNAP", "SCRATCH", "CLAP", "TOM", "SUB THUMP", "808"};
const char *const kKeys[kPulseTypes] = {"click", "metal", "snap", "scratch", "clap", "tom", "thump", "808"};
const char *const kLines[kPulseTypes] = {
    "A SHORT RINGING CLICK", "CLASHING SQUARES, HIGH AND BRIGHT", "A PITCHED BODY UNDER BRIGHT NOISE", "A TONE PUSHED AND PULLED, LIKE VINYL",
    "THREE QUICK NOISE BURSTS AND A TAIL", "A SINE THAT FALLS, STRUCK", "A LOW SINE THAT DROPS IN PITCH", "A LONG TUNED BOOM"};
int clampType(int t) { return t < 0 ? 0 : (t >= kPulseTypes ? kPulseTypes - 1 : t); }
} // namespace

PulsePad pulseDefault(int pad) {
    //                       type      tone    length  rate  level
    static const PulsePad D[kPulsePads] = {
        {PT_CLICK, 3000, 3, 4, 0.8f},   {PT_METAL, 400, 80, 8, 0.7f},     {PT_SNAP, 190, 180, 2, 0.9f},
        {PT_SCRATCH, 330, 260, 2, 0.5f}, {PT_CLAP, 1200, 260, 2, 0.7f},   {PT_TOM, 140, 420, 4, 0.9f},
        {PT_THUMP, 45, 350, 1, 0.9f},   {PT_BOOM, 49, 1600, 0.5f, 0.75f}};
    return D[pad < 0 ? 0 : (pad >= kPulsePads ? kPulsePads - 1 : pad)];
}

void pulseClamp(PulsePad &p) {
    auto fin = [](float v, float lo, float hi, float dflt) { return std::isfinite(v) ? std::clamp(v, lo, hi) : dflt; };
    p.type = clampType(p.type);
    p.tone = fin(p.tone, kPulseToneMin, kPulseToneMax, 1000);
    p.lengthMs = fin(p.lengthMs, kPulseLenMin, kPulseLenMax, 100);
    p.rate = fin(p.rate, kPulseRateMin, kPulseRateMax, 4);
    p.level = fin(p.level, 0, 1, 0.8f);
    p.drive = fin(p.drive, 0, 1, 0); p.fold = fin(p.fold, 0, 1, 0); p.crush = fin(p.crush, 0, 1, 0);
    p.bend = fin(p.bend, -kPulseBendMax, kPulseBendMax, 0);
}

bool samePulse(const PulsePad &a, const PulsePad &b) {
    return a.type == b.type && a.tone == b.tone && a.lengthMs == b.lengthMs && a.rate == b.rate && a.level == b.level
        && a.drive == b.drive && a.fold == b.fold && a.crush == b.crush && a.bend == b.bend;
}

const char *pulseTypeName(int t) { return kNames[clampType(t)]; }
const char *pulseTypeKey(int t) { return kKeys[clampType(t)]; }
const char *pulseTypeLine(int t) { return kLines[clampType(t)]; }
int pulseTypeFromKey(const char *k) {
    for (int i = 0; i < kPulseTypes; ++i) if (std::strcmp(k, kKeys[i]) == 0) return i;
    return -1;
}
bool pulseContinuous(int) { return false; }
bool pulseUsesLength(int) { return true; }

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
