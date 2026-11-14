// venv-vrequency — drawing the tour's concept list and the card regions beside the picture.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

void TourView::paintConceptList() {
    const QRectF pr = pictureRect();
    const auto &cs = cards().concepts;
    const int rowH = 18, rows = (int)(pr.height() / rowH);
    w.ui.frame(grow(pr, 1), LOW, BG);
    conceptScroll = qBound(0, conceptScroll, qMax(0, (int)cs.size() - rows));
    if (cs.empty()) { w.ui.txt(pr, "No concepts available.", 13, MID); return; }
    for (int i = 0; i < rows; ++i) {
        const int ci = conceptScroll + i;
        if (ci >= (int)cs.size()) break;
        const QRectF r(pr.x() + 4, pr.y() + i * rowH + 1, pr.width() - 8, rowH - 2);
        const bool on = cs[ci].key == conceptId;
        if (on) w.ui.g->fillRect(r, INK);
        w.ui.txt(r.adjusted(6, 0, -4, 0), w.ui.fit(cs[ci].title.toUpper(), 12, r.width() - 12), 12, on ? BG : INK, Qt::AlignLeft | Qt::AlignVCenter, on);
    }
    if (conceptScroll > 0) w.ui.tag(QRectF(pr.right() - 58, pr.top() + 2, 54, 14), QString::fromUtf8("▴ less"), 10);
    if (conceptScroll + rows < (int)cs.size()) w.ui.tag(QRectF(pr.right() - 62, pr.bottom() - 16, 58, 14), QString::fromUtf8("▾ more"), 10);
    w.ui.zone(pr, "tour_conceptlist", [this, pr, rowH](QPointF p, int ph) {
        static qreal startY = 0; static int startScroll = 0; static bool dragged = false;
        if (ph == PH_PRESS) { startY = p.y(); startScroll = conceptScroll; dragged = false; return; }
        if (ph == PH_MOVE) {
            if (std::fabs(p.y() - startY) > 6) dragged = true;
            if (dragged) conceptScroll = qMax(0, startScroll - (int)std::round((p.y() - startY) / rowH));
            return;
        }
        if (!dragged) {
            const int ci = conceptScroll + (int)((p.y() - pr.y()) / rowH);
            const auto &cc = cards().concepts;
            if (ci >= 0 && ci < (int)cc.size()) selectConcept(cc[ci].key);
        }
    });
}

void TourView::paintCardRegions() {
    HelpCard intro;
    const HelpCard *c = nullptr;
    QString crumb;
    if (conceptOn) {
        c = conceptId.isEmpty() ? nullptr : cards().conceptCard(conceptId);
        crumb = "concept";
        if (!c) {
            intro.key = "concepts"; intro.kind = "area"; intro.title = "Concepts";
            intro.summary = "The ideas that run through the whole instrument.";
            intro.does = "Each concept is explained once and used everywhere: scale lock, the loop pedal, bar lines, the command layer, and Recall. Pick one from the list.";
            intro.recall = "Tour, Concepts. Or Help page, Tour, Concepts.";
            intro.use.push_back({"Touch", "Tap a concept in the list."});
            intro.use.push_back({"Keys", "Up and Down move through the list. Esc goes back to the pictures."});
            intro.keys = {"Up", "Down", "Tab", "Esc"};
            intro.ask = "Which idea should I learn first?";
            c = &intro;
        }
    } else if (selIdx >= 0 && selIdx < (int)pics[picIndex()].hot.size()) {
        c = cardFor(pics[picIndex()].hot[selIdx]);
        crumb = QString::fromLatin1(pageInfo(pageIdx).label);
    } else {
        intro = introCard(pageIdx);
        c = &intro;
        crumb = "tour";
    }
    // the explanation of Recall, always one touch away: the concept for the card being shown
    const Fit f = fitFor(*c, false);
    const int px = f.px;
    w.ui.g->fillRect(QRectF(8, 447, 704, 1), GRAT);
    w.ui.g->fillRect(QRectF(432, 454, 1, 258), GRAT);
    const QRectF aA = kRegionA, aB1 = kRegionB1, aB2 = kRegionB2;
    const qreal hA = regionN(0, *c, px, 0, false, aA, crumb), hB1 = regionN(1, *c, px, 0, false, aB1, crumb), hB2 = regionN(2, *c, px, 0, false, aB2, crumb);
    scrollA = qBound(0, scrollA, qMax(0, (int)hA - (int)aA.height()));
    scrollB = qBound(0, scrollB, qMax(0, (int)qMax(hB1 - aB1.height(), hB2 - aB2.height())));
    auto scrollZone = [&](const QRectF &a, const char *name, bool isA) {
        w.ui.zone(a, name, [this, isA](QPointF p, int ph) {
            if (ph == PH_PRESS) { dragY = (int)p.y(); return; }
            if (ph == PH_MOVE) {
                const int d = dragY - (int)p.y();
                dragY = (int)p.y();
                if (isA) scrollA = qMax(0, scrollA + d); else scrollB = qMax(0, scrollB + d);
            }
        });
    };
    scrollZone(aA, "tour_scroll_a", true);
    scrollZone(aB1, "tour_scroll_b1", false);
    scrollZone(aB2, "tour_scroll_b2", false);
    auto draw = [&](int which, const QRectF &a, int scroll, qreal total) {
        w.ui.g->save();
        w.ui.g->setClipRect(a);
        regionN(which, *c, px, scroll, true, a, crumb);
        w.ui.g->restore();
        if (total > a.height()) { // never clip silently: say there is more, and which way
            if (scroll > 0) w.ui.tag(QRectF(a.right() - 70, a.top() + 2, 66, 14), QString::fromUtf8("▴ less"), 10);
            if (scroll < total - a.height() - 1) w.ui.tag(QRectF(a.right() - 70, a.bottom() - 16, 66, 14), QString::fromUtf8("▾ more"), 10);
        }
    };
    draw(0, aA, scrollA, hA);
    draw(1, aB1, scrollB, hB1);
    draw(2, aB2, scrollB, hB2);
    // what the keys do, always in the same place
    w.ui.txt(QRectF(16, 678, 410, 14), guided ? "Arrows move   Tab cycles   Enter asks   , . steps" : "Arrows move   Tab cycles   Enter asks   C concepts", 11, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.txt(QRectF(16, 694, 410, 14), guided ? "Esc backs out" : ", . page   Space scrolls   Esc backs out", 11, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    // ASK ROOK: always in the same place
    w.ui.button(kAskRect, "tour_ask", "ASK ROOK  (Enter)", true, [this] { ask(); });
}
