// venv-vrequency — the tour's demo, playing pages: Synth, Sound, Pulse, Freq, Backing, Tune, System, Help and Chat pictures.
#include "app/tour/tour.h"
#include "app/tour/tourdemo.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include <QThread>
#include <array>

using namespace td;

void TourDemo::playPages(Rig &rig, std::vector<TourView::Pic> &pics) {
    Instrument &inst = rig.inst;
    MainWindow &w = rig.w;
    auto C = [&](const char *id) { capture(rig, TourView::pageIndex(id), pics[TourView::pageIndex(id)]); }; // a tour page's picture
    // ---- the playing pages, with two notes held (a slide) so the scope and the strings are lit ----
    w.setPage(PG_PLAY);
    rig.key(Qt::Key_A, true); pump(inst, 250);
    rig.key(Qt::Key_F, true);
    rig.settle(30);
    C("synth");
    w.setPage(PG_SOUND); rig.settle(3);
    C("sound");
    w.soundPage.deepEdit = true; w.soundPage.deepPage = 0; rig.settle(2);
    C("sound-deep");
    const int deep = TourView::pageIndex("sound-deep");
    for (int dp = 1; dp < 4; ++dp) { w.soundPage.deepPage = dp; rig.settle(2); capture(rig, deep, pics[TourView::variantPic(deep, dp)]); } // the other parameters
    w.soundPage.deepEdit = false;
    rig.key(Qt::Key_F, false); rig.key(Qt::Key_A, false);
    w.setPage(PG_PULSE); rig.settle(2);
    for (int p : {0, 2, 6, 3, 7}) { w.pulsePage.hitPad(p); pump(inst, 180); rig.settle(1); } // a few hits for the strip
    w.pulsePage.setLatch(1, true); pump(inst, 600); rig.settle(1); w.pulsePage.setLatch(1, false);
    w.pulsePage.hitPad(5); pump(inst, 400);                                  // the tom, selected
    rig.settle(3);
    QThread::msleep(300);                                                     // the pads that just sounded fade
    C("pulse");
    w.setPage(PG_FREQ); inst.freqSet(2, FF_ON, 1); inst.freqSet(-1, FF_HOLD, 1);  // two tones beating, a fifth above
    pump(inst, 1700); rig.settle(2);
    C("freq");
    inst.freqSet(-1, FF_HOLD, 0); inst.freqSet(2, FF_ON, 0); pump(inst, 60);
    w.setPage(PG_BACK); rig.settle(2);
    C("backing");
    w.setPage(PG_TUNE); rig.settle(2);
    w.tunePage.listener.result = td::ListenResult();
    w.tunePage.listener.result.ok = true;
    w.tunePage.listener.result.keys[0].root = 9; w.tunePage.listener.result.keys[0].minor = true; w.tunePage.listener.result.keys[0].score = 0.8f;
    w.tunePage.listener.result.keyConfidence = 0.7f; w.tunePage.listener.result.bpm = 100; w.tunePage.listener.result.bpmConfidence = 0.7f;
    w.tunePage.listener.state = 2; rig.settle(2);
    C("tune");
    w.setPage(PG_SYS); rig.settle(2);
    C("system");
    w.keys.testOpen = true;
    rig.key(Qt::Key_Z, true); rig.key(Qt::Key_S, true); rig.key(Qt::Key_E, true);
    pump(inst, 60);
    C("key-test");
    rig.key(Qt::Key_E, false); rig.key(Qt::Key_S, false); rig.key(Qt::Key_Z, false);
    w.keys.testOpen = false;
    w.setPage(PG_HELP); rig.settle(2);
    C("help");
    w.chat.add("rook", "Hi. I can hear A minor pentatonic at 100 bpm. Want a backing under it?", "ask", false);
    w.chat.add("you", "yes, something slow with a soft pad", "say", true);
    w.chat.add("rook", "On it: I-vi-IV-V on sound 3, tempo down to 84. Play over it and I will listen.", "say", false);
    w.chat.seen();
    w.setPage(PG_CHAT); w.chat.draft.clear(); rig.settle(2);
    C("chat");
    inst.cmd(EV_LOOP, LC_PLAYSTOP);

}
