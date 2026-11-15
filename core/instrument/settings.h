// venv-vrequency — the performance settings and the names of their choices.
#pragma once
#include "core/drums/drums.h"
#include "core/synth/scale.h"

namespace td {

enum Setting {
    S_ROOT, S_SCALE, S_OCTAVE, S_ROWINT, S_BPM, S_VOLUME, S_XYMAP, S_XYHOLD,
    S_PADMODE, S_METRO,
    S_BENDRANGE, S_BENDTIME, S_GLIDESCALE, S_LOOPVOL, S_LATENCY,
    S_KIT, S_BEATVOL,
    S_POLY,
    S_COUNT
};

enum PadMode { PM_HOLD, PM_DRONE, PM_COUNT };   // what a tap on a chord pad does
enum XyMap { XY_FILTER, XY_WOBBLE, XY_MORPH, XY_SPACE, XY_BEND, XY_GLITCH, XY_COUNT };

inline const char *xyMapName(int m) {
    static const char *N[XY_COUNT] = {"FILTER", "WOBBLE", "MORPH", "SPACE", "BEND", "GLITCH"};
    return N[m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m)];
}
inline const char *xyAxisName(int m, bool yAxis) {
    static const char *X[XY_COUNT] = {"CUTOFF", "CUTOFF", "TIMBRE", "DELAY", "PITCH", "COARSE"};
    static const char *Y[XY_COUNT] = {"RESONANCE", "VIBRATO", "DRIVE", "REVERB", "CUTOFF", "GRIT"};
    m = m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m);
    return yAxis ? Y[m] : X[m];
}
inline const char *padModeName(int m) {
    static const char *N[PM_COUNT] = {"HOLD", "DRONE"};
    return N[m < 0 ? 0 : (m >= PM_COUNT ? PM_COUNT - 1 : m)];
}

struct Settings {
    Tuning tuning;
    float bpm = 100;
    float volume = 0.7f;
    int xyMap = XY_FILTER;
    bool xyHold = false;
    int padMode = PM_HOLD;
    bool metro = false;
    float bendRange = 2;   // semitones
    float bendTime = 120;  // ms
    float glideScale = 1;  // multiplies each patch's glide time
    float loopVol = 0.9f;
    float latencyMs = 0;   // output delay (e.g. Bluetooth): loop recordings are shifted earlier by this
    int kit = KIT_SOFT;    // drum kit
    float beatVol = 0.7f;
    bool poly = false;     // the letter keys: false = three strings (one note per row, slides), true = every key its own note
};

} // namespace td
