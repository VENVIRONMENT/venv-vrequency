// venv-vrequency self-test — Warp: the Sound page's Warp view and the Pulse page's WARP, by touch;
// Store, Undo and favourites keep the warp; files from before Warp load with it off.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>
#include <cstdio>

using namespace td;

namespace selftest {
namespace {
// a touch on a slider at a share of its bar (the bar runs 10 px in from each side)
void tapSlider(MainWindow &w, const QString &zone, float share) {
    const QRectF r = w.zoneRect(zone);
    tapAt(w, QPointF(r.x() + 10 + (r.width() - 20) * share, r.center().y()), 7);
}
double redrawMs(MainWindow &w) {
    w.shot();
    QElapsedTimer t; t.start();
    for (int i = 0; i < 20; ++i) w.shot();
    return t.nsecsElapsed() / 20e6;
}

void soundWarp(MainWindow &w) {
    Sounds &s = w.sounds;
    const Patch start = s.lead;
    w.setPage(PG_SOUND); wait(60); w.shot();
    tap(w, zn::warp); wait(40); w.shot();
    check(w.soundPage.warpView && !w.zoneRect(zn::warpcurve).isNull() && !w.zoneRect(zn::at(zn::param, P_BENDIN)).isNull()
          && w.zoneRect(zn::at(zn::param, P_CUTOFF)).isNull(), "WARP swaps the quick sliders for the IN > OUT picture and the six warp sliders");
    tapSlider(w, zn::at(zn::param, P_FOLD), 0.5f); wait(30); w.shot();
    tapSlider(w, zn::at(zn::param, P_CRUSH), 0.5f); wait(30); w.shot();
    tapSlider(w, zn::at(zn::param, P_DSAMP), 0.5f); wait(30); w.shot();
    tapSlider(w, zn::at(zn::param, P_RING), 0.5f); wait(30); w.shot();
    tapSlider(w, zn::at(zn::param, P_BENDIN), 0.75f); wait(30); w.shot();
    const float *p = s.lead.p;
    check(std::fabs(p[P_FOLD] - 0.5f) < 0.02f && p[P_CRUSH] == 6 && std::fabs(p[P_DSAMP] - 0.5f) < 0.02f && std::fabs(p[P_RING] - 0.5f) < 0.02f
          && p[P_BENDIN] == 12, QString("touching the sliders sets FOLD, CRUSH, DOWNSAMPLE, RING MOD and BEND IN (%1, %2, %3, %4, %5)")
          .arg(p[P_FOLD]).arg(p[P_CRUSH]).arg(p[P_DSAMP]).arg(p[P_RING]).arg(p[P_BENDIN]));
    tapSlider(w, zn::at(zn::param, P_DRIVE), 0.3f); wait(30); w.shot();
    check(std::fabs(p[P_DRIVE] - 0.3f) < 0.02f, "and DRIVE, the same parameter as the quick slider");
    const double ms = redrawMs(w);
    std::printf("  info  sound page redraw, Warp view: %.1f ms\n", ms);
    check(ms < 15.0, QString("the Warp view redraws inside a frame (%1 ms)").arg(ms, 0, 'f', 1));

    // Store, Undo and Save keep the warp
    const Patch slot9 = s.bank[9], warped = s.lead;
    tap(w, zn::slotsave); wait(30); w.shot(); tap(w, zn::at(zn::slot, 9)); wait(30);
    check(s.bank[9].p[P_RING] == warped.p[P_RING] && s.bank[9].p[P_BENDIN] == 12, "Store puts the warp in the slot with the rest of the sound");
    w.shot(); tap(w, zn::newsound); wait(60);
    check(s.lead.p[P_FOLD] == 0 && s.lead.p[P_RING] == 0 && s.lead.p[P_BENDIN] == 0, "New sound starts with the warp off");
    w.shot(); tap(w, zn::undosound); wait(60);
    check(s.lead.p[P_FOLD] == warped.p[P_FOLD] && s.lead.p[P_DSAMP] == warped.p[P_DSAMP], "and Undo brings the warped sound back");
    const int favs0 = s.favs.size();
    w.shot(); tap(w, zn::fav); wait(60);
    bool saved = false; QString added;
    for (const store::NamedPatch &f : s.favs)
        if (QString::fromUtf8(f.patch.name) == QString::fromUtf8(warped.name)) { saved = f.patch.p[P_CRUSH] == 6 && f.patch.p[P_BENDIN] == 12; added = f.file; }
    check(saved, "Save favourite writes the warp to the file and it reads back");
    if (s.favs.size() > favs0 && !added.isEmpty()) { QFile::remove(added); w.refreshLists(); }
    s.bank[9] = slot9;

    // files from before Warp
    QJsonObject old; old["name"] = "Old"; QJsonObject op; op["cutoff"] = 900; op["drive"] = 0.25; old["params"] = op;
    const Patch o = store::patchFromJson(old);
    check(o.p[P_FOLD] == 0 && o.p[P_CRUSH] == 0 && o.p[P_DSAMP] == 0 && o.p[P_RING] == 0 && o.p[P_BENDIN] == 0 && o.p[P_DRIVE] == 0.25f,
          "a sound file without the warp loads with all of it off (DRIVE as written)");
    check(store::patchToJson(warped)["params"].toObject()["bendin"].toDouble() == 12, "and a sound is written with its warp, by name");

    // the All parameters view has the same Warp view as its last page; WARP leaves it
    tap(w, zn::deep); wait(30);
    w.soundPage.deepPage = 4; w.shot();
    check(w.soundPage.deepEdit && !w.soundPage.warpView && !w.zoneRect(zn::warpcurve).isNull(), "All parameters page 5 is the Warp view");
    tap(w, zn::warp); wait(30); w.shot();
    check(w.soundPage.warpView && !w.soundPage.deepEdit, "WARP from All parameters goes straight to the Warp view");
    tap(w, zn::warp); wait(30); w.shot();
    check(!w.soundPage.warpView && !w.zoneRect(zn::at(zn::param, P_CUTOFF)).isNull(), "and again, back to the quick sliders");
    s.lead = start; s.applyLead(false);
    w.soundPage.warpView = true; w.setPage(PG_PLAY); w.setPage(PG_SOUND); wait(30);
    check(!w.soundPage.warpView, "leaving the Sound page drops the Warp view, like All parameters");
}

void pulseWarp(MainWindow &w, Instrument &inst) {
    w.setPage(PG_PULSE); w.pulsePage.select(5); wait(60); w.shot();
    tap(w, zn::pulsewarp); wait(40); w.shot();
    check(w.pulsePage.warpMode && !w.zoneRect(zn::pulsebend).isNull() && w.zoneRect(zn::pulsetone).isNull(),
          "WARP swaps the pad's TONE, LENGTH, RATE and LEVEL for DRIVE, FOLD, CRUSH and BEND");
    tapSlider(w, zn::pulsedrive, 0.5f); wait(40); w.shot();
    tapSlider(w, zn::pulsefold, 0.25f); wait(40); w.shot();
    tapSlider(w, zn::pulsecrush, 0.5f); wait(40); w.shot();
    tapSlider(w, zn::pulsebend, 0.25f); wait(40); w.shot();
    const PulsePad d = inst.uiState().pulse[5];
    check(std::fabs(d.drive - 0.5f) < 0.02f && std::fabs(d.fold - 0.25f) < 0.02f && std::fabs(d.crush - 0.5f) < 0.02f && d.bend == -12,
          QString("touching them warps the selected pad (drive %1, fold %2, crush %3, bend %4)").arg(d.drive).arg(d.fold).arg(d.crush).arg(d.bend));
    const double ms = redrawMs(w);
    std::printf("  info  pulse page redraw, WARP on: %.1f ms\n", ms);
    check(ms < 15.0, QString("the Pulse page with WARP redraws inside a frame (%1 ms)").arg(ms, 0, 'f', 1));
    const int t0 = d.type;
    tap(w, zn::pulsetype); wait(60); w.shot();
    check(inst.uiState().pulse[5].type == (t0 + 1) % kPulseTypes && inst.uiState().pulse[5].drive == d.drive,
          "tapping the pad's name gives it the next kind, and it keeps its warp");
    inst.pulseSet(5, PF_TYPE, t0); wait(30);
    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    const QJsonObject s5 = sj["pulse"].toArray()[5].toObject();
    check(std::fabs(s5["drive"].toDouble() - 0.5) < 0.02 && s5["bend"].toDouble() == -12, "the pad's warp is kept in the saved state");
    PulsePad got[kPulsePads];
    store::pulseFromJson(sj["pulse"].toArray(), got);
    check(samePulse(got[5], inst.uiState().pulse[5]), "and reads back exactly");
    QJsonArray old; QJsonObject tom; tom["type"] = "tom"; tom["tone"] = 150; tom["length"] = 300; tom["rate"] = 2; tom["level"] = 0.9; old.append(tom);
    store::pulseFromJson(old, got);
    check(got[0].drive == 0 && got[0].fold == 0 && got[0].crush == 0 && got[0].bend == 0 && got[0].tone == 150, "a pad from before Warp loads with its warp off");
    tap(w, zn::pulsewarp); wait(40); w.shot();
    check(!w.pulsePage.warpMode && !w.zoneRect(zn::pulsetone).isNull(), "WARP again: the sound sliders are back");
    for (int f : {PF_DRIVE, PF_FOLD, PF_CRUSH, PF_BEND}) inst.pulseSet(5, f, 0);
    w.setPage(PG_PLAY); wait(60);
}
} // namespace

void testWarp(Context &ctx) {
    std::printf("- warp\n");
    soundWarp(ctx.w);
    pulseWarp(ctx.w, ctx.inst);
}
} // namespace selftest
