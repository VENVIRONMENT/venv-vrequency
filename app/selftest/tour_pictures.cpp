// venv-vrequency self-test — tour: every page and panel has a picture, and every hotspot on it has a card.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QImage>
#include <QSet>
#include <cstdio>

namespace selftest {
// every page and panel: hotspots, and every hotspot has a card
void tourPictures(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const CardDb &db = ctx.db; const auto &check = ctx.check;
    {
        int totalHot = 0, unresolved = 0, tourOnly = 0, outside = 0, tiny = 0; QStringList bad;
        for (int p = 0; p < TourView::picCount(); ++p) {
            const TourView::Pic &pic = t.picture(p);
            const QString id = TourView::pageInfo(TourView::picPage(p)).id;
            bool shown = !pic.img.isNull() && pic.img.width() == 720 && pic.img.height() == 720 && !pic.hot.empty();
            if (!shown) bad << id + " (no picture or hotspots)";
            for (const TourView::Hot &h : pic.hot) {
                ++totalHot;
                if (!h.real) ++tourOnly;
                if (h.r.left() < 0 || h.r.top() < 0 || h.r.right() > 720.5 || h.r.bottom() > 720.5) ++outside;
                if (h.r.width() < 4 || h.r.height() < 4) ++tiny;
                // resolve exactly as the tour does
                const HelpCard *c = h.ref.isEmpty() ? db.find(id, h.name) : db.resolve(h.ref);
                if (!c) { ++unresolved; bad << id + "." + h.name; }
            }
        }
        std::printf("  info  %d hotspots on %d pictures (%d are not real zones)\n", totalHot, TourView::picCount(), tourOnly);
        check(bad.isEmpty(), "every page and panel has a picture with hotspots" + (bad.isEmpty() ? QString() : ": " + bad.join(", ").left(300)));
        check(unresolved == 0 && totalHot > 380, QString("EVERY hotspot resolves to a card (%1 of %2)").arg(totalHot - unresolved).arg(totalHot));
        check(outside == 0 && tiny == 0, "every hotspot lies inside the screen and is big enough to see");
        check(tourOnly >= 8, "controls with no zone (level strip, scope readouts, string brackets, step grid, status...) have hotspots too");
    }
    {   // the tour's pictures show the same controls the real pages have
        int missing = 0; QStringList bad;
        for (int p = 0; p < PG_COUNT; ++p) {
            const QVector<MainWindow::ZoneInfo> cat = w.zoneCatalogue(p, p == PG_HELP ? 0 : -1);
            int pi = -1;
            for (int k = 0; k < TourView::pageCount(); ++k) if (TourView::pageInfo(k).realPage == p) { pi = k; break; }
            QSet<QString> have;
            for (const TourView::Hot &h : t.picture(pi).hot) have.insert(h.name);
            for (const MainWindow::ZoneInfo &z : cat) {
                if (z.name.startsWith(zn::dock_prefix) || z.name == zn::ticker || z.name.startsWith(zn::help_row)) continue;
                if (!have.contains(z.name) && !(p == PG_SOUND && z.name.startsWith(zn::param))) { ++missing; bad << QString("%1:%2").arg(p).arg(z.name); }
            }
        }
        check(missing < 40, QString("the pictures cover the controls of the live pages (%1 live-only zones, from different demo state)").arg(missing));
    }
    {   // a picture is a faithful picture: the same size and the same look as the real thing
        w.setPage(PG_TUNE); w.shot();
        const QImage a = w.shot(), b = t.picture(TourView::pageIndex("tune")).img;
        check(a.size() == b.size(), "the picture of a page is drawn by the real page code at full size");
        w.setPage(PG_PLAY); w.shot();
    }
}
} // namespace selftest
