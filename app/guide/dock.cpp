// venv-vrequency — guided help: the chat dock (see guide.h).
#include "app/guide/guide.h"
#include "app/chat/chat.h"
#include "app/guide/zone_names.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>
#include <vector>

using namespace pal;

void Guide::askFromTour(const QString &tourPage, const QString &zone) {
    askFocus = zone.isEmpty() ? QStringList() : QStringList{zone};
    askPage = pageName(sh.page());
    askVia = "tour";
    if (sh.page() == PG_CHAT) { dockFocus = false; }
    else {
        topicLabel = QString(zone.isEmpty() ? tourPage : zone).replace('_', ' ').left(28);
        topicZones = askFocus; topicPage = sh.page();
        dockOpenFocus();
    }
}

void Guide::dockOpenFocus() {
    dockOpen = true;
    dockFocus = true;
    chat.unread = 0;
    chat.hideTicker();
    sh.redraw();
}

void Guide::dockClose() {
    askFocus.clear(); askPage.clear(); askVia.clear();
    dockOpen = false;
    dockFocus = false;
    dockR = QRectF();
    sh.redraw();
}

// Where the dock goes: the bottom of the screen, as tall as is comfortable, unless that would
// cover something highlighted. Then it gets shorter, then it moves to the top, then shorter there.
void Guide::layoutDock() {
    dockR = QRectF();
    dockLines = 0;
    if (!dockOpen || sh.page() == PG_CHAT || sh.keys.testOpen) return;
    QVector<QRectF> hot;
    for (const QString &n : highlightedZones()) { const QRectF r = ui.zoneRect(n); if (!r.isNull()) hot << inflated(r, 8); }
    struct Opt { bool top; int lines; };
    static const Opt order[] = {{false, 4}, {false, 2}, {true, 4}, {true, 2}, {false, 1}, {true, 1}, {false, 0}, {true, 0}};
    QRectF fallback;
    for (const Opt &o : order) {
        const qreal h = 74 + 16 * o.lines;
        const QRectF r(6, o.top ? 4 : 720 - 4 - h, 708, h);
        if (fallback.isNull()) fallback = r;
        bool clash = false;
        for (const QRectF &x : hot) clash = clash || x.intersects(r.adjusted(-3, -3, 3, 3));
        if (!clash) { dockR = r; dockLines = o.lines; return; }
    }
    dockR = QRectF(6, 720 - 4 - 74, 708, 74); // nothing clear: the smallest, at the bottom
    dockLines = 0;
}

void Guide::paintDock() {
    if (dockR.isNull()) return;
    chat.unread = 0;
    const QRectF D = dockR;
    ui.frame(D.adjusted(-2, -2, 2, 2), INK, BG);
    ui.frame(D, dockFocus ? INK : LOW);
    ui.zone(D, "dock", [](QPointF, int) {}); // swallows touches so nothing under the dock is pressed by accident

    // header: what is being discussed, who is listening, and the two ways out
    const bool listening = chat.listening();
    qreal hx = D.x() + 6;
    if (!topicLabel.isEmpty()) {
        const QString t = QString::fromUtf8("▸ ") + ui.fit(topicLabel.toUpper(), 13, 260);
        const qreal tw = t.size() * ui.chW(13) + 14;
        ui.tag(QRectF(hx, D.y() + 4, tw, 24), t, 13, Qt::AlignLeft | Qt::AlignVCenter);
        hx += tw + 8;
    }
    ui.txt(QRectF(hx, D.y() + 4, D.right() - 150 - hx, 24), listening ? "Rook is listening" : "Rook is not connected", 12, listening ? MID : LOW, Qt::AlignLeft | Qt::AlignVCenter);
    ui.button(QRectF(D.right() - 140, D.y() + 4, 64, 24), "dock_full", "Full", false, [this] { chat.toggle(); });
    ui.button(QRectF(D.right() - 70, D.y() + 4, 64, 24), "dock_close", "Hide", false, [this] { dockClose(); });

    // the last few lines
    const int cw = ui.chW(13), lh = 16;
    const QRectF hist(D.x() + 6, D.y() + 32, D.width() - 12, dockLines * lh + 2);
    if (dockLines > 0) {
        struct Ln { int msg; QString text; bool head; };
        const int cols = (int)((hist.width() - 10) / cw);
        std::vector<Ln> lines;
        const size_t n0 = chat.log.size() > 8 ? chat.log.size() - 8 : 0;
        for (size_t i = n0; i < chat.log.size(); ++i) {
            const int nameW = chat.log[i].from.size() + 3;
            const QStringList rows = chatWrap(chat.log[i].text, cols, cols - nameW);
            for (int k = 0; k < rows.size(); ++k) lines.push_back({(int)i, rows[k], k == 0});
        }
        const int total = (int)lines.size();
        chat.scroll = qBound(0, chat.scroll, qMax(0, total - dockLines));
        const int end = total - chat.scroll, start = qMax(0, end - dockLines);
        if (chat.log.empty()) ui.txt(QRectF(hist.x() + 4, hist.y(), hist.width(), lh), "No messages yet. Type below and press Enter.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
        for (int i = start; i < end; ++i) {
            const Ln &ln = lines[i];
            const ChatMsg &m = chat.log[ln.msg];
            const qreal y = hist.y() + (i - start) * lh;
            qreal x = hist.x() + 4;
            if (ln.head) {
                QString name = m.from.toUpper();
                if (m.kind == "ask") name += "?";
                const qreal w = (name.size() + 1) * cw + 4;
                if (m.user) { ui.frame(QRectF(x, y, w, lh - 1), MID); ui.txt(QRectF(x, y, w, lh - 1), name, 12, INK, Qt::AlignCenter, true); }
                else ui.tag(QRectF(x, y, w, lh - 1), name, 12);
                x += w + cw;
            }
            ui.txt(QRectF(x, y, hist.right() - x, lh), ln.text, 13, m.user ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (total > dockLines && chat.scroll < total - dockLines) ui.txt(QRectF(hist.right() - 14, hist.y(), 12, 12), QString::fromUtf8("▴"), 11, MID);
        ui.zone(hist, "dock_history", [this](QPointF p, int ph) { // drag to scroll
            if (ph == PH_PRESS) { dockDragY = p.y(); return; }
            const int d = (int)std::round((p.y() - dockDragY) / 16.0);
            if (d) { chat.scroll += d; dockDragY = p.y(); }
        });
    }

    // the input line and Send
    const QRectF box(D.x() + 6, D.bottom() - 42, D.width() - 12 - 102, 36);
    ui.frame(box, dockFocus ? INK : LOW, BG);
    if (dockFocus) ui.frame(box.adjusted(2, 2, -2, -2), GRAT);
    const int bc = (int)((box.width() - 20) / ui.chW(15));
    QString shown = chat.draft.size() > bc - 1 ? QString::fromUtf8("…") + chat.draft.right(bc - 2) : chat.draft;
    if (chat.draft.isEmpty()) {
        const QString hint = dockFocus ? QString("type to ask  (Esc: back to the keys)") : QString("%1+%2 or touch to type").arg(sh.km.keyFor(A_COMMAND, false)).arg(sh.km.keyFor(A_CHAT, true));
        ui.txt(box.adjusted(10 + (dockFocus ? ui.chW(15) : 0), 0, -10, 0), ui.fit(hint, 14, box.width() - 20), 14, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    } else ui.txt(box.adjusted(10, 0, -10, 0), shown, 15, dockFocus ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter, dockFocus);
    if (dockFocus && (sh.clock.elapsed() / 500) % 2 == 0) {
        const qreal cx = box.x() + 10 + (chat.draft.isEmpty() ? 0 : shown.size() * ui.chW(15));
        ui.g->fillRect(QRectF(cx, box.y() + 8, ui.chW(15) - 2, 20), INK);
    }
    ui.zone(box, "dock_input", [this](QPointF, int ph) { if (ph == PH_PRESS) dockFocus = true; });
    ui.button(QRectF(D.right() - 102, D.bottom() - 42, 96, 36), "dock_send", "Send", false, [this] { chat.send(); });
}
