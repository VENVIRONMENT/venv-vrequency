// venv-vrequency — pictures of every page, for the documentation: `vrequency --audio null --render DIR`.
// (The tour's pictures are in tourdemo.cpp.)
#include "app/selftest/selftest.h"
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <QDir>
#include <cstdio>

using namespace td;

namespace selftest {
namespace {
// every page as it stands, while a slide is held
void shootPages(MainWindow &w, Instrument &inst, const QString &dir) {
    static const char *files[PG_COUNT] = {"0-play", "1-sound", "9-pulse", "27-freq", "2-backing", "3-tune", "4-system", "5-help", "14-chat"};
    for (int p = 0; p < PG_COUNT; ++p) {
        w.setPage(p);
        if (p == PG_PULSE) { // hits in the strip, a latched pad, the pulse train selected
            for (int k : {0, 2, 6, 3, 7, 1}) { w.pulsePage.hitPad(k); w.shot(); wait(160); }
            w.pulsePage.setLatch(4, true); wait(500);
            w.pulsePage.hitPad(5); wait(200);
        }
        if (p == PG_FREQ) { // two tones beating, a third a fifth up, held long enough to fill the 1.5 s view
            inst.freqSet(2, FF_ON, 1); inst.freqSet(-1, FF_HOLD, 1); w.freqPage.select(1); wait(1700);
        }
        if (p == PG_CHAT) { // a conversation, so the page has something to show
            w.chat.add("rook", "Hi. I can hear A minor pentatonic at 100 bpm. Want a backing under it?", "ask", false);
            w.chat.add("you", "yes, something slow with a soft pad", "say", true);
            w.chat.add("rook", "On it: I-vi-IV-V on sound 3, tempo down to 84. Play over it and I will listen.", "say", false);
            w.chat.add("rook", "Tip: hold Space and slide between the top row keys for long glides.", "tip", false);
            w.chat.seen();
            w.chat.toggle(); w.chat.toggle(); w.setPage(PG_CHAT); // leaves the draft empty, unread cleared
        }
        for (int i = 0; i < 30; ++i) { w.shot(); wait(40); } // let the trace and the auto-range settle
        w.shot().save(QString("%1/%2.png").arg(dir).arg(files[p]));
        if (p == PG_PULSE) { w.pulsePage.setLatch(4, false); wait(60); }
        if (p == PG_FREQ) { inst.freqSet(-1, FF_HOLD, 0); inst.freqSet(2, FF_ON, 0); w.freqPage.select(0); wait(60); }
    }
}

// a new message from the copilot while playing: the ticker at the bottom
void shootTicker(MainWindow &w, const QString &dir) {
    w.setPage(PG_PLAY);
    w.chat.add("rook", "Nice slide. Try ending the phrase on the fifth: hold Z, press N.", "tip", false);
    for (int i = 0; i < 8; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/15-ticker.png").arg(dir));
}

// guided help: the dock on Synth with the octave buttons highlighted, a caption without the dock, the dock on Help
void shootGuidedHelp(MainWindow &w, const QString &dir) {
    QStringList unk;
    w.setPage(PG_PLAY);
    wait(7300); w.refreshZones();   // the ticker has gone
    w.guide.setHighlight({zn::loop_pedal}, "LOOP PEDAL", 20000, unk);
    for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/18-caption.png").arg(dir));
    w.guide.clearHighlight();
    w.guide.dockOpenFocus(); w.shot();
    w.chat.add("you", "what does octave do?", "say", true, "synth", {});
    w.chat.add("rook", "It moves every string up or down twelve notes. Tap + and play the same key again.", "say", false);
    w.guide.setHighlight({"octave"}, "OCTAVE", 20000, unk);
    for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/16-dock-synth.png").arg(dir));
    w.guide.clearHighlight(); w.guide.dockClose();
    w.setPage(PG_HELP); w.shot();
    w.guide.dockOpenFocus();
    w.chat.add("rook", "Here is the Loop pedal row: Enter starts a count-in, records, then plays and overdubs.", "say", false);
    w.shot();
    w.guide.setHighlight({QString(zn::help_row) + "loop_pedal_count_in_record_play_ov"}, "LOOP PEDAL", 20000, unk);
    for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/17-dock-help.png").arg(dir));
    w.guide.clearHighlight(); w.guide.dockClose();
    w.setPage(PG_PLAY);
}

// the Glitch mapping with a finger on the pad
void shootGlitch(MainWindow &w, Instrument &inst, const QString &dir) {
    w.setPage(PG_PLAY);
    inst.set(S_XYMAP, XY_GLITCH); wait(60);
    QRectF xy = w.zoneRect(zn::xy);
    w.pointer(1, QPointF(xy.left() + xy.width() * 0.45, xy.top() + xy.height() * 0.62), PH_PRESS);
    for (int i = 0; i < 40; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/7-glitch.png").arg(dir));
    w.pointer(1, xy.center(), PH_RELEASE);
    inst.set(S_XYMAP, XY_FILTER); wait(60);
}

// Warp: the Sound page's Warp view with a folded, crushed sound, and the Pulse page with WARP on
void shootWarp(MainWindow &w, Instrument &inst, const QString &dir) {
    w.setPage(PG_SOUND);
    const Patch before = w.sounds.lead;
    w.soundPage.warpView = true;
    w.sounds.setParam(P_DRIVE, 0.2f); w.sounds.setParam(P_FOLD, 0.75f); w.sounds.setParam(P_CRUSH, 8);
    w.sounds.setParam(P_DSAMP, 0.3f); w.sounds.setParam(P_BENDIN, -12);
    for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/29-sound-warp.png").arg(dir));
    w.sounds.recall(before); w.soundPage.warpView = false;
    w.setPage(PG_PULSE);
    for (int k : {0, 2, 6, 3, 7, 1, 5}) { w.pulsePage.hitPad(k); w.shot(); wait(160); } // hits in the strip, the tom selected
    inst.pulseSet(5, PF_DRIVE, 0.6f); inst.pulseSet(5, PF_FOLD, 0.3f); inst.pulseSet(5, PF_BEND, -12);
    inst.pulseSet(7, PF_CRUSH, 0.5f);
    w.pulsePage.warpMode = true; wait(60);
    for (int i = 0; i < 6; ++i) { w.shot(); wait(40); }
    w.shot().save(QString("%1/30-pulse-warp.png").arg(dir));
    w.pulsePage.warpMode = false;
    for (int p = 0; p < kPulsePads; ++p) for (int f : {PF_DRIVE, PF_FOLD, PF_CRUSH, PF_BEND}) inst.pulseSet(p, f, 0);
    wait(60);
}

// every page of the built-in guide, for proof-reading
void shootHelpSections(MainWindow &w, const QString &dir) {
    w.setPage(PG_HELP);
    for (int i = 0; i < 4; ++i) { w.shot(); w.shot().save(QString("%1/help-%2.png").arg(dir).arg(i)); tap(w, zn::help_next); }
}

// the key test panel with three keys down
void shootKeyTest(MainWindow &w, const QString &dir) {
    w.setPage(PG_SYS);
    w.setKeyTest(true);
    key(w, Qt::Key_Z, true); key(w, Qt::Key_S, true); key(w, Qt::Key_E, true); wait(120);
    w.shot().save(QString("%1/6-keytest.png").arg(dir));
    key(w, Qt::Key_E, false); key(w, Qt::Key_S, false); key(w, Qt::Key_Z, false);
    w.setKeyTest(false);
    w.setPage(PG_PLAY);
}
} // namespace
} // namespace selftest

int renderShots(MainWindow &w, Instrument &inst, const QString &dir) {
    using namespace selftest;
    QDir().mkpath(dir);
    inst.set(S_VOLUME, 0.5f);
    w.setPage(PG_PLAY);
    key(w, Qt::Key_A, true); wait(250);
    key(w, Qt::Key_F, true); // a held slide, so the scope and the strings have something to show
    shootPages(w, inst, dir);
    shootTicker(w, dir);
    shootGuidedHelp(w, dir);
    shootGlitch(w, inst, dir);
    shootWarp(w, inst, dir);
    key(w, Qt::Key_F, false); key(w, Qt::Key_A, false);
    if (qEnvironmentVariableIsSet("VREQUENCY_HELP_SHOTS")) shootHelpSections(w, dir);
    shootKeyTest(w, dir);
    std::printf("wrote %s\n", dir.toUtf8().constData());
    return 0;
}
