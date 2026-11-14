// venv-vrequency self-test — Track: the lane mixer.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <cstdio>

using namespace td;

namespace selftest {
// the mixer: level, mute, solo, the fader, Reset, each with undo
void trackMixer(Context &ctx) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    std::printf("- lane mixer\n");
    press(w, Qt::Key_X); w.shot();
    check(w.trackPage.mixing() && !w.trackPage.pickingSound() && !w.trackPage.chopping(), "X opens the mixer in place of the tools");
    press(w, Qt::Key_2); press(w, Qt::Key_Down); w.shot();
    check(w.trackPage.mixLaneShown() == LN_CHORDS && T.mix[LN_CHORDS].level == 75, "2 chooses Chords and Down lowers it to 75");
    press(w, Qt::Key_Right); press(w, Qt::Key_Up); press(w, Qt::Key_Up); w.shot();
    check(w.trackPage.mixLaneShown() == LN_LOOPA && T.mix[LN_LOOPA].level == 90, "Right moves to Loop A and Up twice raises it to 90");
    press(w, Qt::Key_0);
    check(T.mix[LN_LOOPA].level == kLaneLevelNormal, "0 puts it back to 80");
    tap(w, zn::at(zn::mixer_mute, 3)); w.shot();
    check(T.mix[LN_LOOPB].mute && !laneHeard(T, LN_LOOPB) && w.trackPage.mixLaneShown() == LN_LOOPB, "tapping M on Loop B mutes it");
    ctrl(w, Qt::Key_Z);
    check(!T.mix[LN_LOOPB].mute, "undo unmutes it");
    key(w, Qt::Key_Shift, true); ctrl(w, Qt::Key_Z); key(w, Qt::Key_Shift, false);
    check(T.mix[LN_LOOPB].mute, "and redo mutes it again");
    press(w, Qt::Key_1); press(w, Qt::Key_S); w.shot();
    check(T.mix[LN_BEAT].solo && !laneHeard(T, LN_CHORDS) && laneHeard(T, LN_BEAT), "1 then S solos the beat: the other lanes go quiet");
    press(w, Qt::Key_S);
    {
        QRectF fr = w.zoneRect(zn::at(zn::mixer_fader, 1));
        const int mixBeforeDrag = T.mix[LN_CHORDS].level;
        w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 30), PH_PRESS);
        for (int i = 1; i <= 8; ++i) w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 30 - i * 6), PH_MOVE);
        w.pointer(3, QPointF(fr.center().x(), fr.bottom() - 78), PH_RELEASE);
        w.shot();
        const int lv = T.mix[LN_CHORDS].level, before = mixBeforeDrag;
        check(lv > 50 && lv < 75 && w.trackPage.mixLaneShown() == LN_CHORDS, QString("dragging the Chords fader sets its level (%1)").arg(lv));
        ctrl(w, Qt::Key_Z);
        const int undone = T.mix[LN_CHORDS].level;
        key(w, Qt::Key_Shift, true); ctrl(w, Qt::Key_Z); key(w, Qt::Key_Shift, false);
        check(undone == before && T.mix[LN_CHORDS].level == lv, QString("the whole drag is one undo step (%1 back to %2)").arg(lv).arg(undone));
    }
    tap(w, zn::mixer_reset); w.shot();
    bool flat = true;
    for (int ln = 0; ln < kLanes; ++ln) flat = flat && sameMix(T.mix[ln], LaneMix());
    check(flat, "Reset puts every lane back to 80, unmuted");
    ctrl(w, Qt::Key_Z);
    check(T.mix[LN_LOOPB].mute && T.mix[LN_CHORDS].level != kLaneLevelNormal, "and undo brings the mix back");
    press(w, Qt::Key_Escape); w.shot();
    check(!w.trackPage.mixing(), "Esc closes the mixer");
    if (w.trackPage.chosenLane() < 0) press(w, Qt::Key_Right);                                  // choose a clip
    press(w, Qt::Key_X); press(w, Qt::Key_I); w.shot();
    check(w.trackPage.chosenLane() >= 0 && w.trackPage.pickingSound() && !w.trackPage.mixing(), "opening the sound picker closes the mixer");
    press(w, Qt::Key_Return); w.shot();
}
} // namespace selftest
