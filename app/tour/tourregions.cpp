// venv-vrequency — the tour's card layout: measures and draws the three card regions, picks the font size that fits.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

// ---------------------------------------------------------------------------------- the card layout

// The card is two regions that each lay out from the top and scroll when they must:
//   which 0 = upper right: badge, title, summary, DOES, RECALL
//   which 1 = lower left: USE rows and key caps
//   which 2 = lower right: tips and related chips (ASK ROOK stays pinned below it)
// With paint false nothing is drawn: it only measures, so a layout check and the picture can never differ.
qreal TourView::regionN(int which, const HelpCard &c, int px, int scroll, bool paint, const QRectF &area, const QString &crumb) {
    const int sm = px - 1;
    const qreal x0 = area.x() + 10, wid = area.width() - 20;
    qreal y = 8;
    auto Y = [&](qreal cy) { return area.y() + cy - scroll; };
    auto cols = [&](int p, qreal wd) { return qMax(8, (int)(wd / w.ui.chW(p))); };
    auto visible = [&](qreal cy, qreal h) { return Y(cy) + h > area.top() && Y(cy) < area.bottom(); };
    const int L = Qt::AlignLeft | Qt::AlignVCenter;

    auto para = [&](const QString &s, int p, QColor col, bool bold, qreal x, qreal wd) -> qreal {
        if (s.trimmed().isEmpty()) return 0;
        const QStringList lines = chatWrap(s, cols(p, wd), -1);
        qreal h = 0;
        for (const QString &ln : lines) {
            if (paint && visible(y + h, p + 4)) w.ui.txt(QRectF(x, Y(y + h), wd, p + 4), ln, p, col, L, bold);
            h += p + 4;
        }
        return h;
    };
    auto label = [&](const QString &s) {
        if (paint && visible(y, 14)) {
            w.ui.txt(QRectF(x0, Y(y), 200, 14), s, 11, MID, L, true);
            const qreal lx = x0 + s.size() * w.ui.chW(11) + 8;
            w.ui.g->fillRect(QRectF(lx, Y(y) + 7, area.right() - 10 - lx, 1), GRAT);
        }
        y += 18;
    };
    // a row of chips: returns the height used; `zoneBase` non-empty makes them touchable
    auto chips = [&](const QStringList &items, bool tappable, bool cap, const QString &zoneBase, int firstId) -> qreal {
        qreal cx = 0, cy = 0;
        const int ch = 22;
        for (int i = 0; i < items.size(); ++i) {
            QString s = items[i];
            qreal cwid = s.size() * w.ui.chW(sm) + 16;
            if (cwid > wid) { s = w.ui.fit(s, sm, wid - 16); cwid = wid; }
            if (cx > 0 && cx + cwid > wid) { cx = 0; cy += ch + 5; }
            const QRectF r(x0 + cx, Y(y + cy), cwid, ch);
            if (paint && visible(y + cy, ch)) {
                if (cap) { // a key cap: a frame with a heavier foot
                    w.ui.frame(r, MID, BG);
                    w.ui.g->fillRect(QRectF(r.x() + 1, r.bottom() - 3, r.width() - 2, 2), LOW);
                    w.ui.txt(r.adjusted(0, -1, 0, -2), s, sm, INK, Qt::AlignCenter, true);
                } else {
                    const bool dn = w.ui.isDown(QString("%1%2").arg(zoneBase).arg(firstId + i));
                    w.ui.frame(r, dn ? INK : MID, dn ? INK : BG);
                    w.ui.txt(r, s, sm, dn ? BG : INK, Qt::AlignCenter, false);
                }
                if (tappable && r.bottom() > area.top() && r.top() < area.bottom())
                    w.ui.zone(r.intersected(area), QString("%1%2").arg(zoneBase).arg(firstId + i), [this, idx = firstId + i](QPointF, int ph) {
                        if (ph == PH_PRESS && idx < related.size()) goRef(related[idx]);
                    });
            }
            cx += cwid + 6;
        }
        return cy + (items.isEmpty() ? 0 : ch);
    };

    if (which == 0) {
        // badge row
        if (paint && visible(y, 16)) {
            const QString kind = (c.kind.isEmpty() ? QString("area") : c.kind).toUpper();
            const qreal bw = kind.size() * w.ui.chW(11) + 12;
            w.ui.tag(QRectF(x0, Y(y), bw, 16), kind, 11);
            w.ui.txt(QRectF(x0 + bw + 8, Y(y), wid - bw - 8, 16), w.ui.fit(crumb.toUpper(), 11, wid - bw - 8), 11, MID, Qt::AlignRight | Qt::AlignVCenter);
        }
        y += 24;
        // title: as big as fits on one line
        int tp = 22;
        while (tp > 14 && c.title.size() * w.ui.chW(tp) > wid) --tp;
        if (paint && visible(y, tp + 6)) w.ui.txt(QRectF(x0, Y(y), wid, tp + 6), w.ui.fit(c.title, tp, wid), tp, INK, L, true);
        y += tp + 10;
        qreal h = para(c.summary, px, INK, true, x0, wid);
        y += h + 8;
        if (paint && visible(y, 1)) w.ui.g->fillRect(QRectF(x0, Y(y), wid, 1), LOW);
        y += 9;
        label("DOES");
        h = para(c.does, px, INK, false, x0, wid);
        y += h + 10;
        // RECALL: its own box, because it is the part to remember
        if (!c.recall.trimmed().isEmpty()) {
            const QStringList lines = chatWrap(c.recall, cols(sm, wid - 20), -1);
            const qreal bh = 16 + lines.size() * (sm + 4) + 8;
            if (paint && visible(y, bh + 8)) {
                const QRectF box(x0, Y(y) + 7, wid, bh);
                w.ui.frame(box, INK, BG);
                w.ui.frame(box.adjusted(3, 3, -3, -3), GRAT);
                w.ui.tag(QRectF(box.x() + 8, box.y() - 8, 56, 15), "RECALL", 11);
                for (int i = 0; i < lines.size(); ++i)
                    w.ui.txt(QRectF(box.x() + 10, box.y() + 12 + i * (sm + 4), wid - 20, sm + 4), lines[i], sm, INK, L, false);
            }
            y += 7 + bh + 8;
        }
        return y;
    }
    if (which == 1) {
        label("USE");
        for (const HelpCard::Use &u : c.use) {
            const QString how = u.how.toUpper();
            const qreal tw = 50;
            const QStringList lines = chatWrap(u.text, cols(sm, wid - tw - 10), -1);
            const qreal rh = qMax<qreal>(18, lines.size() * (sm + 4));
            if (paint && visible(y, rh)) {
                const QRectF tg(x0, Y(y), tw, 17);
                if (how == "TOUCH") { w.ui.frame(tg, MID, BG); w.ui.txt(tg, how, 10, INK, Qt::AlignCenter, true); }
                else if (how == "KEYS") { w.ui.g->fillRect(tg, INK); w.ui.txt(tg, how, 10, BG, Qt::AlignCenter, true); }
                else { w.ui.g->fillRect(tg, INK); w.ui.frame(tg.adjusted(2, 2, -2, -2), BG); w.ui.txt(tg, how.isEmpty() ? QString("BOTH") : how, 10, BG, Qt::AlignCenter, true); }
                for (int i = 0; i < lines.size(); ++i)
                    w.ui.txt(QRectF(x0 + tw + 10, Y(y) + i * (sm + 4) - 1, wid - tw - 10, sm + 4), lines[i], sm, INK, L, false);
            }
            y += rh + 6;
        }
        if (!c.keys.isEmpty()) {
            y += 4;
            label("KEYS");
            y += chips(c.keys, false, true, QString(), 0) + 6;
        }
        return y + 4;
    }
    // which == 2
    if (!c.tips.isEmpty()) {
        label("TIPS");
        for (const QString &t : c.tips) {
            const qreal h = para(t, sm, MID, false, x0 + 14, wid - 14);
            if (paint && visible(y, 14)) w.ui.txt(QRectF(x0, Y(y), 14, sm + 4), "+", sm, INK, L, true);
            y += h + 6;
        }
        y += 4;
    }
    QStringList titles;
    related.clear();
    for (const QString &r : c.related) {
        const HelpCard *rc = cards().resolve(r);
        if (rc) { titles << QString::fromUtf8("› ") + rc->title; related << r; }
    }
    if (!titles.isEmpty()) {
        label("RELATED");
        y += chips(titles, true, false, "tour_rel", 0) + 6;
    }
    return y + 4;
}

TourView::Fit TourView::fitFor(const HelpCard &c, bool) {
    Fit f;
    for (int px : {13, 12, 11}) {
        f.px = px;
        f.heightA = (int)regionN(0, c, px, 0, false, kRegionA, QString());
        f.heightB = (int)qMax(regionN(1, c, px, 0, false, kRegionB1, QString()), regionN(2, c, px, 0, false, kRegionB2, QString()));
        const int hb1 = (int)regionN(1, c, px, 0, false, kRegionB1, QString()), hb2 = (int)regionN(2, c, px, 0, false, kRegionB2, QString());
        f.scrollA = f.heightA > kRegionA.height();
        f.scrollB = hb1 > kRegionB1.height() || hb2 > kRegionB2.height();
        if (!f.scrollA && !f.scrollB) break;
    }
    return f;
}

TourView::Fit TourView::measure(const HelpCard &c) { return fitFor(c, false); }

HelpCard TourView::introCard(int page) const {
    HelpCard c;
    const CardPage *cp = cards().page(QString::fromLatin1(pageInfo(page).id));
    c.key = "page";
    c.kind = "page";
    c.title = cp && !cp->title.isEmpty() ? cp->title : QString::fromLatin1(pageInfo(page).label);
    c.summary = "Tap a control in the picture to read what it does.";
    c.does = cp && !cp->intro.isEmpty() ? cp->intro : QString("This is the %1 page.").arg(pageInfo(page).label);
    c.recall = cp && !cp->recall.isEmpty() ? cp->recall : QString("Tour: Help page, Tour button.");
    c.use.push_back({"Touch", "Tap a control in the picture. Tap empty space to clear."});
    c.use.push_back({"Keys", "Arrows move between controls. Tab goes in reading order. Enter asks Rook."});
    c.use.push_back({"Keys", "Comma and period change page. C opens the concepts. Esc leaves."});
    c.keys = {"Arrows", "Tab", "Enter", ", .", "C", "Esc"};
    c.related = tourReadAlso(QString::fromLatin1(pageInfo(page).id));
    c.ask = QString("Tell me about the %1 page.").arg(pageInfo(page).label);
    return c;
}

TourView::Fit TourView::measureIntro(int page) { return fitFor(introCard(page), true); }
