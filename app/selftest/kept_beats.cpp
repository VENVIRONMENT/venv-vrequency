// venv-vrequency self-test — kept beats: + New, Left and Right, switching on the bar line, the More page.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QJsonObject>
#include <cstdio>

using namespace td;

namespace selftest {
void testKeptBeats(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- kept beats\n");
    w.setPage(PG_BEAT); wait(80);
    const UiState b0 = inst.uiState();
    const uint16_t hat0 = b0.beatPat[D_HAT];
    const int kit0b = b0.set.kit, n0 = w.beatPage.kept();
    check(n0 >= 1 && w.beatPage.keptName(w.beatPage.shown()).startsWith("Beat"), QString("the beat you have is a kept beat (%1 of %2)").arg(w.beatPage.shown() + 1).arg(n0));
    tap(w, zn::beatnew); wait(120);
    UiState bn = inst.uiState();
    bool empty = true;
    for (int d = 0; d < kDrums; ++d) empty = empty && bn.beatPat[d] == 0;
    check(w.beatPage.kept() == n0 + 1 && w.beatPage.shown() == n0 && empty, "+ New makes an empty beat and shows it; the others are kept");
    tap(w, zn::beatnew); wait(80);
    check(w.beatPage.kept() == n0 + 1, "+ New on an empty beat makes no second empty one");
    tap(w, zn::at(zn::pad, 0)); wait(40); // not recording: plays, writes nothing
    key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(120);
    tap(w, zn::at(zn::pad, 5)); wait(120);
    key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(120);
    const uint16_t rim = inst.uiState().beatPat[D_RIM];
    check(rim != 0 && !inst.uiState().beatPlaying, "the new beat records hits of its own");
    press(w, Qt::Key_Left); wait(150);
    UiState bl = inst.uiState();
    check(w.beatPage.shown() == n0 - 1 && bl.beatPat[D_HAT] == hat0 && bl.beatPat[D_RIM] == 0 && bl.set.kit == kit0b,
          "Left goes back to the beat before, exactly as it was, kit included");
    press(w, Qt::Key_Right); wait(150);
    check(w.beatPage.shown() == n0 && inst.uiState().beatPat[D_RIM] == rim, "Right comes back to the new one, hits kept");
    // while playing, a switch waits for the bar line
    inst.set(S_BPM, 60); wait(40);
    key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(300);
    tap(w, zn::at(zn::beatslot, n0 - 1)); wait(150);
    UiState bw = inst.uiState();
    check(bw.beatSwitching && bw.beatPat[D_RIM] == rim && w.beatPage.shown() == n0 - 1, "playing, the beat tapped waits for the next bar");
    wait(4200); // a bar at 60 bpm is 4 s
    bw = inst.uiState();
    check(!bw.beatSwitching && bw.beatPat[D_HAT] == hat0 && bw.beatPat[D_RIM] == 0, "and takes over on the bar line");
    key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(80);
    inst.set(S_BPM, 120);
    for (int i = 0; i < 8; ++i) { tap(w, zn::beatnew); wait(40); tap(w, zn::at(zn::pad, 3)); key(w, Qt::Key_Return, true); key(w, Qt::Key_Return, false); wait(60); tap(w, zn::at(zn::pad, 3)); wait(60); key(w, Qt::Key_Space, true); key(w, Qt::Key_Space, false); wait(60); }
    check(w.beatPage.kept() > 8 && w.zoneRect(zn::beatmore).width() > 0, QString("with %1 beats the row has a More page").arg(w.beatPage.kept()));
    w.saveState(); wait(40);
    QJsonObject kj; store::readJson(store::baseDir() + "/state.json", kj);
    const QJsonArray ka = kj["beats"].toArray();
    check(ka.size() == w.beatPage.kept() && kj["beatShown"].toInt() == w.beatPage.shown()
          && ka[n0].toObject()["beat"].toArray()[D_RIM].toString().contains('x'), "every kept beat is in the state file");
    for (int i = 0; i < 12; ++i) { press(w, Qt::Key_Left); wait(30); } // back to the first beat
    wait(120);
    check(w.beatPage.shown() == 0 && inst.uiState().beatPat[D_HAT] == hat0, "Left all the way comes back to Beat 1");
}
} // namespace selftest
