// venv-vrequency — pulse pads: eight short, exact percussive sounds built from sines, squares and noise.
//
// Every pad is one of eight kinds and has four numbers. What TONE means depends on the kind;
// LENGTH is how long the hit lasts (its decay is scaled to fit):
//   CLICK      a damped cosine: TONE is the click's ring (Hz)
//   METAL      six square waves at clashing ratios through a high band, like a hi-hat: TONE is
//              the lowest square (the band sits twenty times above it)
//   SNAP       a short pitched body (TONE, dropping from twice it) under a burst of bright noise
//              (band at 25 times TONE), like a snare
//   SCRATCH    a turntable scratch: a tone with its second and third harmonics, plus a little
//              grit, pushed forward then pulled back (two strokes over LENGTH); TONE is the
//              pitch at normal speed
//   CLAP       band-passed noise (band at TONE) in three quick bursts 11 ms apart, then a tail
//   TOM        an 808 tom: a sine falling from 2.5 times TONE to TONE, with a short noise strike
//   SUB THUMP  a low sine that falls from four times TONE to TONE in about 15 ms
//   808        a long, tuned sub boom: a sine gliding down from twice TONE, softly saturated
// RATE sets how often a latched pad repeats. LEVEL is 0..1.
// Warp, over every kind: DRIVE and FOLD (0..1), CRUSH (0 off .. 1: fewer bits and a lower rate, the
// Freq page's crush) and BEND (semitones the pitch, or a noise kind's band, moves across the hit).
#pragma once

namespace td {

enum PulseType { PT_CLICK, PT_METAL, PT_SNAP, PT_SCRATCH, PT_CLAP, PT_TOM, PT_THUMP, PT_BOOM, kPulseTypes };
enum PulseField { PF_TYPE, PF_TONE, PF_LENGTH, PF_RATE, PF_LEVEL, PF_DRIVE, PF_FOLD, PF_CRUSH, PF_BEND, PF_COUNT };
constexpr int kPulsePads = 8;

constexpr float kPulseToneMin = 20, kPulseToneMax = 18000;  // Hz
constexpr float kPulseLenMin = 1, kPulseLenMax = 2000;      // ms
constexpr float kPulseRateMin = 0.25f, kPulseRateMax = 200; // Hz
constexpr float kPulseBendMax = 24;                         // semitones, either way

struct PulsePad {
    int type = PT_CLICK;
    float tone = 1000;      // Hz (meaning per kind, above)
    float lengthMs = 100;
    float rate = 4;         // Hz
    float level = 0.8f;     // 0..1
    float drive = 0, fold = 0, crush = 0; // 0..1: the warp, off by default
    float bend = 0;         // semitones across the hit
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
