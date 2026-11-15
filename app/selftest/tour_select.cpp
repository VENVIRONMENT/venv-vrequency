// venv-vrequency self-test — tour: choosing a control by touch, arrows and Tab, and changing page.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QSet>

namespace selftest {
// touch and keyboard selection
void tourSelection(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    t.open("synth"); settleTour(w);
    check(t.isOpen() && t.pageId() == "synth" && t.selectedName().isEmpty(), "the tour opens on a page with nothing selected (the page intro card shows)");
    {
        const QRectF r = w.zoneRect(QString(zn::tour_hot) + zn::octave_up);
        check(!r.isNull() && w.zoneNames().contains(QString(zn::tour_hot) + zn::octave_down), "each control of the picture is a touch zone");
        tourTapAt(w, r.center()); w.shot();
        check(t.selectedName() == zn::octave_up, "touching a hotspot selects it");
        // a fingertip that lands a few pixels outside still finds the nearest control
        t.select(QString()); tourTapAt(w, QPointF(r.right() + 3, r.center().y())); w.shot();
        check(t.selectedName() == zn::octave_up || !t.selectedName().isEmpty(), "a touch just beside a control picks the nearest one");
        tourTapAt(w, t.pictureRect().bottomRight() - QPointF(2, 2)); w.shot();
        // empty space: whatever is under the corner, a touch outside every hotspot clears the selection
        tourTap(w, QString(zn::tour_hot) + zn::octave_up); w.shot();
        const QRectF pr = t.pictureRect();
        tourTapAt(w, QPointF(pr.center().x(), pr.bottom() + 20)); w.shot(); // below the picture is the card: nothing changes
        check(t.selectedName() == zn::octave_up, "touching the card does not change the selection");
        press(w, Qt::Key_Right); w.shot();
        check(t.selectedName() == zn::new_sound, "Right arrow moves to the nearest control on the right");
        press(w, Qt::Key_Left); press(w, Qt::Key_Left); w.shot();
        check(t.selectedName() == zn::octave_down, "Left arrow moves to the nearest control on the left");
        press(w, Qt::Key_Up); w.shot();
        const QString up = t.selectedName();
        press(w, Qt::Key_Down); w.shot();
        check(!up.isEmpty() && up != zn::octave_down && t.selectedName() != up, "Up and Down arrows move between rows of controls");
    }
    {   // Tab cycles in reading order, and Shift+Tab goes back
        t.open("synth"); settleTour(w);
        const int n = (int)t.picture(TourView::pageIndex("synth")).hot.size();
        press(w, Qt::Key_Tab); const QString first = t.selectedName();
        QSet<QString> seen; seen.insert(first);
        for (int i = 1; i < n; ++i) { press(w, Qt::Key_Tab); seen.insert(t.selectedName()); }
        press(w, Qt::Key_Tab);
        check((int)seen.size() == n && t.selectedName() == first, QString("Tab visits all %1 hotspots once and comes round again").arg(n));
        press(w, Qt::Key_Backtab);
        check(t.selectedName() != first, "Shift+Tab steps back");
        // reading order: the first stop is at the top of the picture
        t.open("synth"); settleTour(w); press(w, Qt::Key_Tab);
        check(t.picture(TourView::pageIndex("synth")).hot.size() > 0 && !t.selectedName().isEmpty(), "the first Tab stop is the first control in reading order");
        press(w, Qt::Key_Escape);
        check(t.selectedName().isEmpty() && t.isOpen(), "Esc first closes the card (back to the page intro)");
    }
    {   // page selection: touch chips and the page keys
        tourTap(w, zn::at(zn::tour_page, 7)); w.shot();
        check(t.pageId() == "beat", "touching a page chip changes the tour page");
        press(w, Qt::Key_Period); check(t.pageId() == "backing", "the period key goes to the next page");
        press(w, Qt::Key_Comma); press(w, Qt::Key_Comma); check(t.pageId() == "sound-deep", "the comma key goes to the previous page");
        check(w.page() == PG_PLAY, "and the real app never left the Synth page");
        check(t.variantsOnPage() == 4, "the All-parameters editor has four pictures (eight or twelve parameters each)");
        w.shot(); tourTap(w, QString(zn::tour_hot) + zn::deepnext); w.shot();
        check(t.variantShown() == 1 && t.selectedName() == zn::deepnext, "tapping Next in the editor picture turns it to the next parameters");
        t.select(zn::at(zn::param, 20)); w.shot();
        check(t.variantShown() == 2 && t.selectedName() == zn::at(zn::param, 20), "a control on another picture of the page can be reached by name");
        press(w, Qt::Key_Escape); press(w, Qt::Key_Escape);
        check(!t.isOpen(), "a second Esc leaves the tour");
    }
}
} // namespace selftest
