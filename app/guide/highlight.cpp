// venv-vrequency — guided help: the highlight overlay and what is being discussed (see guide.h).
#include "app/guide/guide.h"
#include "app/chat/chat.h"
#include "app/guide/zone_names.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>
#include <functional>

using namespace pal;

void Guide::noteTap(const QString &zone) {
    if (!hiddenZone(zone) && !zone.startsWith("tab_")) { lastTapped = zone; lastTappedPage = sh.page(); }
}

void Guide::beginFrame() {
    const int now = sh.page();
    if (hl.page != now) { hl.zones.clear(); hl.until = 0; }
    if (topicPage != now) { topicLabel.clear(); topicZones.clear(); }
    if (lastTappedPage != now) lastTapped.clear();
    if (now == PG_CHAT) dockFocus = false;
}

void Guide::agentSpoke() {
    if (sh.clock.elapsed() - hl.issued > 5000) { topicLabel.clear(); topicZones.clear(); }
}

QStringList Guide::highlightedZones() {
    if (hl.zones.isEmpty()) return {};
    if (hl.page != sh.page() || sh.clock.elapsed() >= hl.until) { hl.zones.clear(); hl.label.clear(); return {}; }
    return hl.zones;
}

void Guide::clearHighlight() {
    hl.zones.clear(); hl.label.clear(); hl.until = 0; hl.page = -1;
    topicLabel.clear(); topicZones.clear();
    hlCaption = QRectF();
    sh.redraw();
}

QStringList Guide::expandZoneNames(const QStringList &names, QStringList &unknown) const {
    QStringList have;
    for (const Ui::Zone &z : ui.zones()) if (!hiddenZone(z.name)) have << z.name;
    QStringList out;
    auto add = [&](const QString &n) { if (!out.contains(n)) out << n; };
    std::function<bool(const QString &, int)> resolve = [&](const QString &n, int depth) -> bool {
        if (have.contains(n)) { add(n); return true; }
        bool any = false;
        if (n.endsWith('*') && n.size() > 1) {
            for (const QString &h : have) if (h.startsWith(n.left(n.size() - 1))) { add(h); any = true; }
            return any;
        }
        if (depth < 2)
            for (const QString &a : aliasZones(n)) any = resolve(a, depth + 1) || any;
        return any;
    };
    for (const QString &raw : names) {
        const QString n = raw.trimmed();
        if (n.isEmpty()) { unknown << raw.left(48); continue; }
        if (!resolve(n, 0)) unknown << n.left(48);
    }
    return out;
}

QStringList Guide::setHighlight(const QStringList &names, const QString &label, int ms, QStringList &unknown) {
    const QStringList known = expandZoneNames(names, unknown);
    if (known.isEmpty()) return known;
    hl.zones = known;
    hl.label = label;
    hl.page = sh.page();
    hl.issued = sh.clock.elapsed();
    hl.until = hl.issued + qBound(100, ms, 30000);
    // what the dock says is being discussed: the label, else the name that was asked for
    QString what = label;
    if (what.isEmpty()) {
        const QString first = names.value(0).trimmed();
        what = (first.endsWith('*') || !aliasZones(first).isEmpty() || known.size() > 1) && !known.contains(first)
                   ? QString(first).remove('*').replace('_', ' ') : zoneLabel(known[0]);
    }
    topicLabel = what.left(28);
    topicZones = known;
    topicPage = sh.page();
    sh.redraw();
    return known;
}

// What a message sent now carries as `focus`: the highlighted zones, else the ones the last agent
// message was about, else the control the player touched last on this page.
QStringList Guide::focus() {
    QStringList f = highlightedZones();
    if (f.isEmpty() && topicPage == sh.page()) f = topicZones;
    if (f.isEmpty() && !lastTapped.isEmpty() && lastTappedPage == sh.page()) f << lastTapped;
    return f;
}

void Guide::paintHighlights() {
    hlCaption = QRectF();
    const QStringList names = highlightedZones();
    if (names.isEmpty()) return;
    const qreal t = sh.clock.elapsed() / 1000.0;
    const qreal ph = 0.5 + 0.5 * std::sin(t * 2 * M_PI / 1.6);  // a gentle pulse
    QColor soft = INK;
    soft.setAlpha((int)(70 + 185 * ph));
    QVector<QRectF> rects;
    for (const QString &n : names) {
        const QRectF r = ui.zoneRect(n);
        if (r.isNull()) continue;
        rects << r;
        ui.frame(inflated(r, 6), BG);        // a dark gap keeps the rules readable over bright controls
        ui.frame(inflated(r, 5), soft);      // outer rule: pulses
        ui.frame(inflated(r, 4), BG);
        ui.frame(inflated(r, 3), INK);       // inner rule: steady (2 px of padding inside it)
    }
    if (rects.isEmpty() || hl.label.isEmpty()) return;

    // the caption: a small tag by the first zone that covers neither a highlight nor the dock
    const QString text = ui.fit(hl.label, 13, 40 * ui.chW(13));
    const qreal w = text.size() * ui.chW(13) + 12, h = 18;
    QVector<QRectF> avoid;
    for (const QRectF &r : rects) avoid << inflated(r, 8);
    if (!dockR.isNull()) avoid << inflated(dockR, 4);
    if (sh.page() != PG_CHAT && !dockOpen && chat.tickerShowing()) avoid << QRectF(12, 2, 696, 52) << QRectF(12, 664, 696, 52);
    const QRectF z = rects[0], screen(2, 2, 716, 716);
    auto ok = [&](const QRectF &c) {
        if (!screen.contains(c)) return false;
        for (const QRectF &a : avoid) if (a.intersects(c)) return false;
        return true;
    };
    const qreal x0 = qBound(2.0, z.x(), 718 - w), x1 = qBound(2.0, z.right() - w, 718 - w);
    QVector<QRectF> cand = {
        QRectF(x0, z.top() - 9 - h, w, h), QRectF(x0, z.bottom() + 9, w, h),
        QRectF(x1, z.top() - 9 - h, w, h), QRectF(x1, z.bottom() + 9, w, h),
        QRectF(z.right() + 9, z.center().y() - h / 2, w, h), QRectF(z.left() - 9 - w, z.center().y() - h / 2, w, h)};
    QRectF pick;
    for (const QRectF &c : cand) if (ok(c)) { pick = c; break; }
    if (pick.isNull()) { // anywhere free, nearest to the zone
        qreal best = 1e9;
        for (qreal y = 2; y + h <= 718; y += 6)
            for (qreal x = 2; x + w <= 718; x += 12) {
                const QRectF c(x, y, w, h);
                if (!ok(c)) continue;
                const qreal d = std::hypot(c.center().x() - z.center().x(), c.center().y() - z.center().y());
                if (d < best) { best = d; pick = c; }
            }
    }
    if (pick.isNull()) return;
    hlCaption = pick;
    ui.frame(inflated(pick, 2), INK, BG);
    ui.tag(pick, text, 13);
}
