// venv-vrequency — engine tests: the scale lock and the diatonic chords.
#include "tests/support.h"
#include "core/synth/scale.h"
#include <cstring>

void testScales() {
    SECTION("scale lock: note tables");
    Tuning t; // defaults: A minor pentatonic
    int expect0[7] = {45, 48, 50, 52, 55, 57, 60}; // A2 C3 D3 E3 G3 A3 C4
    for (int c = 0; c < 7; ++c) CHECK(t.noteFor(0, c, false) == expect0[c], "bottom row col %d = %d", c, t.noteFor(0, c, false));
    CHECK(t.noteFor(1, 0, false) == 50, "middle string starts on D3, got %d", t.noteFor(1, 0, false));
    CHECK(t.noteFor(2, 0, false) == 55, "top string starts on G3, got %d", t.noteFor(2, 0, false));
    CHECK(t.noteFor(0, 3, true) == 48, "chromatic override gives semitone steps");
    for (int sc = 0; sc < SC_COUNT; ++sc)
        for (int root = 0; root < 12; ++root) {
            Tuning u; u.scale = sc; u.root = root;
            const ScaleDef &d = scaleDef(sc);
            bool allIn = true, rising = true;
            for (int row = 0; row < 3; ++row) {
                int prev = -1;
                for (int col = 0; col < 10; ++col) {
                    int n = u.noteFor(row, col, false);
                    int pc = ((n - root) % 12 + 12) % 12;
                    bool in = false;
                    for (int i = 0; i < d.n; ++i) if (d.steps[i] == pc) in = true;
                    if (!in) allIn = false;
                    if (n <= prev) rising = false;
                    prev = n;
                }
            }
            CHECK(allIn, "%s root %d: every key is in the scale", d.name, root);
            CHECK(rising, "%s root %d: pitch rises left to right", d.name, root);
        }
    SECTION("chords: diatonic triads");
    Tuning am; // A minor family
    Chord i = buildChord(am, 0, false), iv = buildChord(am, 3, false), v7 = buildChord(am, 4, true), III = buildChord(am, 2, false);
    CHECK(std::strcmp(i.name, "Am") == 0, "i = %s", i.name);
    CHECK(std::strcmp(iv.name, "Dm") == 0, "iv = %s", iv.name);
    CHECK(std::strcmp(v7.name, "Em7") == 0, "v7 = %s", v7.name);
    CHECK(std::strcmp(III.name, "C") == 0, "III = %s", III.name);
    CHECK(i.count == 4 && v7.count == 5, "triad has bass+3, seventh has bass+4");
    Tuning cm; cm.root = 0; cm.scale = SC_MAJOR;
    CHECK(std::strcmp(buildChord(cm, 4, true).name, "G7") == 0, "V7 in C = %s", buildChord(cm, 4, true).name);
    CHECK(std::strcmp(buildChord(cm, 6, false).name, "Bdim") == 0, "vii in C = %s", buildChord(cm, 6, false).name);
}
