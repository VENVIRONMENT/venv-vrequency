// venv-vrequency — the tour's choosing of controls: select, move by arrows, reading order, Start-here steps, related links.
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
using namespace pal;

// ---------------------------------------------------------------------------------- selection

QString TourView::selectedName() const {
    if (!built || selIdx < 0 || selIdx >= (int)pics[picIndex()].hot.size()) return QString();
    return pics[picIndex()].hot[selIdx].name;
}

QString TourView::selectedRef() const {
    if (!built || selIdx < 0 || selIdx >= (int)pics[picIndex()].hot.size()) return QString();
    const HelpCard *c = cardFor(pics[picIndex()].hot[selIdx]);
    QString pg, key;
    const Hot &h = pics[picIndex()].hot[selIdx];
    if (!h.ref.isEmpty()) return h.ref;
    return pageId() + "." + (c ? c->key : h.name);
}

const HelpCard *TourView::cardFor(const Hot &h) const {
    const CardDb &db = cards();
    if (!db.ok) {
        missing = HelpCard();
        missing.key = "error"; missing.kind = "area"; missing.title = "Help unavailable";
        missing.summary = "The help text could not be read.";
        missing.does = db.error.isEmpty() ? QString("The help file is missing or damaged.") : db.error.left(200);
        missing.recall = "The pictures still work. Reinstall or rebuild the program to get the help text back.";
        missing.ask = "The tour says its help text is missing. What can I do?";
        missing.use.push_back({"Both", "Everything else in the app works as before."});
        return &missing;
    }
    const HelpCard *c = h.ref.isEmpty() ? db.find(pageId(), h.name) : db.resolve(h.ref);
    if (c) return c;
    missing = HelpCard();
    missing.key = h.name; missing.kind = "area"; missing.title = h.name;
    missing.summary = "No help card for this control yet.";
    missing.does = "This control is on the screen but has no card. Ask Rook what it does.";
    missing.recall = "Look for it on the " + QString::fromLatin1(pageInfo(pageIdx).label) + " page.";
    missing.ask = QString("What does %1 do on the %2 page?").arg(h.name, pageInfo(pageIdx).label);
    missing.use.push_back({"Both", "Ask Rook for details."});
    return &missing;
}

// a touch on a hotspot: select it; the editor's Prev / Next also turn the picture to the other parameters
void TourView::tapHot(int idx) {
    choose(idx);
    const QString n = selectedName();
    if (pageIdx == pageIndex("sound-deep") && (n == "deepnext" || n == "deepprev")) {
        setVariant(variant + (n == "deepnext" ? 1 : -1));
        const auto &hot = pics[picIndex()].hot;
        for (int i = 0; i < (int)hot.size(); ++i) if (hot[i].name == n) choose(i);
    }
}

void TourView::choose(int idx) {
    if (!built) return;
    selIdx = (idx >= 0 && idx < (int)pics[picIndex()].hot.size()) ? idx : -1;
    scrollA = scrollB = 0;
    w.update();
}

bool TourView::select(const QString &name) {
    if (!built || name.isEmpty()) return false;
    const CardDb &db = cards();
    const HelpCard *c = db.ok ? db.find(pageId(), name) : nullptr;
    for (int k = 0; k < variantCount(pageIdx); ++k) {
        const int v = (variant + k) % variantCount(pageIdx);
        const auto &hot = pics[variantPic(pageIdx, v)].hot;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < (int)hot.size(); ++i) {
                const bool hit = pass == 0 ? hot[i].name == name : (c && cardFor(hot[i]) == c);
                if (hit) { conceptOn = false; if (v != variant) setVariant(v); choose(i); return true; }
            }
    }
    return false;
}

void TourView::selectConcept(const QString &id) {
    conceptOn = true;
    conceptId = id;
    scrollA = scrollB = 0;
    const auto &cs = cards().concepts;
    for (int i = 0; i < (int)cs.size(); ++i) if (cs[i].key == id) conceptScroll = qMax(0, i - 6);
    w.update();
}

int TourView::nearestHot(QPointF p, qreal tol) const {
    // p in canvas (720 x 720) coordinates of the page picture
    const auto &hot = pics[picIndex()].hot;
    int best = -1; qreal bestArea = 1e18;
    for (int i = 0; i < (int)hot.size(); ++i) {
        const QRectF &r = hot[i].r;
        if (r.contains(p) && r.width() * r.height() < bestArea) { best = i; bestArea = r.width() * r.height(); }
    }
    if (best >= 0) return best;
    qreal bd = tol;
    for (int i = 0; i < (int)hot.size(); ++i) {
        const QRectF &r = hot[i].r;
        const qreal dx = qMax(qMax(r.left() - p.x(), 0.0), p.x() - r.right()), dy = qMax(qMax(r.top() - p.y(), 0.0), p.y() - r.bottom());
        const qreal d = std::hypot(dx, dy);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int TourView::variantPic(int page, int v) { return v <= 0 || variantBase(page) < 0 ? page : variantBase(page) + v - 1; }

void TourView::move(int dx, int dy) {
    if (!built) return;
    const auto &hot = pics[picIndex()].hot;
    if (hot.empty()) return;
    if (selIdx < 0) { cycle(1); return; }
    const QPointF cur = hot[selIdx].r.center();
    int best = -1; qreal bestScore = 1e18;
    for (int i = 0; i < (int)hot.size(); ++i) {
        if (i == selIdx) continue;
        const QPointF c = hot[i].r.center();
        const qreal ddx = c.x() - cur.x(), ddy = c.y() - cur.y();
        const qreal fwd = dx ? ddx * dx : ddy * dy;
        if (fwd < 1.5) continue;
        const qreal lat = dx ? std::fabs(ddy) : std::fabs(ddx);
        if (lat > fwd * 2.5 + 30) continue;                 // mostly sideways: not "in that direction"
        const qreal score = fwd + 2.0 * lat;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best >= 0) choose(best);
}

// reading order: top to bottom in bands, left to right inside a band
static std::vector<int> readingOrder(const std::vector<TourView::Hot> &hot) {
    std::vector<int> o(hot.size());
    for (size_t i = 0; i < o.size(); ++i) o[i] = (int)i;
    std::stable_sort(o.begin(), o.end(), [&](int a, int b) {
        const int ba = (int)std::floor(hot[a].r.center().y() / 28), bb = (int)std::floor(hot[b].r.center().y() / 28);
        if (ba != bb) return ba < bb;
        return hot[a].r.center().x() < hot[b].r.center().x();
    });
    return o;
}

void TourView::cycle(int d) {
    if (!built) return;
    if (conceptOn) {
        const auto &cs = cards().concepts;
        if (cs.empty()) return;
        int i = -1;
        for (int k = 0; k < (int)cs.size(); ++k) if (cs[k].key == conceptId) i = k;
        i = i < 0 ? (d > 0 ? 0 : (int)cs.size() - 1) : (i + d + (int)cs.size()) % (int)cs.size();
        selectConcept(cs[i].key);
        return;
    }
    const auto &hot = pics[picIndex()].hot;
    if (hot.empty()) return;
    const std::vector<int> o = readingOrder(hot);
    int pos = -1;
    for (int k = 0; k < (int)o.size(); ++k) if (o[k] == selIdx) pos = k;
    pos = pos < 0 ? (d > 0 ? 0 : (int)o.size() - 1) : (pos + d + (int)o.size()) % (int)o.size();
    choose(o[pos]);
}

void TourView::nextStep(int d) {
    if (!guided) return;
    step += d;
    if (step < 0) step = 0;
    if (step >= tourPathLen()) { // the end of the walk: carry on exploring freely
        guided = false;
        step = tourPathLen() - 1;
        noticeUntil = w.clock.elapsed() + 4000;   // said on the tour's own screen: the tour never writes to the real toast
        return;
    }
    setPage(pageIndex(tourPathAt(step)));
}

void TourView::goRef(const QString &ref) {
    const CardDb &db = cards();
    QString pg, key;
    const HelpCard *c = db.resolve(ref, &pg, &key);
    if (!c) return;
    if (pg == "concepts") { selectConcept(key); return; }
    const int idx = pageIndex(pg);
    if (idx < 0) return;
    if (idx != pageIdx) setPage(idx);
    conceptOn = false;
    for (int k = 0; k < variantCount(pageIdx); ++k) {
        const int v = (variant + k) % variantCount(pageIdx);
        const auto &hot = pics[variantPic(pageIdx, v)].hot;
        for (int i = 0; i < (int)hot.size(); ++i) if (cardFor(hot[i]) == c) { if (v != variant) setVariant(v); choose(i); return; }
    }
    choose(-1);
}
