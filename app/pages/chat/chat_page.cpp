// venv-vrequency — the Chat page (see chat_page.h).
#include "app/pages/chat/chat_page.h"
#include "app/chat/chat.h"
#include "app/ui/palette.h"
#include <QDateTime>
#include <QPainter>
#include <cmath>
#include <vector>

using namespace pal;

namespace {
struct Line { int msg; QString text; bool head; };
} // namespace

bool ChatPage::key(int qtKey, int plainAct) { return chat.key(qtKey, plainAct); }

void ChatPage::paint() {
    chat.unread = 0;
    const bool listening = chat.listening();
    ui.tag(QRectF(16, 64, 150, 22), "Copilot chat", 14, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(176, 64, 528, 22), listening ? "copilot: listening" : "copilot: not connected", 13, listening ? INK : MID,
        Qt::AlignRight | Qt::AlignVCenter, listening);

    const QRectF hist(16, 92, 688, 500);
    ui.frame(hist, LOW, BG);
    const int cw = ui.chW(13), lh = 17;
    const int cols = (int)((hist.width() - 28) / cw);
    const int rows = (int)((hist.height() - 8) / lh);
    std::vector<Line> lines;
    for (size_t i = 0; i < chat.log.size(); ++i) {
        lines.push_back({(int)i, QString(), true});
        for (const QString &l : chatWrap(chat.log[i].text, cols - 2)) lines.push_back({(int)i, l, false});
    }
    const int total = (int)lines.size();
    chat.scroll = qBound(0, chat.scroll, qMax(0, total - rows));
    const int end = total - chat.scroll, start = qMax(0, end - rows);
    if (chat.log.empty()) {
        ui.txt(QRectF(hist.x() + 12, hist.y() + 8, hist.width() - 24, 18), "No messages yet. Type below and press Enter.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
        ui.txt(QRectF(hist.x() + 12, hist.y() + 26, hist.width() - 24, 18), "A copilot connected to the control socket answers here.", 13, GRAT, Qt::AlignLeft | Qt::AlignVCenter);
    }
    for (int i = start; i < end; ++i) {
        const Line &ln = lines[i];
        const ChatMsg &m = chat.log[ln.msg];
        const qreal y = hist.y() + 5 + (i - start) * lh;
        if (ln.head) {
            QString name = m.from.toUpper();
            if (m.kind == "ask") name += "?";
            const qreal w = (name.size() + 2) * cw;
            if (m.user) { ui.frame(QRectF(hist.x() + 10, y, w, lh - 2), MID); ui.txt(QRectF(hist.x() + 10, y, w, lh - 2), name, 13, INK, Qt::AlignCenter, true); }
            else ui.tag(QRectF(hist.x() + 10, y, w, lh - 2), name, 13);
            QString extra = m.kind == "tip" ? "tip  " : "";
            extra += QDateTime::fromMSecsSinceEpoch(m.ts).toString("HH:mm");
            ui.txt(QRectF(hist.x() + 16 + w, y, 200, lh - 2), extra, 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
        } else {
            if (!m.user) ui.g->fillRect(QRectF(hist.x() + 10, y, 2, lh), INK); // agent text carries a rule on the left
            ui.txt(QRectF(hist.x() + 10 + 2 * cw, y, hist.width() - 24, lh), ln.text, 13, m.user ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (total > rows) { // scroll marker
        const qreal th = hist.height() - 6, bh = qMax(12.0, th * rows / total);
        const qreal by = hist.y() + 3 + (th - bh) * (total - rows - chat.scroll) / (total - rows);
        ui.g->fillRect(QRectF(hist.right() - 5, hist.y() + 3, 1, th), GRAT);
        ui.g->fillRect(QRectF(hist.right() - 6, by, 3, bh), MID);
    }
    ui.zone(hist, "chat_history", [this](QPointF p, int ph) { // drag the history to scroll it
        if (ph == PH_PRESS) { dragLastY = p.y(); return; }
        const int d = (int)std::round((p.y() - dragLastY) / 17.0);
        if (d) { chat.scroll += d; dragLastY = p.y(); }
    });

    const QRectF box(16, 600, 580, 44);
    ui.frame(box, INK, BG);
    const int bc = (int)((box.width() - 20) / ui.chW(15));
    QString shown = chat.draft.size() > bc - 1 ? QString::fromUtf8("…") + chat.draft.right(bc - 2) : chat.draft;
    if (chat.draft.isEmpty()) ui.txt(box.adjusted(10, 0, -10, 0), "type a message", 15, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    else ui.txt(box.adjusted(10, 0, -10, 0), shown, 15, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    if ((sh.clock.elapsed() / 500) % 2 == 0) {
        const qreal cx = box.x() + 10 + (chat.draft.isEmpty() ? 0 : shown.size() * ui.chW(15));
        ui.g->fillRect(QRectF(cx, box.y() + 10, ui.chW(15) - 2, 24), INK);
    }
    ui.zone(box, "chat_input", [](QPointF, int) {});
    ui.button(QRectF(604, 600, 100, 44), "chat_send", "Send", false, [this] { chat.send(); });
    ui.txt(QRectF(18, 652, 686, 18), "Enter sends  Backspace edits  Up/Down scrolls  Esc leaves", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 670, 686, 18), QString("%1+Shift+%2 jumps here from any page and back. Letters type on this page.").arg(sh.km.keyFor(A_COMMAND, false)).arg(sh.km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}
