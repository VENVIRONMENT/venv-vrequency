// venv-vrequency — the conversation with the copilot (see chat.h).
#include "app/chat/chat.h"
#include "app/guide/guide.h"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

namespace {
const int kKeep = 200;       // messages kept in the state file
const int kDraftMax = 400;
} // namespace

QStringList chatWrap(const QString &s, int cols, int first) {
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

void Chat::load(const QJsonObject &st) {
    for (const QJsonValue &v : st["chat"].toArray()) {
        const QJsonObject o = v.toObject();
        ChatMsg m;
        m.seq = o["seq"].toInt(); m.from = o["from"].toString(); m.text = o["text"].toString();
        m.kind = o["kind"].toString("say"); m.ts = (qint64)o["ts"].toDouble(); m.user = o["user"].toBool();
        m.page = o["page"].toString(); m.via = o["via"].toString();
        for (const QJsonValue &f : o["focus"].toArray()) m.focus << f.toString();
        if (m.seq > 0 && !m.text.isEmpty()) log.push_back(m);
    }
    seq = st["chatSeq"].toInt(0);
    if (!log.empty()) seq = qMax(seq, log.back().seq);
}

void Chat::save(QJsonObject &o) const {
    QJsonArray ca;
    for (const ChatMsg &m : log) {
        QJsonObject c;
        c["seq"] = m.seq; c["from"] = m.from; c["text"] = m.text; c["kind"] = m.kind; c["ts"] = (double)m.ts; c["user"] = m.user;
        if (!m.page.isEmpty()) c["page"] = m.page;
        if (!m.via.isEmpty()) c["via"] = m.via;
        if (!m.focus.isEmpty()) c["focus"] = QJsonArray::fromStringList(m.focus);
        ca.append(c);
    }
    o["chat"] = ca;
    o["chatSeq"] = seq;
}

int Chat::add(const QString &from, const QString &text, const QString &kind, bool user,
              const QString &page, const QStringList &focus, const QString &via) {
    ChatMsg m;
    m.seq = ++seq;
    m.from = from.left(16);
    m.text = text.left(2000);
    m.kind = kind;
    m.ts = QDateTime::currentMSecsSinceEpoch();
    m.user = user;
    if (user) { m.page = page.left(16); m.focus = focus; m.via = via; }
    log.push_back(m);
    while ((int)log.size() > kKeep) log.pop_front();
    if (user) {
        if (onUserMessage) onUserMessage();
    } else {
        guide->agentSpoke();
        if (sh.page() != PG_CHAT && !guide->dockOpen) {
            ++unread;
            ticker = m;
            tickerUntil = sh.clock.elapsed() + 7000;
        }
    }
    sh.saveSoon(); // saved a moment later, like the track
    sh.redraw();
    return m.seq;
}

void Chat::toggle() {
    if (sh.page() == PG_CHAT) sh.goPage(returnPage == PG_CHAT ? (int)PG_PLAY : returnPage);
    else { returnPage = sh.page(); sh.goPage(PG_CHAT); }
    guide->dockFocus = false; // the page has the keyboard when it comes back
    sh.redraw();
}

void Chat::send() {
    const QString t = draft.trimmed();
    if (t.isEmpty()) return;
    draft.clear();
    scroll = 0;
    if (!guide->askVia.isEmpty()) { // started from a tour card: it is about that control, wherever the player is now
        const QStringList f = guide->askFocus;
        guide->askFocus.clear();
        guide->lastQuestion = f.value(0);
        add("you", t, "say", true, guide->askPage.isEmpty() ? QString(pageName(sh.page())) : guide->askPage, f, guide->askVia);
        guide->askPage.clear(); guide->askVia.clear();
        return;
    }
    const QStringList focus = guide->focus();   // the same path from the Chat page and the dock
    guide->lastQuestion = focus.value(0);
    add("you", t, "say", true, pageName(sh.page()), focus, sh.page() == PG_CHAT ? "chat" : "dock");
}

// Letters type on this page. The command layer (hold Ctrl) and Tab still work.
bool Chat::key(int key, int plainAct, bool dock) {
    if (sh.keys.command || plainAct == A_PAGE_NEXT) return false;
    const QString typed = sh.keys.text;
    sh.keys.text.clear();
    switch (key) {
    case Qt::Key_Return: case Qt::Key_Enter: send(); return true;
    case Qt::Key_Backspace: if (!draft.isEmpty()) draft.chop(1); return true;
    case Qt::Key_Escape:
        if (dock) guide->dockFocus = false;   // the dock stays; the keys play again
        else { sh.goPage(returnPage == PG_CHAT ? (int)PG_PLAY : returnPage); guide->dockFocus = false; }
        return true;
    case Qt::Key_Up: ++scroll; return true;
    case Qt::Key_Down: scroll = qMax(0, scroll - 1); return true;
    case Qt::Key_PageUp: scroll += 10; return true;
    case Qt::Key_PageDown: scroll = qMax(0, scroll - 10); return true;
    default: break;
    }
    QString ch;
    if (!typed.isEmpty() && typed[0].isPrint()) ch = typed;
    else if (key == Qt::Key_Space) ch = " ";
    else if (key >= 0x21 && key <= 0x7e) ch = QString(sh.keys.chromatic ? QChar(key) : QChar(key).toLower());
    if (!ch.isEmpty() && draft.size() + ch.size() <= kDraftMax) draft += ch;
    return true; // everything else is swallowed: no string plays while you type
}
