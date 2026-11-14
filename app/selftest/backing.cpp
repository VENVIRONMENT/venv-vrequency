// venv-vrequency self-test — the backing: a progression tapped in, with a sound of its own.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <cstdio>

using namespace td;

namespace selftest {
void testBacking(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- backing\n");
    inst.set(S_BPM, 240); wait(50);
    key(w, Qt::Key_Alt, true);
    for (int k : {Qt::Key_Z, Qt::Key_V, Qt::Key_B, Qt::Key_V}) { key(w, k, true); key(w, k, false); }
    key(w, Qt::Key_Alt, false);
    wait(150);
    UiState u = inst.uiState();
    check(u.backPlaying && u.progLen == 4 && u.prog[1] == 3, "Alt + bottom row taps a four-chord progression");
    key(w, Qt::Key_3, true); key(w, Qt::Key_3, false);
    key(w, Qt::Key_W, true); wait(1200);
    u = inst.uiState();
    check(QString(u.backName) == "Warm Lead" && QString(u.leadName) == "Soft Pad" && u.str[2].gate && u.backPlaying, "backing keeps its sound while the lead changes and plays");
    check(u.progPos != 0 || u.beat > 4, "progression advances with the clock");
    key(w, Qt::Key_W, false);
}
} // namespace selftest
