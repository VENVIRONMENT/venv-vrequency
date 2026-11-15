// venv-vrequency — pulse pads: eight short, exact sounds (clicks, ticks, noise, pure tones, pulses).
//
// Every pad is one of eight kinds and has four numbers. What TONE means depends on the kind:
//   CLICK        a damped cosine: TONE is the click's ring (Hz); LENGTH its decay
//   TICK         1 to 3 samples at full height: TONE sets the width (16 kHz and up 1 sample,
//                about 12 kHz 2, 8 kHz and below 3); LENGTH is not used
//   NOISE BURST  white noise with a fast decay: TONE is a low-pass on the noise
//   SINE BLIP    a pure sine for LENGTH: TONE is its pitch
//   GATED TONE   a pure sine switched on and off RATE times a second: TONE is its pitch
//   PULSE TRAIN  clicks RATE times a second for LENGTH: TONE is each click's ring
//   SUB THUMP    a low sine that falls from four times TONE to TONE in about 15 ms
//   HISS         band-passed noise that swells and fades over LENGTH: TONE is the band
// RATE also sets how often a latched one-shot repeats. LEVEL is 0..1.
#pragma once

namespace td {

enum PulseType { PT_CLICK, PT_TICK, PT_NOISE, PT_BLIP, PT_GATED, PT_TRAIN, PT_THUMP, PT_HISS, kPulseTypes };
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
bool pulseContinuous(int type);            // gated tone and pulse train: latched, they run without end
bool pulseUsesLength(int type);            // every kind but the tick

// SYNC: the rate moved to the nearest whole division of the beat (in octaves). `perBeat` gets the
// hits per beat it chose (1 = quarter notes, 4 = sixteenths, 0.25 = once a bar).
float pulseSyncRate(float rate, float bpm, float *perBeat = nullptr);

} // namespace td
