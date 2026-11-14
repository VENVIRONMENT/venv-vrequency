// venv-vrequency self-test — guided help: highlights, the zone listing and the chat dock, part by part (guided_*.cpp).
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include <QTemporaryDir>
#include <cstdio>

using namespace td;

namespace selftest {
void testGuidedHelp(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- guided help: highlights, zones and the chat dock\n");
    QTemporaryDir gdir;
    const QString gsock = gdir.path() + "/vrequency.sock";
    ControlServer gctl(w, inst);
    check(gctl.start(gsock), "a control socket for the guided-help checks");
    Client c, c2;
    check(c.open(gsock) && c2.open(gsock), "two clients connect");
    inst.cmd(EV_PANIC);
    w.guide.dockClose();
    w.guide.clearHighlight();
    w.setPage(PG_PLAY); wait(150); w.shot();

    Link link{gctl, c, c2, gsock};
    guideZoneListing(ctx, link);
    guideHighlights(ctx, link);
    guideHostileArguments(ctx, link);
    guideDock(ctx, link);
    guideTicker(ctx);
    guideDockAvoidsHighlight(ctx, link);
    guideDockAvoidsEveryZone(ctx);
    guideHelpPage(ctx, link);

    gctl.stop();
    w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY);
    inst.cmd(EV_PANIC);
    wait(200);
}
} // namespace selftest
