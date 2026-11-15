// venv-vrequency — the state the audio thread publishes for the interface to draw.
#pragma once
#include "core/drums/drums.h"
#include "core/instrument/limits.h"
#include "core/instrument/settings.h"
#include <cstdint>

namespace td {

enum LoopState { L_EMPTY, L_COUNTIN, L_REC, L_PLAY, L_OVERDUB, L_STOPPED };

struct StringUi {
    bool gate = false;
    float note = 0, target = 0;
    int held = 0;      // number of keys held on this string
    int topCol = -1;   // column of the sounding key
    uint16_t heldMask = 0, soundMask = 0; // poly: keys held / notes sounding (held or sustained), bit = column
};

struct UiState {
    StringUi str[kStrings];
    Settings set;
    double beat = 0;
    // chord pads
    bool padsPlaying = false;
    int holdChord = -1, droneMask = 0, curChord = -1;
    char padsName[28] = {0};           // the sound they play
    // looper
    int loopState = L_EMPTY;
    float loopPos = 0;
    int loopBars = 0, layers = 0, countBeat = 0;
    bool loopFull = false, loopClosing = false;
    // beat
    bool beatPlaying = false, beatRec = false;
    int beatStep = -1;                 // the step sounding now, -1 when stopped
    bool beatSwitching = false;        // another beat waits for the next bar
    uint16_t beatPat[kDrums] = {0};
    uint32_t drumHits[kDrums] = {0};   // counts every hit of each drum (pads light up on a change)
    // meters
    float peak = 0, limiter = 1;
    int voices = 0;
    bool sustain = false;
    float bend = 0, xyX = 0.5f, xyY = 0.5f;
    bool xyTouch = false;
    bool glitch = false;        // the crusher is on
    float glitchX = 0, glitchY = 0;
    char leadName[28] = {0};
    uint64_t frames = 0;
    uint32_t droppedEvents = 0;
};

} // namespace td
