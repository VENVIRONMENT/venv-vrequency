// venv-vrequency — the control socket: replies, parsing a request, dispatch, and the chat commands.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"
#include <QJsonDocument>
#include <QLocalSocket>

using namespace td;
using namespace ctl;

void ControlServer::reply(QLocalSocket *s, const QJsonValue &id, bool ok, const QJsonObject &result, const QString &error) {
    if (!s || s->state() != QLocalSocket::ConnectedState) return;
    QJsonObject o;
    o["id"] = id;
    o["ok"] = ok;
    if (ok) o["result"] = result; else o["error"] = error;
    s->write(QJsonDocument(o).toJson(QJsonDocument::Compact) + '\n');
}

void ControlServer::line(Conn *c, const QByteArray &ln) {
    if (ln.trimmed().isEmpty()) return;
    QJsonParseError pe;
    const QJsonDocument d = QJsonDocument::fromJson(ln, &pe);
    if (pe.error != QJsonParseError::NoError || !d.isObject()) {
        reply(c->sock, QJsonValue(), false, QJsonObject(), pe.error != QJsonParseError::NoError ? "bad JSON: " + pe.errorString() : "a request must be a JSON object");
        return;
    }
    const QJsonObject req = d.object();
    const QJsonValue id = req.contains("id") ? req["id"] : QJsonValue();
    if (!req["cmd"].isString()) { reply(c->sock, id, false, QJsonObject(), "missing 'cmd'"); return; }
    if (req["cmd"].toString() == "chat_wait") { // answered later, from onUserChat or the timer
        QString e;
        Args a{req, e};
        const int since = a.integer("since", 0, 2000000000, 0);
        const int ms = a.integer("timeout_ms", 0, 30000, 25000);
        if (!e.isEmpty()) { reply(c->sock, id, false, QJsonObject(), e); return; }
        w.chat.seen();
        bool ready = false;
        for (const ChatMsg &m : w.chat.log) ready = ready || (m.user && m.seq > since);
        if (ready || ms == 0) {
            QJsonObject r; r["messages"] = chatJson(since, 200); r["last_seq"] = w.chat.seq; r["timed_out"] = !ready;
            reply(c->sock, id, true, r, QString());
            return;
        }
        Waiter wt;
        wt.sock = c->sock; wt.id = id; wt.since = since;
        wt.timer = new QTimer(this);
        wt.timer->setSingleShot(true);
        QPointer<QLocalSocket> key = c->sock;
        connect(wt.timer, &QTimer::timeout, this, [this, key, id] {
            for (size_t i = 0; i < waiters.size(); ++i)
                if (waiters[i].sock == key && waiters[i].id == id && waiters[i].timer == sender()) { finishWaiter(i, true); return; }
        });
        wt.timer->start(ms);
        waiters.push_back(wt);
        updateWaiters();
        return;
    }
    const QJsonObject res = handle(req);
    reply(c->sock, id, res["ok"].toBool(), res["result"].toObject(), res["error"].toString());
}

void ControlServer::updateWaiters() { w.chat.waiting((int)waiters.size()); }

void ControlServer::finishWaiter(size_t i, bool timedOut) {
    Waiter wt = waiters[i];
    waiters.erase(waiters.begin() + i);
    updateWaiters();
    QJsonObject r;
    r["messages"] = chatJson(wt.since, 200);
    r["last_seq"] = w.chat.seq;
    r["timed_out"] = timedOut;
    reply(wt.sock, wt.id, true, r, QString());
    delete wt.timer;
}

void ControlServer::onUserChat() {
    for (size_t i = waiters.size(); i-- > 0;) {
        bool user = false;
        for (const ChatMsg &m : w.chat.log) user = user || (m.user && m.seq > waiters[i].since);
        if (user) finishWaiter(i, false);
    }
}

QJsonArray ControlServer::chatJson(int since, int limit) const {
    QJsonArray a;
    for (const ChatMsg &m : w.chat.log) if (m.seq > since) a.append(msgJson(m));
    while (a.size() > limit) a.removeFirst();
    return a;
}

QJsonObject ControlServer::handle(const QJsonObject &req) {
    QJsonObject out;
    out["id"] = req.contains("id") ? req["id"] : QJsonValue();
    if (!req["cmd"].isString()) { out["ok"] = false; out["error"] = "missing 'cmd'"; return out; }
    QString e;
    const QJsonObject r = dispatch(req["cmd"].toString(), req, e);
    out["ok"] = e.isEmpty();
    if (e.isEmpty()) out["result"] = r; else out["error"] = e;
    return out;
}

QJsonObject ControlServer::dispatch(const QString &cmd, const QJsonObject &a, QString &e) {
    if (cmd == "state") return cmdState();
    if (cmd == "note") return cmdNote(a, e, false);
    if (cmd == "notes" || cmd == "chord") return cmdNote(a, e, true);
    if (cmd == "slide") return cmdSlide(a, e);
    if (cmd == "release" || cmd == "panic") {
        if (cmd == "panic") {
            w.perform(A_PANIC);
            liveNotes.clear();
            return QJsonObject();
        }
        return cmdRelease(a, e);
    }
    if (cmd == "set_param") return cmdParams(a, e, true);
    if (cmd == "get_params") return cmdParams(a, e, false);
    if (cmd == "sound") return cmdSound(a, e);
    if (cmd == "save_favourite") return cmdFavourite(a, e);
    if (cmd == "scale" || cmd == "tempo" || cmd == "octave" || cmd == "bend" || cmd == "sustain") return cmdPlayControls(cmd, a, e);
    if (cmd == "loop") return cmdLoop(a, e);
    if (cmd == "pulse") return cmdPulse(a, e);
    if (cmd == "page") return cmdPage(a, e);
    if (cmd == "highlight" || cmd == "unhighlight" || cmd == "zones") return cmdGuide(cmd, a, e);
    if (cmd == "tour") return cmdTour(a, e);
    if (cmd == "session") return cmdSession(a, e);
    if (cmd == "screenshot") return cmdScreenshot(e);
    if (cmd == "chat_say" || cmd == "chat_poll") return cmdChat(cmd, a, e);
    if (cmd == "chat_wait") { e = "chat_wait needs a socket connection"; return QJsonObject(); }
    e = "unknown command '" + cmd.left(40) + "'";
    return QJsonObject();
}

QJsonObject ControlServer::cmdChat(const QString &cmd, const QJsonObject &a, QString &e) {
    Args A{a, e};
    QJsonObject r;
    if (cmd == "chat_say") {
        const QString text = A.str("text", "", 2000, true).trimmed();
        QString from = A.str("from", "rook", 16).trimmed();
        const QString kind = A.str("kind", "say", 4);
        if (e.isEmpty() && text.isEmpty()) e = "'text' is empty";
        if (e.isEmpty() && from.isEmpty()) e = "'from' is empty";
        if (e.isEmpty() && kind != "say" && kind != "tip" && kind != "ask") e = "kind must be say, tip or ask";
        if (e.isEmpty() && from.compare("you", Qt::CaseInsensitive) == 0) e = "'you' is the player's name";
        if (!e.isEmpty()) return {};
        r["seq"] = w.chat.add(from, text, kind, false);
        w.chat.seen();
        return r;
    }
    const int since = A.integer("since", 0, 2000000000, 0);
    const int limit = A.integer("limit", 1, 200, 200);
    if (!e.isEmpty()) return {};
    w.chat.seen();
    r["messages"] = chatJson(since, limit);
    r["last_seq"] = w.chat.seq;
    return r;
}

