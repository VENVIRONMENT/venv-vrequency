// venv-vrequency self-test — the tour and onboarding, part by part (tour_*.cpp).
#include "app/selftest/selftest.h"
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <cstdio>

using namespace td;
using namespace selftest;

void runTourTests(MainWindow &w, Instrument &inst, AudioOut &audio, const std::function<void(bool, const QString &)> &check) {
    std::printf("- tour and onboarding (0.13.0)\n");
    TourView &t = w.tour();
    ControlServer ctl(w, inst);
    inst.cmd(EV_PANIC); wait(100);
    w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY); w.shot();
    t.close();

    TourContext ctx{w, inst, audio, check, t, ctl, helpCards()};
    tourCards(ctx);
    tourWaysIn(ctx);
    tourPictures(ctx);
    tourSelection(ctx);
    tourReachesNothing(ctx);
    tourAskRook(ctx);
    tourConcepts(ctx);
    tourGuidedWalk(ctx);
    tourFirstRunOffer(ctx);
    tourRemembersLastPage(ctx);
    tourDamagedCards(ctx);
    tourCardsFit(ctx);
    tourPaintTime(ctx);
    tourHostileArguments(ctx);

    w.setPage(PG_PLAY); w.guide.dockClose(); t.close();
    inst.cmd(EV_PANIC);
    wait(150);
}
