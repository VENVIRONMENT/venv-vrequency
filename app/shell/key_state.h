// venv-vrequency — the physical keys as the window last saw them: which are down and what each did,
// the modifier layers, and the short log the key test shows.
#pragma once
#include <QHash>
#include <QString>
#include <QStringList>

enum HeldKind { HK_STRING, HK_ACT, HK_NONE, HK_PAGE }; // HK_PAGE: a = the page that took it
struct Held { int kind; int a, b; };          // what a physical key did when pressed

struct KeyState {
    QHash<int, Held> held;                    // Qt key -> what it did
    int maxHeld = 0;                          // most keys down at once (key test)
    QStringList log;                          // the last presses and releases (key test)
    QString text;                             // the text of the key being handled, for typing
    bool chromatic = false, chord = false, command = false;   // the modifier layers
    bool bendUp = false, bendDown = false;
    int filterDir = 0;                        // Ctrl+C / Ctrl+X held: the cutoff sweeps up (1) or down (-1)
    bool susKey = false, susLatch = false;    // sustain: held by a key, latched by the soft key
    bool chordRow = false;                    // the bottom letter row plays chords
    bool testOpen = false;                    // the key test panel is over the page
};
