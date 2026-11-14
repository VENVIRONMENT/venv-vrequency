// venv-vrequency — the instrument shelf: ready-made Keys, Guitar and Bass sounds.
//
// These are ordinary patches for the same voice as every other sound, shaped to stand in
// for played instruments. The Track page offers them for a clip that is already recorded.
#pragma once
#include "core/synth/params.h"

namespace td {

enum SoundKind { SK_KEYS, SK_GUITAR, SK_BASS, kLibKinds };
constexpr int kLibSounds = 6; // in each kind

inline const char *libraryKindName(int kind) {
    static const char *N[kLibKinds] = {"Keys", "Guitar", "Bass"};
    return N[kind < 0 ? 0 : (kind >= kLibKinds ? kLibKinds - 1 : kind)];
}

// Built and matched in loudness the first time one is asked for (a few milliseconds each),
// so call these from the app's thread, never from the audio thread.
const Patch &librarySound(int kind, int index);
const char *libraryHint(int kind, int index);         // two words on what it is like
// Where a clip should sit when it takes a sound of this kind: bass plays an octave down.
inline int libraryOctave(int kind) { return kind == SK_BASS ? -1 : 0; }
// The kind that holds a sound of this name (and its place there), or -1.
int libraryFind(const char *name, int *index = nullptr);

} // namespace td
