// venv-vrequency self-test — the chord pads: a chord held from the keyboard, with a sound of its own.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <cstdio>

using namespace td;

namespace selftest {
void testChordPads(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- chord pads\n");
    inst.set(S_BPM, 240); wait(50);
    key(w, Qt::Key_Alt, true);
    key(w, Qt::Key_V, true); key(w, Qt::Key_V, false);
    key(w, Qt::Key_Alt, false);
    wait(150);
    UiState u = inst.uiState();
    check(u.padsPlaying && u.holdChord == 3 && u.curChord == 3, "Alt + V holds chord IV");
    key(w, Qt::Key_3, true); key(w, Qt::Key_3, false);
    key(w, Qt::Key_W, true); wait(1200);
    u = inst.uiState();
    check(QString(u.padsName) == "Warm Lead" && QString(u.leadName) == "Soft Pad" && u.str[2].gate && u.padsPlaying,
          "the pads keep their sound while the lead changes and plays");
    key(w, Qt::Key_W, false);
}
} // namespace selftest
