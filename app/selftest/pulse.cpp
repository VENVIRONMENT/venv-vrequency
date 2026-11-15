// venv-vrequency self-test — the Pulse page: pads by touch and key, the sliders, TYPE, LATCH, SYNC,
// LOOP, the strip of hits, and the pads' settings in the state file.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QElapsedTimer>
#include <QJsonObject>
#include <cmath>
#include <cstdio>

using namespace td;

namespace selftest {
void testPulse(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- pulse\n");
    w.setPage(PG_PULSE); wait(100);
    const float vol0 = inst.uiState().set.volume, bpm0 = inst.uiState().set.bpm;
    inst.set(S_VOLUME, 0.3f); inst.set(S_BPM, 120); wait(60);
    const uint32_t k0 = inst.uiState().pulseHits[0];
    tap(w, zn::at(zn::pad, 0)); wait(80);
    check(inst.uiState().pulseHits[0] == k0 + 1 && w.pulsePage.selected() == 0, "tapping the first pad plays it and selects it");
    press(w, Qt::Key_Q); wait(60); press(w, Qt::Key_U); wait(80);
    check(inst.uiState().pulseHits[0] == k0 + 3 && inst.uiState().voices == 0, "on the Pulse page Q and U play pad 1, not a string");
    const uint32_t a0 = inst.uiState().pulseHits[4];
    press(w, Qt::Key_A); wait(80);
    check(inst.uiState().pulseHits[4] == a0 + 1 && w.pulsePage.selected() == 4, "A plays pad 5 and selects it");
    const uint32_t r0 = inst.uiState().pulseHits[4];
    key(w, Qt::Key_Right, true); wait(40);
    check(w.pulsePage.selected() == 4 && inst.uiState().pulseHits[4] == r0 && inst.uiState().bend > 0, "Right bends on the Pulse page too (no pad played or picked)");
    key(w, Qt::Key_Right, false); wait(40);
    w.pulsePage.select(5);
    key(w, Qt::Key_S, true); wait(300);               // pad 6, the pulse train: held like a synth key
    check(inst.uiState().pulseHeld[5] && inst.uiState().pulsing, "holding S keeps pad 6 going");
    key(w, Qt::Key_S, false); wait(400);
    check(!inst.uiState().pulseHeld[5] && !inst.uiState().pulsing, "and letting go stops it");
    w.shot();
    check(w.pulsePage.ticksKept() >= 3, QString("the strip keeps every hit the engine logged (%1)").arg(w.pulsePage.ticksKept()));

    const int t0 = inst.uiState().pulse[5].type;
    tap(w, zn::pulsetype); wait(60);
    check(inst.uiState().pulse[5].type == (t0 + 1) % kPulseTypes, "TYPE gives the selected pad the next kind");
    for (int i = 1; i < kPulseTypes; ++i) { w.shot(); tap(w, zn::pulsetype); wait(30); }
    wait(60);
    check(inst.uiState().pulse[5].type == t0, "and eight taps go round to where it was");
    w.shot(); tap(w, zn::pulsetone); wait(60);
    const float tone = inst.uiState().pulse[5].tone;
    check(tone > 400 && tone < 900, QString("the TONE slider moves in octaves: its middle is about 600 Hz (%1)").arg(tone));
    w.shot(); tap(w, zn::pulselevel); wait(60);
    check(std::fabs(inst.uiState().pulse[5].level - 0.5f) < 0.02f, "the LEVEL slider sets the level");

    w.shot(); tap(w, zn::pulselatch); wait(300);
    check(inst.uiState().pulseLatched[5] && inst.uiState().pulsing, "LATCH keeps the selected pad going");
    const size_t kept = w.pulsePage.ticksKept();
    wait(300); w.shot();
    check(w.pulsePage.ticksKept() > kept + 5, "a latched pulse train keeps adding clicks to the strip");
    {   // the busiest the page gets: a 200 Hz train latched beside it fills the strip
        w.pulsePage.setLatch(1, true); inst.pulseSet(1, PF_RATE, 200); wait(1500); w.shot();
        QElapsedTimer t; t.start();
        for (int i = 0; i < 20; ++i) w.shot();
        const double ms = t.nsecsElapsed() / 20e6;
        std::printf("  info  pulse page redraw, two pads latched (%zu hits in the strip): %.1f ms\n", w.pulsePage.ticksKept(), ms);
        check(ms < 15.0, QString("with two pads latched the Pulse page redraws well inside a frame (%1 ms)").arg(ms, 0, 'f', 1));
        w.pulsePage.setLatch(1, false); inst.pulseSet(1, PF_RATE, pulseDefault(1).rate); w.pulsePage.select(5); wait(60);
    }
    tap(w, zn::at(zn::pad, 5)); wait(150);
    check(!inst.uiState().pulseLatched[5], "tapping the latched pad lets it go");
    w.shot(); tap(w, zn::pulsesync); wait(60);
    check(inst.uiState().set.pulseSync, "SYNC switches tempo sync on");
    w.shot(); tap(w, zn::pulsesync); wait(60);
    check(!inst.uiState().set.pulseSync, "and off");

    inst.cmd(EV_LOOP, LC_CLEAR); wait(60);   // a loop an earlier check left must not start playing
    press(w, Qt::Key_Return); wait(100);
    check(inst.uiState().loopState == L_COUNTIN, "Enter is the loop pedal on this page too");
    w.shot(); tap(w, zn::pulseloop); wait(100);
    check(inst.uiState().loopState == L_EMPTY, "and the LOOP soft key is the same pedal");

    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    const QJsonArray pa = sj["pulse"].toArray();
    check(pa.size() == kPulsePads && std::fabs(pa[5].toObject()["tone"].toDouble() - tone) < 1 && sj["pulseSelected"].toInt() == 5 && !sj.contains("beat"),
          "the pads' settings and the selected pad are kept in the saved state (and no beat)");
    inst.set(S_VOLUME, vol0); inst.set(S_BPM, bpm0);
    w.setPage(PG_PLAY); wait(60);
}
} // namespace selftest
