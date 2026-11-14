// venv-vrequency — the ticker (see chat.h).
#include "app/chat/chat.h"
#include "app/guide/guide.h"
#include "app/ui/palette.h"

using namespace pal;

namespace {
bool inflatedHit(const QRectF &zone, const QRectF &r) { return !zone.isNull() && zone.adjusted(-8, -8, 8, 8).intersects(r.adjusted(-3, -3, 3, 3)); }
} // namespace

// A new agent message on any other page: two lines at the bottom, for a few seconds. It draws
// over the page and takes no key, so playing carries on; touching it opens the chat dock to answer.
void Chat::paintTicker() {
    if (sh.page() == PG_CHAT || guide->dockOpen || sh.clock.elapsed() >= tickerUntil) return;
    QRectF r(16, 668, 688, 44);
    for (const QString &n : guide->highlightedZones()) // never over what is highlighted: then it goes to the top
        if (inflatedHit(ui.zoneRect(n), r)) { r = QRectF(16, 6, 688, 44); break; }
    ui.frame(r.adjusted(-2, -2, 2, 2), INK, BG);
    const int cw = ui.chW(13);
    QString name = ticker.from.toUpper();
    if (ticker.kind == "ask") name += "?";
    const qreal tw = (name.size() + 2) * cw;
    ui.tag(QRectF(r.x() + 5, r.y() + 4, tw, 17), name, 13);
    const QString flat = QString(ticker.text).replace('\n', ' ');
    const int c1 = (int)((r.width() - tw - 20) / cw), c2 = (int)((r.width() - 12 - 150) / cw);
    QString l1 = flat.left(c1), rest = flat.mid(c1);
    if (flat.size() > c1) { // break at a space when one is near
        const int sp = l1.lastIndexOf(' ');
        if (sp > c1 / 2) { l1 = flat.left(sp); rest = flat.mid(sp + 1); }
    }
    ui.txt(QRectF(r.x() + 12 + tw, r.y() + 4, r.width() - tw - 16, 17), l1, 13, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    ui.txt(QRectF(r.x() + 6, r.y() + 23, r.width() - 12 - 150, 17), ui.fit(rest, 13, c2 * cw), 13, INK, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(r.right() - 150, r.y() + 23, 144, 17), QString("%1+%2 to reply").arg(sh.km.keyFor(A_COMMAND, false)).arg(sh.km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
    ui.zone(r.adjusted(-2, -2, 2, 2), "ticker", [this](QPointF, int ph) { if (ph == PH_PRESS) { tickerUntil = 0; guide->dockOpenFocus(); } }); // touch the ticker to answer
}
