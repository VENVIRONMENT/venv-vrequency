// venv-vrequency self-test — tour: it redraws in time, and hostile control-socket arguments do no harm.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QElapsedTimer>
#include <QJsonObject>

using namespace td;

namespace selftest {
// paint time
void tourPaintTime(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    double worst = 0; QString worstPage;
    t.open("synth"); settleTour(w);
    for (int p = 0; p < TourView::pageCount(); ++p) {
        tourTap(w, zn::at(zn::tour_page, p)); w.shot();
        const auto &hot = t.picture(p).hot;
        if (!hot.empty()) t.select(hot[hot.size() / 2].name);
        QElapsedTimer pt; pt.start();
        for (int i = 0; i < 12; ++i) w.shot();
        const double ms = pt.elapsed() / 12.0;
        if (ms > worst) { worst = ms; worstPage = TourView::pageInfo(p).id; }
    }
    t.selectConcept("recall");
    { QElapsedTimer pt; pt.start(); for (int i = 0; i < 12; ++i) w.shot(); const double ms = pt.elapsed() / 12.0; if (ms > worst) { worst = ms; worstPage = "concepts"; } }
    t.close();
    check(worst < 25.0, QString("the tour redraws well inside a 40 ms frame (slowest: %1, %2 ms)").arg(worstPage).arg(worst, 0, 'f', 1));
}

// hostile arguments
void tourHostileArguments(TourContext &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; TourView &t = ctx.t; const auto &check = ctx.check;
    const char *reqs[] = {
        R"({"cmd":"tour","action":5})", R"({"cmd":"tour","action":"explode"})", R"({"cmd":"tour","page":[1]})", R"({"cmd":"tour","page":"nowhere"})",
        R"({"cmd":"tour","page":{"a":1}})", R"({"cmd":"tour","element":[1,2]})", R"({"cmd":"tour","guided":"yes"})", R"({"cmd":"tour","guided":3})",
        R"({"cmd":"tour","page":null,"element":null})", R"({"cmd":"tour","action":null})"};
    int refused = 0, handled = 0;
    for (const char *r : reqs) { const QJsonObject o = ctx.call(r); if (!o["ok"].toBool()) ++refused; ++handled; settleTour(w); t.close(); }
    // (null arguments mean "not given": those two are accepted)
    check(handled == 10 && refused >= 8, QString("hostile tour arguments are refused without harm (%1 of 10 refused, none crashed)").arg(refused));
    const QByteArray big = QByteArray(R"({"cmd":"tour","page":")") + QByteArray(5000, 'x') + "\"}";
    check(!ctx.call(big)["ok"].toBool(), "a 5000-character page name is refused");
    const QByteArray big2 = QByteArray(R"({"cmd":"tour","element":")") + QByteArray(5000, 'y') + "\"}";
    check(!ctx.call(big2)["ok"].toBool(), "a 5000-character element is refused");
    QJsonObject r = ctx.call(R"({"cmd":"tour","page":"beat","element":"no_such_control"})");
    settleTour(w);
    check(r["ok"].toBool() && ctx.tourState()["page"].toString() == "beat" && ctx.tourState()["selected"].isNull(), "an unknown element opens the page with nothing selected");
    r = ctx.call(R"({"cmd":"tour","page":"beat","element":"pad0","extra":{"x":1}})");
    settleTour(w);
    check(r["ok"].toBool() && ctx.tourState()["selected"].toString() == zn::at(zn::pad, 0), "unknown extra fields are ignored");
    r = ctx.call(R"({"cmd":"tour","page":"  BEAT  "})");
    settleTour(w);
    check(r["ok"].toBool() && ctx.tourState()["page"].toString() == "beat", "page names are not case or space sensitive");
    for (int i = 0; i < 6; ++i) { ctx.call(R"({"cmd":"tour","action":"open"})"); ctx.call(R"({"cmd":"tour","action":"close"})"); }
    check(!t.isOpen(), "opening and closing over and over leaves it closed");
    t.close();
    check(inst.uiState().droppedEvents == 0, "none of it dropped an audio event");
}
} // namespace selftest
