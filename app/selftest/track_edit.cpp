// venv-vrequency self-test — Track: Chop by touch and by keys, choosing clips, copy and paste, the cycle.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"

using namespace td;

namespace selftest {
// Chop by touch: the buttons and the strip
void trackChopByTouch(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    touch(w, tr.lanes.cell(LN_LOOPA, 0));
    press(w, Qt::Key_E); wait(60);
    check(w.trackPage.chopping() && w.trackPage.chosenLane() == LN_LOOPA, "E opens Chop for the chosen clip");
    const double Lc = T.clips[2]->beats();
    tap(w, zn::chop_halves); wait(40);
    check(T.lane[LN_LOOPA][0].pieces.size() == 2, "Halves cuts it in two");
    tap(w, zn::chop_move_right); wait(40);
    check(T.lane[LN_LOOPA][0].pieces[0].src == Lc / 2 && T.lane[LN_LOOPA][0].pieces[1].src == 0, "Move swaps the halves");
    tap(w, zn::chop_mute_piece); wait(40);
    check(T.lane[LN_LOOPA][0].pieces[1].mute, "Mute piece silences the chosen piece");
    tap(w, zn::chop_reset); wait(40);
    check(T.lane[LN_LOOPA][0].pieces.size() == 1 && !T.lane[LN_LOOPA][0].pieces[0].mute, "Reset undoes every cut");
    QRectF strip = w.zoneRect(zn::chop_strip);
    const QPointF quarter(strip.left() + strip.width() / 4 + 3, strip.center().y());
    touch(w, quarter); wait(40);
    check(T.lane[LN_LOOPA][0].pieces.size() == 2 && T.lane[LN_LOOPA][0].pieces[0].len == Lc / 4, "tapping a dotted line cuts there");
    touch(w, quarter); wait(40);
    check(T.lane[LN_LOOPA][0].pieces.size() == 1, "tapping the cut mends it");
    tap(w, zn::chop_done); wait(40);
    check(!w.trackPage.chopping(), "Done closes Chop");
}

// the arrow keys choose a clip, the play line moves, copy, paste and cut
void trackChooseCopyPaste(Context &ctx) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    press(w, Qt::Key_Up); press(w, Qt::Key_Up);
    check(w.trackPage.chosenLane() == LN_BEAT && w.trackPage.chosenIndex() == 0, "Up chooses a clip on the lane above");
    press(w, Qt::Key_Right);
    check(w.trackPage.chosenLane() == LN_BEAT && w.trackPage.chosenIndex() == 1, "Right chooses the clip after");
    press(w, Qt::Key_Down);
    check(w.trackPage.chosenLane() == LN_CHORDS, "Down chooses a clip on the lane below");
    press(w, Qt::Key_Up);
    const int line0 = w.trackPage.playLineBar();
    press(w, Qt::Key_Period);
    const int line1 = w.trackPage.playLineBar();
    press(w, Qt::Key_Comma);
    key(w, Qt::Key_Shift, true); press(w, Qt::Key_Left); key(w, Qt::Key_Shift, false);
    check(line1 == line0 + 1 && w.trackPage.playLineBar() == line0 - 1, ". and , (and Shift+Left) move the play line");
    press(w, Qt::Key_Period);
    ctrl(w, Qt::Key_C);
    check(w.trackPage.clipInHand() && T.lane[LN_BEAT].size() == 2, "Ctrl+C takes a copy of the chosen clip in hand");
    ctrl(w, Qt::Key_V);
    check(T.lane[LN_BEAT].size() == 3 && T.lane[LN_BEAT][2].bar == 8 && T.bars == 10, "Ctrl+V puts it on the first free bars after the line, and the track grows to fit");
    ctrl(w, Qt::Key_X);
    check(T.lane[LN_BEAT].size() == 2 && w.trackPage.clipInHand(), "Ctrl+X takes the chosen clip off the track and into your hand");
    press(w, Qt::Key_Escape);
    ctrl(w, Qt::Key_Z); ctrl(w, Qt::Key_Z);
    check(T.lane[LN_BEAT].size() == 2 && T.bars == 8, "two undos take the paste back, and the track's length with it");
    press(w, Qt::Key_K);
    check(T.clips.size() == 3 && w.lastToast().startsWith("Already kept"), "K is Keep (nothing new to keep this time)");
}

// Chop by keys
void trackChopByKeys(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    press(w, Qt::Key_Return); w.shot(); // the paste scrolled the bars: back to bar 1
    touch(w, tr.lanes.cell(LN_LOOPA, 0));
    press(w, Qt::Key_E);
    ctrl(w, Qt::Key_T);
    check(w.trackPage.chopping() && T.lane[LN_LOOPA][0].pieces.size() == 2, "in Chop, Ctrl+T cuts the chosen piece in half");
    press(w, Qt::Key_Right); press(w, Qt::Key_M);
    check(T.lane[LN_LOOPA][0].pieces[1].mute && !T.lane[LN_LOOPA][0].pieces[0].mute, "Right chooses the next piece, M mutes it");
    key(w, Qt::Key_Alt, true); press(w, Qt::Key_Left); key(w, Qt::Key_Alt, false);
    check(T.lane[LN_LOOPA][0].pieces[0].mute && T.lane[LN_LOOPA][0].pieces[0].src > 0, "Alt+Left moves the piece earlier");
    press(w, Qt::Key_M);
    key(w, Qt::Key_Alt, true); press(w, Qt::Key_Right); key(w, Qt::Key_Alt, false);
    press(w, Qt::Key_Left);
    ctrl(w, Qt::Key_J);
    check(T.lane[LN_LOOPA][0].pieces.size() == 1, "Ctrl+J mends the piece with the one after it");
    ctrl(w, Qt::Key_T); ctrl(w, Qt::Key_R);
    check(T.lane[LN_LOOPA][0].pieces.size() == 2 && T.lane[LN_LOOPA][0].pieces[1].src == 0, "Ctrl+R makes the next piece repeat it");
    ctrl(w, Qt::Key_Z); ctrl(w, Qt::Key_Z);
    press(w, Qt::Key_E);
    check(!w.trackPage.chopping() && T.lane[LN_LOOPA][0].pieces.size() == 1, "E closes Chop again");
    touch(w, tr.lanes.cell(LN_LOOPA, 0));
}

// the cycle
void trackCycle(Context &ctx) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    press(w, Qt::Key_C);
    check(T.cycleTo > T.cycleFrom && T.cycleFrom == 0, "C cycles round the chosen clip");
    press(w, Qt::Key_C);
    check(T.cycleTo == 0, "C again switches the cycle off");
}
} // namespace selftest
