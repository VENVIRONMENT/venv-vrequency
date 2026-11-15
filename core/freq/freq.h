// venv-vrequency — Freq: up to four steady tones at exact frequencies, what they are set to, and
// the sums the Freq page shows about them (beat, ratio, nearest note, SNAP).
//
// A tone is ON or off, a wave, a frequency (Hz) and a level. Tone 2 can bend tone 1 ("2 > 1"):
// FM (tone 2 moves tone 1's phase) or RING (tone 1 times tone 2). The sum goes through a warp
// stage (core/fx/warp.h) and can be sent into the master delay.
//
// What makes the tones sound: HOLD (always), GATE (on and off at RATE, a square), or with KEYS on
// a held letter key (HOLD still holds them). GATE with HOLD or a held key pulses them.
#pragma once
#include "core/fx/warp.h"

namespace td {

constexpr int kFreqTones = 4;
enum FreqWave { FW_SINE, FW_TRI, FW_SQUARE, FW_SAW, FW_NOISE, kFreqWaves };
enum FreqMod { FM_OFF, FM_FM, FM_RING, kFreqMods };

// EV_FREQSET's fields. The first four belong to one tone (the event's `a`); the rest to the page.
enum FreqField {
    FF_ON, FF_WAVE, FF_HZ, FF_LEVEL,     // a tone's settings
    FF_NUDGE, FF_SEMI, FF_SNAP,          // a tone moved: by Hz, by semitones, to the nearest simple ratio / note
    FF_MOD, FF_MODAMT,                   // 2 > 1: FreqMod and its amount 0..1
    FF_FOLD, FF_FILTER, FF_DRIVE, FF_CRUSH, FF_DELAY,
    FF_HOLD, FF_GATE, FF_RATE, FF_KEYS,
    FF_KEYHELD,                          // KEYS: a letter key is down (1) or every one is up (0)
    FF_ROOT,                             // KEYS: tone 1 to this Hz; the others keep their offset or ratio
    FF_COUNT
};

constexpr float kFreqHzMin = 20, kFreqHzMax = 18000;
constexpr float kFreqRateMin = 0.25f, kFreqRateMax = 50;     // GATE, Hz
constexpr float kFreqNear = 20;                              // Hz: "close to tone 1" (offsets, beats)

struct FreqTone {
    bool on = true;
    int wave = FW_SINE;
    float hz = 220;
    float level = 0.8f;
};

struct FreqState {
    FreqTone tone[kFreqTones];
    int mod = FM_OFF;
    float modAmt = 0.5f;
    WarpParams warp;
    float delay = 0;            // send into the master delay, 0..1
    bool hold = false, gate = false, keys = false;
    float rate = 4;             // GATE, Hz (SYNC snaps it to the tempo)
    FreqState();
};

void freqClamp(FreqState &s);
bool sameFreq(const FreqState &a, const FreqState &b);       // every saved setting equal
const char *freqWaveName(int w);                             // "SINE"...
const char *freqWaveKey(int w);                              // "sine"...: files and the control socket
int freqWaveFromKey(const char *k);                          // -1 when unknown
const char *freqModName(int m);                              // "OFF", "FM", "RING"
const char *freqModKey(int m);
int freqModFromKey(const char *k);

// The beat the page shows: |f1 - f2| of the two lowest sounding (on, level > 0) tones that are
// within kFreqNear of each other. 0 when there is none; i and j get the tones.
float freqBeat(const FreqState &s, int *i = nullptr, int *j = nullptr);
// The simplest ratio p:q (both 1..8) near f / f1. 2: within a cent ("3:2"), 1: within 30 cents
// ("~3:2"), 0: none ("-").
int freqRatio(float f, float f1, int &p, int &q);
float freqSnapRatio(float f, float f1);                      // f moved to the nearest simple ratio of f1
float freqSnapNote(float f);                                 // f moved to the nearest equal-tempered note
void freqNoteName(float f, char *out, int size);             // "A3", "C#5"
// KEYS: tone 1 to hz; a tone within kFreqNear of the old tone 1 keeps its Hz offset, another its ratio
void freqMoveRoot(FreqState &s, float hz);

} // namespace td
