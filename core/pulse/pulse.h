// venv-vrequency — pulse pads: eight short, exact percussive sounds built from sines, squares and noise.
//
// Every pad is one of eight kinds and has four numbers. What TONE means depends on the kind;
// LENGTH is how long the hit lasts (its decay is scaled to fit):
//   CLICK      a damped cosine: TONE is the click's ring (Hz)
//   METAL      six square waves at clashing ratios through a high band, like a hi-hat: TONE is
//              the lowest square (the band sits twenty times above it)
//   SNAP       a short pitched body (TONE, dropping from twice it) under a burst of bright noise
//              (band at 25 times TONE), like a snare
//   FM PING    a sine carrier at TONE, phase-modulated at 1.4 times TONE by an index that fades
//              faster than the ping: a glassy, bell-like strike
//   CLAP       band-passed noise (band at TONE) in three quick bursts 11 ms apart, then a tail
//   ZAP        a sine that sweeps down from 25 times TONE to TONE: a laser-like drop
//   SUB THUMP  a low sine that falls from four times TONE to TONE in about 15 ms
//   KNOCK      two sine modes (TONE and 2.7 times TONE) with a tiny pitch drop: wood or a rim
// RATE sets how often a latched pad repeats. LEVEL is 0..1.
#pragma once

namespace td {

enum PulseType { PT_CLICK, PT_METAL, PT_SNAP, PT_PING, PT_CLAP, PT_ZAP, PT_THUMP, PT_KNOCK, kPulseTypes };
enum PulseField { PF_TYPE, PF_TONE, PF_LENGTH, PF_RATE, PF_LEVEL, PF_COUNT };
constexpr int kPulsePads = 8;

constexpr float kPulseToneMin = 20, kPulseToneMax = 18000;  // Hz
constexpr float kPulseLenMin = 1, kPulseLenMax = 2000;      // ms
constexpr float kPulseRateMin = 0.25f, kPulseRateMax = 200; // Hz

struct PulsePad {
    int type = PT_CLICK;
    float tone = 1000;      // Hz (meaning per kind, above)
    float lengthMs = 100;
    float rate = 4;         // Hz
    float level = 0.8f;     // 0..1
};

PulsePad pulseDefault(int pad);            // pad n is the n-th kind, with values that suit it
void pulseClamp(PulsePad &p);
bool samePulse(const PulsePad &a, const PulsePad &b);
const char *pulseTypeName(int type);       // "CLICK", "NOISE BURST"...
const char *pulseTypeKey(int type);        // "click", "noise"...: for files and the control socket
const char *pulseTypeLine(int type);       // one short line on what it does
int pulseTypeFromKey(const char *key);     // -1 when unknown
bool pulseContinuous(int type);            // a kind that runs without end when latched (none at present)
bool pulseUsesLength(int type);            // every kind

// SYNC: the rate moved to the nearest whole division of the beat (in octaves). `perBeat` gets the
// hits per beat it chose (1 = quarter notes, 4 = sixteenths, 0.25 = once a bar).
float pulseSyncRate(float rate, float bpm, float *perBeat = nullptr);

} // namespace td
