// venv-vrequency — clips: the beat, the backing's chords or the looper's layers, kept for the track.
#pragma once
#include "core/drums/drums.h"
#include "core/instrument/limits.h"
#include "core/instrument/session.h"
#include "core/instrument/settings.h"
#include "core/synth/params.h"
#include <cstdint>
#include <vector>

namespace td {

enum ClipKind { CK_BEAT, CK_CHORDS, CK_LOOP };
constexpr int kTrackMaxBars = 64;

// A sound picked for a clip after it was kept. It stands in for the recorded one: every layer
// of a loop plays it, a chords clip backs with it, a beat clip uses the kit. The octave moves
// the notes of a loop or chords clip, with or without a picked sound.
struct SoundPick {
    bool on = false;  // `patch` stands in for the recorded sound
    Patch patch;
    int kit = -1;     // 0 or more: this kit stands in for the clip's own
    int octave = 0;   // -2..+2
    bool own = false; // on a placement: this one goes its own way, even back to the recorded sound
    bool any() const { return on || kit >= 0 || octave != 0; }
};
bool samePick(const SoundPick &a, const SoundPick &b);

struct Clip {
    int kind = CK_BEAT;
    char name[20] = {0};
    int bars = 1;
    // beat
    uint16_t beat[kDrums] = {0};
    int kit = KIT_SOFT;
    // chords: a copy of the backing as it was
    int prog[kProgMax] = {0};
    int progLen = 0, holdChord = -1, droneMask = 0;
    int backStyle = BS_SUSTAIN, chordBeats = 4, arpRate = 2;
    bool seventh = false;
    Patch patch;
    // loop: the layers, each with its events in time order
    std::vector<LayerData> layers;
    SoundPick pick;          // for every placement of this clip that has no pick of its own
    double beats() const { return bars * 4.0; }
};

const char *soundName(const Clip &c, const SoundPick &pick);   // "" when a loop has no sound at all
const char *recordedSoundName(const Clip &c);

// Clips from what is live now (false = there is nothing of that kind to keep).
bool beatClip(const SessionData &s, Clip &out);
bool chordsClip(const SessionData &s, Clip &out);
bool loopClip(const SessionData &s, Clip &out);
bool sameClip(const Clip &a, const Clip &b);

} // namespace td
