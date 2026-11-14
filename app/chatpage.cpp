// venv-vrequency — the Chat page: a terminal-style conversation with the copilot (an AI agent
// that reaches the app through the control socket, see control.cpp), plus the ticker that shows
// a new agent message on any other page for a few seconds without taking the keyboard.
#include "mainwindow.h"
#include "palette.h"
#include <QDateTime>
#include <QPainter>
#include <cmath>

using namespace pal;

namespace {
bool inflatedHit(const QRectF &zone, const QRectF &r) { return !zone.isNull() && zone.adjusted(-8, -8, 8, 8).intersects(r.adjusted(-3, -3, 3, 3)); }
const int kKeep = 200;       // messages kept in the state file
const int kDraftMax = 400;
qreal dragLastY = 0;

struct Line { int msg; QString text; bool head; };

// Break `s` into rows of at most `cols` characters, at spaces where possible.
QStringList wrap(const QString &s, int cols, int first = -1) { // `first`: the width of the first row, when it differs
    QStringList out;
    cols = qMax(8, cols);
    auto lim = [&] { return out.isEmpty() && first > 0 ? qMin(first, cols) : cols; };
    for (const QString &para : s.split('\n')) {
        QString cur;
        for (const QString &word : para.split(' ')) {
            QString w = word;
            while (w.size() > lim()) { // a word longer than a row is cut
                if (!cur.isEmpty()) { out << cur; cur.clear(); }
                out << w.left(lim()); w = w.mid(out.last().size());
            }
            if (cur.isEmpty()) cur = w;
            else if (cur.size() + 1 + w.size() <= lim()) cur += " " + w;
            else { out << cur; cur = w; }
        }
        out << cur;
    }
    return out;
}
} // namespace

QStringList chatWrap(const QString &s, int cols, int first) { return wrap(s, cols, first); }

int MainWindow::chatAdd(const QString &from, const QString &text, const QString &kind, bool user,
                        const QString &page, const QStringList &focus, const QString &via) {
    ChatMsg m;
    m.seq = ++chatSeq;
    m.from = from.left(16);
    m.text = text.left(2000);
    m.kind = kind;
    m.ts = QDateTime::currentMSecsSinceEpoch();
    m.user = user;
    if (user) { m.page = page.left(16); m.focus = focus; m.via = via; }
    chat.push_back(m);
    while ((int)chat.size() > kKeep) chat.pop_front();
    if (user) {
        emit userChatArrived();
    } else {
        // a message with no highlight just before it is about nothing in particular
        if (clock.elapsed() - hl.issued > 5000) { topicLabel.clear(); topicZones.clear(); }
        if (pg != PG_CHAT && !dockOpen) {
            ++chatUnread;
            ticker = m;
            tickerUntil = clock.elapsed() + 7000;
        }
    }
    trackSaveTimer.start(2500); // saved a moment later, like the track
    update();
    return m.seq;
}

void MainWindow::chatToggle() {
    if (pg == PG_CHAT) goPage(chatReturn == PG_CHAT ? (int)PG_PLAY : chatReturn);
    else { chatReturn = pg; goPage(PG_CHAT); }
    dockFocus = false; // the page has the keyboard when it comes back
    deepEdit = false;
    update();
}

void MainWindow::chatSend() {
    const QString t = chatDraft.trimmed();
    if (t.isEmpty()) return;
    chatDraft.clear();
    chatScroll = 0;
    if (!askVia.isEmpty()) { // started from a tour card: it is about that control, wherever the player is now
        const QStringList f = askFocus;
        askFocus.clear();
        lastQuestion = f.value(0);
        chatAdd("you", t, "say", true, askPage.isEmpty() ? QString(pageName(pg)) : askPage, f, askVia);
        askPage.clear(); askVia.clear();
        return;
    }
    const QStringList focus = guideFocus();   // the same path from the Chat page and the dock
    lastQuestion = focus.value(0);
    chatAdd("you", t, "say", true, pageName(pg), focus, pg == PG_CHAT ? "chat" : "dock");
}

// Letters type on this page. The command layer (hold Ctrl) and Tab still work.
bool MainWindow::chatKey(int key, int plainAct, bool dock) {
    if (modCommand || plainAct == A_PAGE_NEXT) return false;
    const QString typed = pendingText;
    pendingText.clear();
    switch (key) {
    case Qt::Key_Return: case Qt::Key_Enter: chatSend(); return true;
    case Qt::Key_Backspace: if (!chatDraft.isEmpty()) chatDraft.chop(1); return true;
    case Qt::Key_Escape:
        if (dock) dockFocus = false;   // the dock stays; the keys play again
        else { goPage(chatReturn == PG_CHAT ? (int)PG_PLAY : chatReturn); dockFocus = false; }
        return true;
    case Qt::Key_Up: ++chatScroll; return true;
    case Qt::Key_Down: chatScroll = qMax(0, chatScroll - 1); return true;
    case Qt::Key_PageUp: chatScroll += 10; return true;
    case Qt::Key_PageDown: chatScroll = qMax(0, chatScroll - 10); return true;
    default: break;
    }
    QString ch;
    if (!typed.isEmpty() && typed[0].isPrint()) ch = typed;
    else if (key == Qt::Key_Space) ch = " ";
    else if (key >= 0x21 && key <= 0x7e) ch = QString(modChromatic ? QChar(key) : QChar(key).toLower());
    if (!ch.isEmpty() && chatDraft.size() + ch.size() <= kDraftMax) chatDraft += ch;
    return true; // everything else is swallowed: no string plays while you type
}

void MainWindow::paintChat() {
    chatUnread = 0;
    const bool listening = copilotWaiters > 0 || clock.elapsed() - copilotSeenAt < 45000;
    tag(QRectF(16, 64, 150, 22), "Copilot chat", 14, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(176, 64, 528, 22), listening ? "copilot: listening" : "copilot: not connected", 13, listening ? INK : MID,
        Qt::AlignRight | Qt::AlignVCenter, listening);

    const QRectF hist(16, 92, 688, 500);
    frame(hist, LOW, BG);
    const int cw = chW(13), lh = 17;
    const int cols = (int)((hist.width() - 28) / cw);
    const int rows = (int)((hist.height() - 8) / lh);
    std::vector<Line> lines;
    for (size_t i = 0; i < chat.size(); ++i) {
        lines.push_back({(int)i, QString(), true});
        for (const QString &l : wrap(chat[i].text, cols - 2)) lines.push_back({(int)i, l, false});
    }
    const int total = (int)lines.size();
    chatScroll = qBound(0, chatScroll, qMax(0, total - rows));
    const int end = total - chatScroll, start = qMax(0, end - rows);
    if (chat.empty()) {
        txt(QRectF(hist.x() + 12, hist.y() + 8, hist.width() - 24, 18), "No messages yet. Type below and press Enter.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
        txt(QRectF(hist.x() + 12, hist.y() + 26, hist.width() - 24, 18), "A copilot connected to the control socket answers here.", 13, GRAT, Qt::AlignLeft | Qt::AlignVCenter);
    }
    for (int i = start; i < end; ++i) {
        const Line &ln = lines[i];
        const ChatMsg &m = chat[ln.msg];
        const qreal y = hist.y() + 5 + (i - start) * lh;
        if (ln.head) {
            QString name = m.from.toUpper();
            if (m.kind == "ask") name += "?";
            const qreal w = (name.size() + 2) * cw;
            if (m.user) { frame(QRectF(hist.x() + 10, y, w, lh - 2), MID); txt(QRectF(hist.x() + 10, y, w, lh - 2), name, 13, INK, Qt::AlignCenter, true); }
            else tag(QRectF(hist.x() + 10, y, w, lh - 2), name, 13);
            QString extra = m.kind == "tip" ? "tip  " : "";
            extra += QDateTime::fromMSecsSinceEpoch(m.ts).toString("HH:mm");
            txt(QRectF(hist.x() + 16 + w, y, 200, lh - 2), extra, 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
        } else {
            if (!m.user) g->fillRect(QRectF(hist.x() + 10, y, 2, lh), INK); // agent text carries a rule on the left
            txt(QRectF(hist.x() + 10 + 2 * cw, y, hist.width() - 24, lh), ln.text, 13, m.user ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    if (total > rows) { // scroll marker
        const qreal th = hist.height() - 6, bh = qMax(12.0, th * rows / total);
        const qreal by = hist.y() + 3 + (th - bh) * (total - rows - chatScroll) / (total - rows);
        g->fillRect(QRectF(hist.right() - 5, hist.y() + 3, 1, th), GRAT);
        g->fillRect(QRectF(hist.right() - 6, by, 3, bh), MID);
    }
    zone(hist, "chat_history", [this](QPointF p, int ph) { // drag the history to scroll it
        if (ph == PH_PRESS) { dragLastY = p.y(); return; }
        const int d = (int)std::round((p.y() - dragLastY) / 17.0);
        if (d) { chatScroll += d; dragLastY = p.y(); }
    });

    const QRectF box(16, 600, 580, 44);
    frame(box, INK, BG);
    const int bc = (int)((box.width() - 20) / chW(15));
    QString shown = chatDraft.size() > bc - 1 ? QString::fromUtf8("…") + chatDraft.right(bc - 2) : chatDraft;
    if (chatDraft.isEmpty()) txt(box.adjusted(10, 0, -10, 0), "type a message", 15, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    else txt(box.adjusted(10, 0, -10, 0), shown, 15, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    if ((clock.elapsed() / 500) % 2 == 0) {
        const qreal cx = box.x() + 10 + (chatDraft.isEmpty() ? 0 : shown.size() * chW(15));
        g->fillRect(QRectF(cx, box.y() + 10, chW(15) - 2, 24), INK);
    }
    zone(box, "chat_input", [](QPointF, int) {});
    button(QRectF(604, 600, 100, 44), "chat_send", "Send", false, [this] { chatSend(); });
    txt(QRectF(18, 652, 686, 18), "Enter sends  Backspace edits  Up/Down scrolls  Esc leaves", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(18, 670, 686, 18), QString("%1+Shift+%2 jumps here from any page and back. Letters type on this page.").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}

// A new agent message on any other page: two lines at the bottom, for a few seconds. It draws
// over the page and takes no key, so playing carries on; touching it opens the chat dock to answer.
void MainWindow::paintTicker() {
    if (pg == PG_CHAT || dockOpen || clock.elapsed() >= tickerUntil) return;
    QRectF r(16, 668, 688, 44);
    for (const QString &n : highlightedZones()) // never over what is highlighted: then it goes to the top
        if (inflatedHit(zoneRect(n), r)) { r = QRectF(16, 6, 688, 44); break; }
    frame(r.adjusted(-2, -2, 2, 2), INK, BG);
    const int cw = chW(13);
    QString name = ticker.from.toUpper();
    if (ticker.kind == "ask") name += "?";
    const qreal tw = (name.size() + 2) * cw;
    tag(QRectF(r.x() + 5, r.y() + 4, tw, 17), name, 13);
    const QString flat = QString(ticker.text).replace('\n', ' ');
    const int c1 = (int)((r.width() - tw - 20) / cw), c2 = (int)((r.width() - 12 - 150) / cw);
    QString l1 = flat.left(c1), rest = flat.mid(c1);
    if (flat.size() > c1) { // break at a space when one is near
        const int sp = l1.lastIndexOf(' ');
        if (sp > c1 / 2) { l1 = flat.left(sp); rest = flat.mid(sp + 1); }
    }
    txt(QRectF(r.x() + 12 + tw, r.y() + 4, r.width() - tw - 16, 17), l1, 13, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    txt(QRectF(r.x() + 6, r.y() + 23, r.width() - 12 - 150, 17), fit(rest, 13, c2 * cw), 13, INK, Qt::AlignLeft | Qt::AlignVCenter);
    txt(QRectF(r.right() - 150, r.y() + 23, 144, 17), QString("%1+%2 to reply").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignRight | Qt::AlignVCenter);
    zone(r.adjusted(-2, -2, 2, 2), "ticker", [this](QPointF, int ph) { if (ph == PH_PRESS) { tickerUntil = 0; dockOpenFocus(); } }); // touch the ticker to answer
}
