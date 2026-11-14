// venv-vrequency — the physical keys as the window last saw them: which are down and what each did,
// the modifier layers, and the short log the key test shows.
#pragma once
#include <QHash>
#include <QString>
#include <QStringList>

enum HeldKind { HK_STRING, HK_ACT, HK_NONE };
struct Held { int kind; int a, b; };          // what a physical key did when pressed

struct KeyState {
    QHash<int, Held> held;                    // Qt key -> what it did
    int maxHeld = 0;                          // most keys down at once (key test)
    QStringList log;                          // the last presses and releases (key test)
    QString text;                             // the text of the key being handled, for typing
    bool chromatic = false, chord = false, command = false;   // the modifier layers
    bool bendUp = false, bendDown = false;
    bool susKey = false, susLatch = false;    // sustain: held by a key, latched by the soft key
    bool chordRow = false;                    // the bottom letter row plays chords
};
