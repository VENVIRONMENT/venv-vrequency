// venv-vrequency self-test — the loop pedal: count-in, record, play, overdub, undo.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <cstdio>

using namespace td;

namespace selftest {
void testLooper(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- looper\n");
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    check(inst.uiState().loopState == L_COUNTIN, "Enter starts the count-in");
    for (int i = 0; i < 40 && inst.uiState().loopState != L_REC; ++i) wait(50);
    check(inst.uiState().loopState == L_REC, "recording starts on the bar");
    wait(150); key(w, Qt::Key_E, true); wait(250); key(w, Qt::Key_T, true); wait(200); key(w, Qt::Key_T, false); key(w, Qt::Key_E, false);
    wait(250);
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false);
    for (int i = 0; i < 40 && (inst.uiState().layers < 1); ++i) wait(50);
    const UiState u = inst.uiState();
    check(u.loopState == L_PLAY && u.layers == 1, QString("loop closes on the bar and plays (%1 bar)").arg(u.loopBars));
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    key(w, Qt::Key_H, true); wait(200); key(w, Qt::Key_H, false); wait(100);
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(100);
    check(inst.uiState().layers == 2, "overdub adds a second layer");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_U, true); key(w, Qt::Key_U, false); wait(80);
    check(inst.uiState().layers == 1, "Ctrl+U undoes the overdub");
    key(w, Qt::Key_Control, false);
}
} // namespace selftest
