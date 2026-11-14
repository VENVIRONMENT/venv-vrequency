// venv-vrequency self-test — Track: keeping clips and arranging them on the lanes.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QJsonObject>

using namespace td;

namespace selftest {
// + Keep, then arranging: place, join, repeat, split, undo, drag, mute, move
void trackKeepAndPlace(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    const TrackData &T = w.trackPage.trackData();
    w.setPage(PG_TRACK); wait(100);
    check(w.page() == PG_TRACK && PG_TRACK == 0 && PG_PLAY == 1, "Track is the first tab, Synth the second");
    check(T.clips.empty(), "the track starts empty");
    press(w, Qt::Key_Space); wait(80);
    check(!inst.uiState().trackPlaying, "Space on an empty track plays nothing");
    tap(w, zn::track_keep); wait(80);
    check(T.clips.size() == 3 && T.clips[0]->kind == CK_BEAT && T.clips[1]->kind == CK_CHORDS && T.clips[2]->kind == CK_LOOP,
          "+ Keep copies the beat, the chords and the loop onto the shelf");
    check(w.trackPage.clipInHand(), "and the newest clip is in hand, ready to place");
    wait(3200);
    QJsonObject aj; store::readJson(store::baseDir() + "/state.json", aj);
    check(aj["track"].toObject()["clips"].toArray().size() == 3, "a moment after an edit the track is in the state file without being asked");
    tap(w, zn::track_keep); wait(60);
    check(T.clips.size() == 3, "keeping the same things again adds nothing");
    tr.lanes.r = w.zoneRect(zn::track_lanes);
    if (!w.trackPage.clipInHand()) press(w, Qt::Key_3);
    touch(w, tr.lanes.cell(LN_LOOPA, 0)); wait(60);
    check(T.lane[LN_LOOPA].size() == 1 && T.lane[LN_LOOPA][0].bar == 0 && w.trackPage.chosenLane() == LN_LOOPA, "with a clip in hand, tapping a bar places it there");
    press(w, Qt::Key_1);
    key(w, Qt::Key_Control, true); press(w, Qt::Key_V); press(w, Qt::Key_V); key(w, Qt::Key_Control, false); wait(60);
    check(T.lane[LN_BEAT].size() == 2 && T.lane[LN_BEAT][1].bar == 1 && w.trackPage.playLineBar() == 2, "1 takes the first clip; Ctrl+V places it at the play line, twice");
    press(w, Qt::Key_Left);
    check(w.trackPage.chosenLane() == LN_BEAT && w.trackPage.chosenIndex() == 0, "Left chooses the clip before");
    ctrl(w, Qt::Key_J);
    check(T.lane[LN_BEAT].size() == 1 && T.lane[LN_BEAT][0].bars == 2, "Ctrl+J joins a clip with its repeat");
    ctrl(w, Qt::Key_R);
    check(T.lane[LN_BEAT].size() == 2 && T.lane[LN_BEAT][1].bar == 2 && w.trackPage.chosenIndex() == 1, "Ctrl+R repeats it to the right");
    press(w, Qt::Key_Return);
    key(w, Qt::Key_Shift, true); press(w, Qt::Key_Right); key(w, Qt::Key_Shift, false);
    check(w.trackPage.playLineBar() == 1, "Enter sends the play line to bar 1, Shift+Right moves it a bar");
    press(w, Qt::Key_Left);
    ctrl(w, Qt::Key_T);
    check(T.lane[LN_BEAT].size() == 3 && T.lane[LN_BEAT][0].bars == 1 && T.lane[LN_BEAT][1].bar == 1, "Ctrl+T splits the chosen clip at the play line");
    int depth = w.trackPage.trackUndoDepth();
    ctrl(w, Qt::Key_Z);
    check(T.lane[LN_BEAT].size() == 2 && w.trackPage.trackUndoDepth() == depth - 1, "Ctrl+Z undoes the split");
    key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(w, Qt::Key_Z); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
    check(T.lane[LN_BEAT].size() == 3, "Shift+Ctrl+Z does it again");
    ctrl(w, Qt::Key_Z);
    depth = w.trackPage.trackUndoDepth();
    w.pointer(3, tr.lanes.cell(LN_BEAT, 2), PH_PRESS);
    w.pointer(3, tr.lanes.cell(LN_BEAT, 3.6), PH_MOVE);
    w.pointer(3, tr.lanes.cell(LN_BEAT, 5), PH_MOVE);
    w.pointer(3, tr.lanes.cell(LN_BEAT, 5), PH_RELEASE);
    check(T.lane[LN_BEAT].size() == 2 && T.lane[LN_BEAT][1].bar == 5 && w.trackPage.trackUndoDepth() == depth + 1, "dragging a clip moves it by whole bars, as one undo step");
    press(w, Qt::Key_M);
    check(T.lane[LN_BEAT][1].mute, "M mutes the chosen clip");
    press(w, Qt::Key_M);
    key(w, Qt::Key_Alt, true); press(w, Qt::Key_Right); key(w, Qt::Key_Alt, false);
    check(!T.lane[LN_BEAT][1].mute && T.lane[LN_BEAT][1].bar == 6, "Alt+Right moves it a bar");
    press(w, Qt::Key_2);
    tap(w, zn::track_place); wait(60);
    check(T.lane[LN_CHORDS].size() == 1 && T.lane[LN_CHORDS][0].bar == 1 && T.lane[LN_CHORDS][0].bars == T.clips[1]->bars, "Place puts the chords clip on the Chords lane at the play line");
    press(w, Qt::Key_Escape);
    check(w.page() == PG_TRACK && !w.trackPage.clipInHand(), "Esc puts the clip in hand down (and stays on the page)");
}
} // namespace selftest
