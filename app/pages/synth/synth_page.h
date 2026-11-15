// venv-vrequency — the Synth page (PG_PLAY): the instrument itself. Readouts on top, the scope that
// is also the X-Y pad, the three strings, the chord pads and the soft keys along the bottom.
// synth_page.cpp lays it out; synth_header.cpp, synth_strings.cpp and synth_softkeys.cpp draw the parts.
#pragma once
#include "app/pages/page.h"
#include "app/pages/synth/chord_pads.h"
#include "app/pages/synth/scope.h"
#include <deque>

class Sounds;

class SynthPage : public Page {
public:
    SynthPage(Shell &s, ChordPads &c);
    void paint() override;
    bool tabs() const override { return false; }   // this page has its own header instead of the tabs
    void releaseTouches();                   // let go of every string a finger holds (the window lost focus)
    bool toggleScopeView() { scope.stackView = !scope.stackView; return scope.stackView; } // true: now the 3D stack

private:
    Sounds &sounds;
    ChordPads &pads;
    Scope scope;
    int touchCol[td::kStrings] = {-1, -1, -1};     // the column each string's finger is on
    std::deque<float> trail[td::kStrings];         // where each sounding pitch has been, for the slide trail

    void paintHeader();                      // the sound, the key, tempo and octave, the output level
    void paintStrings(QRectF area);          // the three strings, their notes and the touch on them
    void paintSoftKeys();                    // loop, sustain, octave, New, Menu
    void paintLoopButton(QRectF r);
};
