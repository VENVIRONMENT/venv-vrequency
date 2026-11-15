// venv-vrequency self-test — tour: every way in and out, by touch, by key and over the control socket.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QJsonObject>

namespace selftest {
// every way in
void tourWaysIn(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    check(!t.isOpen() && !ctx.tourState()["open"].toBool(), "the tour starts closed");
    w.setPage(PG_HELP); w.shot();
    check(w.zoneNames().contains(zn::help_tour), "the Help page has a Tour button");
    tourTap(w, zn::help_tour);
    settleTour(w);
    check(t.isOpen() && w.page() == PG_HELP, "the Help page's Tour button opens the tour (the real page stays where it was)");
    press(w, Qt::Key_Escape);
    check(!t.isOpen() && w.page() == PG_HELP, "Esc leaves the tour and the Help page is still there");
    w.setPage(PG_PLAY); w.shot();
    ctrl(w, Qt::Key_O);
    settleTour(w);
    check(t.isOpen() && w.page() == PG_PLAY, "Ctrl+O opens the tour from any page");
    tourTap(w, zn::tour_close);
    check(!t.isOpen(), "Leave tour closes it by touch");
    {
        QJsonObject r = ctx.call(R"({"id":1,"cmd":"tour","action":"open","page":"pulse","element":"pulselatch"})");
        settleTour(w);
        const QJsonObject st = ctx.tourState();
        check(r["ok"].toBool() && st["open"].toBool() && st["page"].toString() == "pulse" && st["selected"].toString() == zn::pulselatch,
              "the control socket opens the tour on a page and a control (Rook: let me show you)");
        check(w.page() == PG_PLAY, "and the real page did not change");
        r = ctx.call(R"({"cmd":"tour","action":"close"})");
        check(r["ok"].toBool() && !t.isOpen() && !ctx.tourState()["open"].toBool(), "the control socket closes it");
        r = ctx.call(R"({"cmd":"tour","page":"play","element":"octave_up"})");
        settleTour(w);
        check(ctx.tourState()["page"].toString() == "synth" && ctx.tourState()["selected"].toString() == zn::octave_up, "page names are the app's (play is the Synth page)");
        r = ctx.call(R"({"cmd":"tour","page":"synth","element":"concepts.recall"})");
        settleTour(w);
        check(ctx.tourState()["concept"].toString() == "recall", "a concept can be opened the same way (state.tour.concept)");
        r = ctx.call(R"({"cmd":"page","name":"sound"})");
        check(!t.isOpen() && w.page() == PG_SOUND, "the page command makes the tour step aside");
        w.setPage(PG_PLAY);
    }
}
} // namespace selftest
