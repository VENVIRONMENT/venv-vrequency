// venv-vrequency — drawing the tour screen: header, chips, guide bar, and the picture with its hotspots.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

// ---------------------------------------------------------------------------------- painting

void TourView::paint() {
    if (!built) { // the pictures are being drawn: a moment, with a way out
        w.ui.tag(QRectF(8, 4, 64, 26), "TOUR", 14);
        w.ui.txt(QRectF(0, 300, 720, 40), "Drawing the tour pictures...", 20, INK, Qt::AlignCenter, true);
        w.ui.txt(QRectF(0, 340, 720, 24), "Nothing on them is live. Esc leaves.", 13, MID);
        w.ui.button(QRectF(554, 4, 158, 26), "tour_close", "Leave tour", false, [this] { close(); });
        return;
    }
    paintHeader();
    if (guided) paintGuideBar(); else paintChips();
    if (conceptOn) paintConceptList(); else paintPicture();
    paintCardRegions();
}

void TourView::paintHeader() {
    w.ui.tag(QRectF(8, 4, 64, 26), "TOUR", 14);
    QString title = conceptOn ? QString("Concepts") : QString::fromLatin1(pageInfo(pageIdx).label);
    const bool multi = !conceptOn && variantCount(pageIdx) > 1;
    w.ui.txt(QRectF(82, 4, multi ? 214 : 290, 26), w.ui.fit(title.toUpper(), 16, multi ? 214 : 290), 16, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    if (multi) { // this page has more than one picture
        w.ui.button(QRectF(298, 4, 26, 26), "tour_var_prev", QString::fromUtf8("‹"), false, [this] { setVariant(variant - 1); });
        w.ui.txt(QRectF(326, 4, 36, 26), QString("%1/%2").arg(variant + 1).arg(variantCount(pageIdx)), 12, MID, Qt::AlignCenter);
        w.ui.button(QRectF(364, 4, 26, 26), "tour_var_next", QString::fromUtf8("›"), false, [this] { setVariant(variant + 1); });
    }
    w.ui.button(QRectF(396, 4, 152, 26), "tour_concepts", conceptOn ? "Pictures" : "Concepts", conceptOn, [this] {
        conceptOn = !conceptOn; conceptId.clear(); scrollA = scrollB = 0;
    });
    w.ui.button(QRectF(554, 4, 158, 26), "tour_close", "Leave tour", false, [this] { close(); });
}

void TourView::paintChips() {
    const qreal cw = 704.0 / pageCount();
    for (int i = 0; i < pageCount(); ++i) {
        const QRectF r(std::floor(8 + i * cw), 33, std::floor(8 + (i + 1) * cw) - std::floor(8 + i * cw) + 1, 24);
        w.ui.button(r, QString("tour_page_%1").arg(i), pageInfo(i).chip, i == pageIdx && !conceptOn, [this, i] { setPage(i); });
    }
}

void TourView::paintGuideBar() {
    const QString head = QString("START HERE  %1 of %2").arg(step + 1).arg(tourPathLen());
    w.ui.tag(QRectF(8, 33, 190, 24), head, 13, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.txt(QRectF(206, 33, 190, 24), w.ui.fit(QString::fromLatin1(pageInfo(pageIdx).label), 13, 186), 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.button(QRectF(392, 33, 92, 24), "tour_back", QString::fromUtf8("‹ Back"), false, [this] { nextStep(-1); });
    w.ui.button(QRectF(490, 33, 92, 24), "tour_next", QString::fromUtf8("Next ›"), true, [this] { nextStep(1); });
    w.ui.button(QRectF(588, 33, 124, 24), "tour_free", "Explore freely", false, [this] { guided = false; });
}

void TourView::paintPicture() {
    const Pic &pic = pics[picIndex()];
    const QRectF pr = pictureRect();
    const qreal S = kScale;
    auto scr = [&](const QRectF &r) { return QRectF(pr.x() + r.x() * S, pr.y() + r.y() * S, r.width() * S, r.height() * S); };
    // touching empty space clears the selection; touching near a control selects it (fingers are bigger than 20 pixels)
    w.ui.zone(pr, "tour_picture", [this, pr](QPointF p, int ph) {
        if (ph != PH_PRESS) return;
        const int i = nearestHot(QPointF((p.x() - pr.x()) / kScale, (p.y() - pr.y()) / kScale), 16 / kScale);
        if (i >= 0) tapHot(i); else choose(-1);
    });
    w.ui.g->drawImage(pr.topLeft(), pic.small);
    const bool sel = selIdx >= 0 && selIdx < (int)pic.hot.size();
    if (sel) { // the rest of the screen steps back
        w.ui.g->fillRect(pr, QColor(4, 4, 4, 135));
        const QRectF s = scr(pic.hot[selIdx].r).intersected(pr);
        w.ui.g->drawImage(s, pic.small, QRectF(s.x() - pr.x(), s.y() - pr.y(), s.width(), s.height()));
    }
    // every control, as a faint outline; the real ones are touchable in the order of their size so the smallest on top wins
    std::vector<int> order(pic.hot.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pic.hot[a].r.width() * pic.hot[a].r.height() > pic.hot[b].r.width() * pic.hot[b].r.height(); });
    for (int i : order) {
        const QRectF r = scr(pic.hot[i].r).intersected(pr);
        if (i != selIdx) {
            const QColor c(243, 243, 239, pic.hot[i].real ? 58 : 100);
            if (pic.hot[i].real) w.ui.frame(r, c);
            else { // controls that are not zones are drawn with corner ticks only
                const qreal t = 4;
                w.ui.g->fillRect(QRectF(r.left(), r.top(), t, 1), c); w.ui.g->fillRect(QRectF(r.left(), r.top(), 1, t), c);
                w.ui.g->fillRect(QRectF(r.right() - t, r.top(), t, 1), c); w.ui.g->fillRect(QRectF(r.right() - 1, r.top(), 1, t), c);
                w.ui.g->fillRect(QRectF(r.left(), r.bottom() - 1, t, 1), c); w.ui.g->fillRect(QRectF(r.left(), r.bottom() - t, 1, t), c);
                w.ui.g->fillRect(QRectF(r.right() - t, r.bottom() - 1, t, 1), c); w.ui.g->fillRect(QRectF(r.right() - 1, r.bottom() - t, 1, t), c);
            }
        }
        const QString zn = "tour_hot_" + pic.hot[i].name;
        w.ui.zone(grow(r, r.width() < 14 || r.height() < 14 ? 4 : 0), zn, [this, i](QPointF, int ph) { if (ph == PH_PRESS) tapHot(i); });
    }
    if (sel) { // the pulsing double rule, as the highlights use
        const QRectF r = scr(pic.hot[selIdx].r).intersected(pr);
        const qreal ph = 0.5 + 0.5 * std::sin(w.clock.elapsed() / 1000.0 * 2 * M_PI / 1.6);
        QColor soft = INK; soft.setAlpha((int)(70 + 185 * ph));
        w.ui.frame(grow(r, 5), soft);
        w.ui.frame(grow(r, 4), BG);
        w.ui.frame(grow(r, 3), INK);
        w.ui.frame(grow(r, 2), BG);
    }
    w.ui.frame(grow(pr, 1), LOW);
    if (w.clock.elapsed() < noticeUntil) { // the walk is over
        const QString t = "That was the tour. Tap or arrow to explore any control.";
        const QRectF r(pr.x() + 8, pr.bottom() - 30, pr.width() - 16, 22);
        w.ui.frame(grow(r, 2), INK, BG);
        w.ui.tag(r, w.ui.fit(t, 12, r.width() - 8), 12);
    }
}
