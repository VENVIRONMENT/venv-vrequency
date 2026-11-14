// venv-vrequency — the tour's demo, playing pages: Synth, Sound, Beat, Backing, Tune, System, Help and Chat pictures.
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

void TourDemo::playPages(Rig &rig, std::vector<TourView::Pic> &pics) {
    Instrument &inst = rig.inst;
    MainWindow &w = rig.w;
    auto P = [&](const char *id) -> TourView::Pic & { return pics[TourView::pageIndex(id)]; };
    // ---- the playing pages, with two notes held (a slide) so the scope and the strings are lit ----
    w.setPage(PG_PLAY);
    rig.key(Qt::Key_A, true); pump(inst, 250);
    rig.key(Qt::Key_F, true);
    rig.settle(30);
    capture(rig, 4, P("synth"));
    w.setPage(PG_SOUND); rig.settle(3);
    capture(rig, 5, P("sound"));
    w.soundPage.deepEdit = true; w.soundPage.deepPage = 0; rig.settle(2);
    capture(rig, 6, P("sound-deep"));
    for (int dp = 1; dp < 4; ++dp) { w.soundPage.deepPage = dp; rig.settle(2); capture(rig, 13 + dp, pics[13 + dp]); } // the other parameters
    w.soundPage.deepEdit = false;
    rig.key(Qt::Key_F, false); rig.key(Qt::Key_A, false);
    w.setPage(PG_BEAT); rig.settle(2);
    for (int i = 0; i < 7; ++i) { // eight kept beats: the row pages, with More
        w.beatPage.newBeat(); rig.settle(2);
        inst.beatDo(BT_STEP, D_KICK, 0, 1); rig.settle(2);
    }
    w.beatPage.switchTo(0); rig.settle(3);
    inst.beatDo(BT_PLAYSTOP); pump(inst, 700);
    inst.beatDo(BT_PLAYSTOP); pump(inst, 60);
    QThread::msleep(700);                                                     // the pads that just sounded fade
    capture(rig, 7, P("beat"));
    w.setPage(PG_BACK); rig.settle(2);
    capture(rig, 8, P("backing"));
    w.setPage(PG_TUNE); rig.settle(2);
    w.tunePage.listener.result = td::ListenResult();
    w.tunePage.listener.result.ok = true;
    w.tunePage.listener.result.keys[0].root = 9; w.tunePage.listener.result.keys[0].minor = true; w.tunePage.listener.result.keys[0].score = 0.8f;
    w.tunePage.listener.result.keyConfidence = 0.7f; w.tunePage.listener.result.bpm = 100; w.tunePage.listener.result.bpmConfidence = 0.7f;
    w.tunePage.listener.state = 2; rig.settle(2);
    capture(rig, 9, P("tune"));
    w.setPage(PG_SYS); rig.settle(2);
    capture(rig, 10, P("system"));
    w.keys.testOpen = true;
    rig.key(Qt::Key_Z, true); rig.key(Qt::Key_S, true); rig.key(Qt::Key_E, true);
    pump(inst, 60);
    capture(rig, 11, P("key-test"));
    rig.key(Qt::Key_E, false); rig.key(Qt::Key_S, false); rig.key(Qt::Key_Z, false);
    w.keys.testOpen = false;
    w.setPage(PG_HELP); rig.settle(2);
    capture(rig, 12, P("help"));
    w.chat.add("rook", "Hi. I can hear A minor pentatonic at 100 bpm. Want a backing under it?", "ask", false);
    w.chat.add("you", "yes, something slow with a soft pad", "say", true);
    w.chat.add("rook", "On it: I-vi-IV-V on sound 3, tempo down to 84. Play over it and I will listen.", "say", false);
    w.chat.seen();
    w.setPage(PG_CHAT); w.chat.draft.clear(); rig.settle(2);
    capture(rig, 13, P("chat"));
    inst.cmd(EV_LOOP, LC_PLAYSTOP);

}
