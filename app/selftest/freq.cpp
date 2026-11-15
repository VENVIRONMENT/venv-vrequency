// venv-vrequency self-test — the Freq page: its tab, the tone rows and warp cells by touch, the
// keys it takes (and the ones it leaves), HOLD / GATE / KEYS / SNAP, the views' redraw time,
// panic, and the tones in the state file and in sessions (old files without them still load).
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace td;

namespace selftest {
void testFreq(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- freq\n");
    w.setPage(PG_PULSE); w.shot();
    tap(w, zn::tab(pageName(PG_FREQ))); wait(60);
    check(w.page() == PG_FREQ && QString(pageTitle(PG_FREQ)) == "Freq", "the Freq tab, after Pulse, opens the Freq page");
    auto F = [&] { return inst.uiState().freq; };
    check(!F().hold && !F().gate && !inst.uiState().freqSounding, "nothing sounds until HOLD, GATE or a key");

    w.shot(); tap(w, zn::freqhold); wait(400);
    std::vector<float> tapBuf(9600);
    inst.readFreqScope(tapBuf.data(), 9600);
    float pk = 0; for (float v : tapBuf) pk = std::max(pk, std::fabs(v));
    check(F().hold && inst.uiState().freqSounding && pk > 0.05f, QString("HOLD sounds the tones and the Freq tap holds them (peak %1)").arg(pk, 0, 'f', 2));
    w.shot(); tap(w, zn::at(zn::freqtone, 2)); wait(60);
    check(w.freqPage.selected() == 2 && F().tone[2].on, "tapping an off row selects it and switches it on");
    w.shot(); tap(w, zn::at(zn::freqwave, 1)); wait(60);
    check(F().tone[1].wave == FW_TRI && w.freqPage.selected() == 1, "tapping WAVE steps the tone's wave (sine to tri)");
    w.shot(); tap(w, zn::at(zn::freqlevel, 0)); wait(60);
    const QRectF lz = w.zoneRect(zn::at(zn::freqlevel, 0));
    const float want = (float)((lz.width() / 2 - 4) / (lz.width() - 52));
    check(std::fabs(F().tone[0].level - want) < 0.02f, QString("the level bar sets the level where it is touched (%1)").arg(F().tone[0].level));
    w.shot(); tap(w, zn::at(zn::freqon, 2)); wait(60);
    check(!F().tone[2].on, "ON / off switches a tone out");
    w.shot(); tap(w, zn::freqmod); wait(60);
    check(F().mod == FM_FM, "a tap on 2 > 1 goes from OFF to FM");
    w.shot(); tap(w, zn::freqfilter); wait(60);
    check(F().warp.cutoff > 500 && F().warp.cutoff < 1500, QString("FILTER moves in octaves: its middle is about 850 Hz (%1)").arg(F().warp.cutoff, 0, 'f', 0));
    tap(w, zn::freqmod); wait(60); w.shot(); tap(w, zn::freqmod); wait(60); w.shot();
    inst.freqSet(-1, FF_FILTER, kWarpCutMax); wait(30);
    check(F().mod == FM_OFF, "and two more taps go round through RING to OFF");

    w.freqPage.select(1); inst.freqSet(1, FF_HZ, 300); wait(40);
    press(w, Qt::Key_Right); wait(40);
    check(std::fabs(F().tone[1].hz - 300.5f) < 1e-3f, "Right moves the selected tone up 0.5 Hz");
    key(w, Qt::Key_Shift, true); press(w, Qt::Key_Left); key(w, Qt::Key_Shift, false); wait(40);
    check(std::fabs(F().tone[1].hz - 290.5f) < 1e-3f, "Shift+Left moves it down 10 Hz");
    press(w, Qt::Key_Up); wait(40);
    check(std::fabs(F().tone[1].hz - 290.5f * std::pow(2.0f, 1 / 12.0f)) < 0.01f, "Up moves it a semitone");
    press(w, Qt::Key_4); wait(40);
    check(w.freqPage.selected() == 3, "4 selects tone 4");
    w.freqPage.select(1); inst.freqSet(1, FF_HZ, 331); w.shot(); tap(w, zn::freqsnap); wait(40);
    check(std::fabs(F().tone[1].hz - 330) < 0.01f, "SNAP puts tone 2 at 3:2 of tone 1 (331 Hz to 330)");

    key(w, Qt::Key_Q, true); wait(80);
    check(inst.uiState().voices > 0 && F().tone[0].hz == 220, "with KEYS off a letter plays the synth as on every page");
    key(w, Qt::Key_Q, false);
    press(w, Qt::Key_Backspace); wait(80);
    check(!F().hold && !inst.uiState().freqSounding, "Backspace's panic lets HOLD go and silences Freq");
    w.shot(); tap(w, zn::freqkeys); wait(60);
    key(w, Qt::Key_A, true); wait(150);
    const float noteHz = (float)midiToHz(inst.uiState().set.tuning.noteFor(1, 0, false));
    check(F().keys && inst.uiState().freqKeyHeld && std::fabs(F().tone[0].hz - noteHz) < 0.01f && inst.uiState().voices == 0,
          QString("KEYS: A sets tone 1 to the string's note (%1 Hz) and sounds it, not the synth").arg(noteHz, 0, 'f', 2));
    key(w, Qt::Key_A, false); w.shot(); wait(80);
    check(!inst.uiState().freqKeyHeld, "letting the key go stops it (HOLD is off)");
    w.shot(); tap(w, zn::freqkeys); wait(40);

    w.shot(); tap(w, zn::freqgate); wait(40);
    const float r0 = F().rate;
    w.shot(); tap(w, zn::plus(zn::freqrate)); wait(40);
    check(F().gate && F().rate > r0 && inst.uiState().freqSounding, QString("GATE pulses the tones; + steps the rate (%1 to %2 Hz)").arg(r0).arg(F().rate));
    {   // the busiest the page gets: four tones, noise, FM, crush
        for (int i = 0; i < kFreqTones; ++i) inst.freqSet(i, FF_ON, 1);
        inst.freqSet(3, FF_WAVE, FW_NOISE); inst.freqSet(-1, FF_MOD, FM_FM); inst.freqSet(-1, FF_CRUSH, 0.5f); inst.freqSet(-1, FF_HOLD, 1);
        wait(800); w.shot();
        QElapsedTimer t; t.start();
        for (int i = 0; i < 20; ++i) w.shot();
        const double ms = t.nsecsElapsed() / 20e6;
        std::printf("  info  freq page redraw, four tones sounding: %.1f ms\n", ms);
        check(ms < 15.0, QString("with four tones sounding the Freq page redraws well inside a frame (%1 ms)").arg(ms, 0, 'f', 1));
    }
    press(w, Qt::Key_Return); wait(100);
    check(inst.uiState().loopState == L_COUNTIN, "Enter is still the loop pedal on this page");
    inst.cmd(EV_LOOP, LC_CLEAR); inst.cmd(EV_PANIC); wait(80);

    w.freqPage.select(2); w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    const QJsonObject fj = sj["freq"].toObject();
    check(fj["tones"].toArray().size() == kFreqTones && fj["tones"].toArray()[3].toObject()["wave"].toString() == "noise" &&
          fj["mod"].toString() == "fm" && !fj.contains("hold") && sj["freqSelected"].toInt() == 2,
          "the tones, waves, 2 > 1 and warp are in the state file (HOLD is not)");
    SessionData sd; inst.snapshot(sd);
    SessionData back;
    QJsonObject so = store::sessionToJson(sd);
    check(store::sessionFromJson(so, back) && sameFreq(back.freq, sd.freq), "a session keeps the Freq tones exactly");
    so.remove("freq");
    check(store::sessionFromJson(so, back) && sameFreq(back.freq, FreqState()), "a session from before Freq loads, with the default tones");
    const FreqState d;   // the defaults again, for the checks that follow
    for (int i = 0; i < kFreqTones; ++i) {
        inst.freqSet(i, FF_ON, d.tone[i].on); inst.freqSet(i, FF_WAVE, (float)d.tone[i].wave);
        inst.freqSet(i, FF_HZ, d.tone[i].hz); inst.freqSet(i, FF_LEVEL, d.tone[i].level);
    }
    inst.freqSet(-1, FF_MOD, (float)d.mod); inst.freqSet(-1, FF_CRUSH, d.warp.crush); inst.freqSet(-1, FF_RATE, d.rate); inst.freqSet(-1, FF_GATE, 0);
    wait(40);
    check(sameFreq(inst.uiState().freq, d), "(the default tones are back)");
    w.setPage(PG_PLAY); wait(60);
}
} // namespace selftest
