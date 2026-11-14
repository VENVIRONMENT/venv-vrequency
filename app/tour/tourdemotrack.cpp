// venv-vrequency — the tour's demo, Track page: arranges clips on the lanes, then draws the Track pictures.
#include "app/tour/tour.h"
#include "app/tour/tourdemo.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include "core/presets/sound_shelf.h"
#include <QThread>
#include <array>

using namespace td;

void TourDemo::trackPages(Rig &rig, std::vector<TourView::Pic> &pics) {
    Instrument &inst = rig.inst;
    MainWindow &w = rig.w;
    SessionData sd;
    auto P = [&](const char *id) -> TourView::Pic & { return pics[TourView::pageIndex(id)]; };
    // ---- the Track page: two beats, a progression and two loops arranged on the lanes ----
    inst.snapshot(sd);
    sd.prog[0] = 5; sd.prog[1] = 3; sd.prog[2] = 0; sd.prog[3] = 4; sd.progLen = 4; sd.settings.chordBeats = 4;
    sd.backing = extraPatch(1);
    sd.loopBeats = 8;
    sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
    inst.restore(sd); pump(inst, 80);
    w.setPage(PG_TRACK); rig.settle(3);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 1, Chords 1, Loop 1
    inst.beatDo(BT_ERASE, D_HAT); inst.beatDo(BT_STEP, D_CLAP, 4, 1); inst.beatDo(BT_STEP, D_TOM, 13, 1); pump(inst, 80);
    inst.snapshot(sd);
    sd.loopBeats = 16;
    sd.layers = {melody(extraPatch(1), {{0, 3, 52}, {4, 2, 55}, {6, 2, 57}, {8, 3.5, 60}, {12, 1, 59}, {13, 1, 57}, {14, 1.9, 55}}),
                 melody(extraPatch(1), {{2, 1, 76}, {7, 0.5, 79}, {10, 2, 81}, {15, 0.5, 79}})};
    inst.restore(sd); pump(inst, 80);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 2, Loop 2
    inst.beatDo(BT_STEP, D_COWBELL, 7, 1); pump(inst, 80); rig.settle(2);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 3: more clips than the shelf shows, so it pages
    rig.settle(2);
    w.trackPage.shelfPage = 0;                                                        // the keys 1-5 take the clips of the first page
    rig.esc();
    rig.press(Qt::Key_Return);
    rig.press(Qt::Key_1); for (int i = 0; i < 4; ++i) rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_4); rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_1); for (int i = 0; i < 3; ++i) rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_Return); rig.press(Qt::Key_2); rig.ctrl(Qt::Key_V); rig.ctrl(Qt::Key_V);
    const QRectF lanes = w.zoneRect("track_lanes");
    auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
    rig.press(Qt::Key_3); rig.touch(cell(LN_LOOPA, 2)); rig.touch(cell(LN_LOOPA, 6));
    rig.press(Qt::Key_5); rig.touch(cell(LN_LOOPB, 4));
    rig.esc();
    rig.touch(cell(LN_BEAT, 0)); rig.ctrl(Qt::Key_J); rig.ctrl(Qt::Key_J); rig.ctrl(Qt::Key_J);
    rig.touch(cell(LN_LOOPA, 2));
    rig.press(Qt::Key_Return);
    rig.key(Qt::Key_Shift, true); rig.press(Qt::Key_Right); rig.press(Qt::Key_Right); rig.key(Qt::Key_Shift, false);
    rig.press(Qt::Key_C);
    rig.press(Qt::Key_3);
    rig.touch(cell(LN_LOOPA, 2));
    rig.settle(2);
    w.toastUntil = 0;
    capture(rig, 0, P("track"));
    rig.esc();
    rig.press(Qt::Key_C);
    rig.press(Qt::Key_E);
    rig.settle(2);
    const QRectF strip = w.zoneRect("chop_strip");
    for (double f : {0.25, 0.5, 0.75}) rig.touch(QPointF(strip.left() + strip.width() * f + 2, strip.center().y()));
    rig.touch(QPointF(strip.left() + strip.width() * 0.6, strip.center().y()));
    w.toastUntil = 0;
    capture(rig, 1, P("track-chop"));
    rig.tap("chop_done");
    rig.press(Qt::Key_I); rig.press(Qt::Key_Right); rig.press(Qt::Key_1);
    rig.press(Qt::Key_Space);                                                // not playing in the picture
    w.toastUntil = 0;
    capture(rig, 2, P("track-sound"));
    rig.press(Qt::Key_Return);
    rig.touch(cell(LN_BEAT, 5)); rig.press(Qt::Key_I);                         // a beat clip: the Drums kinds
    w.toastUntil = 0;
    capture(rig, 17, pics[17]);
    rig.press(Qt::Key_Return);
    rig.touch(cell(LN_LOOPA, 2));
    inst.cmd(EV_PANIC); pump(inst, 60);
    rig.press(Qt::Key_X);
    rig.press(Qt::Key_4); rig.press(Qt::Key_M);
    rig.press(Qt::Key_2); for (int i = 0; i < 3; ++i) rig.press(Qt::Key_Down);
    rig.press(Qt::Key_3); rig.press(Qt::Key_Up); rig.press(Qt::Key_Up);
    rig.press(Qt::Key_1);
    w.toastUntil = 0;
    capture(rig, 3, P("track-mixer"));
    inst.cmd(EV_PANIC); pump(inst, 30);

}
