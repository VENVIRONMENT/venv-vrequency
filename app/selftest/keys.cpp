// venv-vrequency self-test — the keyboard plays notes: slides, chords, Shift, Space and the octave keys.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <cmath>
#include <cstdio>

using namespace td;

namespace selftest {
void testKeys(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- keyboard events -> notes\n");
    key(w, Qt::Key_Z, true); wait(120);
    UiState u = inst.uiState();
    check(u.str[0].gate && std::fabs(u.str[0].note - 45) < 0.01f, "Z sounds the low string's first note (A2)");
    int v0 = u.voices;
    for (int i = 0; i < 6; ++i) key(w, Qt::Key_Z, true, true); // keyboard auto-repeat
    wait(80);
    check(inst.uiState().voices == v0, "auto-repeat events do not retrigger the note");
    key(w, Qt::Key_C, true); wait(40);
    u = inst.uiState();
    check(u.str[0].target == 50 && u.str[0].note > 45.0f && u.str[0].note < 50.0f, QString("holding Z and pressing C slides (pitch in transit: %1)").arg(u.str[0].note));
    wait(300);
    u = inst.uiState();
    check(std::fabs(u.str[0].note - 50) < 0.01f && u.voices == v0, "slide arrives on D3 with the same single voice");
    key(w, Qt::Key_C, false); wait(400);
    u = inst.uiState();
    check(u.str[0].gate && std::fabs(u.str[0].note - 45) < 0.01f, "releasing C slides back to the held Z note");
    key(w, Qt::Key_A, true); key(w, Qt::Key_Q, true); wait(120);
    u = inst.uiState();
    check(u.str[0].gate && u.str[1].gate && u.str[2].gate, "three strings sound together");
    key(w, Qt::Key_Z, false); key(w, Qt::Key_A, false); key(w, Qt::Key_Q, false); wait(120);
    u = inst.uiState();
    check(!u.str[0].gate && !u.str[1].gate && !u.str[2].gate, "key releases stop the notes");
    key(w, Qt::Key_Shift, true); key(w, Qt::Key_X, true); wait(80);
    check(std::fabs(inst.uiState().str[0].target - 46) < 0.01f, "Shift = chromatic override (X gives A#2)");
    key(w, Qt::Key_X, false); key(w, Qt::Key_Shift, false);
    key(w, Qt::Key_Space, true); key(w, Qt::Key_S, true); wait(60); key(w, Qt::Key_S, false); wait(150);
    check(inst.uiState().str[1].gate, "Space sustains a released note");
    key(w, Qt::Key_Space, false); wait(100);
    check(!inst.uiState().str[1].gate, "releasing Space lets it go");
    key(w, Qt::Key_Up, true); key(w, Qt::Key_Up, false); wait(60);
    check(inst.uiState().set.tuning.octave == 1, "Up arrow raises the octave");
    key(w, Qt::Key_Down, true); key(w, Qt::Key_Down, false); wait(60);
}
} // namespace selftest
