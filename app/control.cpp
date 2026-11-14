// venv-vrequency — the control socket (see control.h and docs/CONTROL.md).
#include "control.h"
#include "tour.h"
#include "mainwindow.h"
#include "store.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QRegularExpression>
#include <cmath>

using namespace td;

namespace {
const int kMaxLine = 1 << 20;     // longest request line
const int kMaxClients = 8;
const int kLinesPerTurn = 100;    // requests handled before the GUI gets a turn
const int kListenSecs = 45000;

// Argument reading: the first problem found is kept in `e`, and the caller refuses the request.
struct Args {
    const QJsonObject &o;
    QString &e;
    bool has(const char *k) const { return o.contains(k) && !o[k].isNull(); }
    void fail(const QString &m) const { if (e.isEmpty()) e = m; }
    double num(const char *k, double lo, double hi, double def, bool required = false) const {
        if (!has(k)) { if (required) fail(QString("missing '%1'").arg(k)); return def; }
        if (!o[k].isDouble()) { fail(QString("'%1' must be a number").arg(k)); return def; }
        const double d = o[k].toDouble();
        if (!(d >= lo && d <= hi)) { fail(QString("'%1' must be between %2 and %3").arg(k).arg(lo).arg(hi)); return def; }
        return d;
    }
    int integer(const char *k, int lo, int hi, int def, bool required = false) const {
        const double d = num(k, lo, hi, def, required);
        if (has(k) && d != std::floor(d)) { fail(QString("'%1' must be a whole number").arg(k)); return def; }
        return (int)d;
    }
    QString str(const char *k, const QString &def, int maxLen, bool required = false) const {
        if (!has(k)) { if (required) fail(QString("missing '%1'").arg(k)); return def; }
        if (!o[k].isString()) { fail(QString("'%1' must be a string").arg(k)); return def; }
        const QString s = o[k].toString();
        if (s.size() > maxLen) { fail(QString("'%1' is longer than %2 characters").arg(k).arg(maxLen)); return def; }
        return s;
    }
    bool boolean(const char *k, bool def) const {
        if (!has(k)) return def;
        if (!o[k].isBool()) { fail(QString("'%1' must be true or false").arg(k)); return def; }
        return o[k].toBool();
    }
};

const char *pageNames[MainWindow::PG_COUNT] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
const char *loopNames[] = {"empty", "count-in", "recording", "playing", "overdubbing", "stopped"};

int parseRoot(const QJsonValue &v) {
    if (v.isDouble()) { const double d = v.toDouble(); return (d >= 0 && d < 12 && d == std::floor(d)) ? (int)d : -1; }
    if (!v.isString()) return -1;
    QString s = v.toString().trimmed().toUpper();
    static const char *sharp[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    static const char *flat[12] = {"C", "DB", "D", "EB", "E", "F", "GB", "G", "AB", "A", "BB", "B"};
    for (int i = 0; i < 12; ++i) if (s == sharp[i] || s == flat[i]) return i;
    return -1;
}

int parseDrum(const QJsonValue &v) {
    if (v.isDouble()) { const double d = v.toDouble(); return (d >= 0 && d < kDrums && d == std::floor(d)) ? (int)d : -1; }
    if (!v.isString()) return -1;
    const QString s = v.toString().toLower().remove(' ').remove('_').remove('-');
    for (int i = 0; i < kDrums; ++i) if (s == QString(drumName(i)).toLower().remove(' ')) return i;
    return -1;
}

int parseDegree(const QJsonValue &v) { // a chord: 1..7 or a roman numeral
    if (v.isDouble()) { const double d = v.toDouble(); return (d >= 1 && d <= 7 && d == std::floor(d)) ? (int)d - 1 : -1; }
    if (!v.isString()) return -1;
    static const char *R[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    const QString s = v.toString().trimmed().toUpper();
    for (int i = 0; i < 7; ++i) if (s == R[i]) return i;
    return -1;
}

QJsonObject msgJson(const ChatMsg &m) {
    QJsonObject o;
    o["seq"] = m.seq; o["from"] = m.from; o["text"] = m.text; o["kind"] = m.kind;
    o["ts"] = (double)m.ts; o["user"] = m.user;
    if (m.user) { // what the player was looking at (see highlight)
        if (!m.page.isEmpty()) o["page"] = m.page;
        o["focus"] = QJsonArray::fromStringList(m.focus);
        if (!m.via.isEmpty()) o["via"] = m.via;
    }
    return o;
}
} // namespace

// --------------------------------------------------------------------- lifecycle

ControlServer::ControlServer(MainWindow &win, Instrument &i) : w(win), inst(i) {
    connect(&server, &QLocalServer::newConnection, this, &ControlServer::onConnect);
    connect(&w, &MainWindow::userChatArrived, this, &ControlServer::onUserChat);
}

ControlServer::~ControlServer() { stop(); }

QString ControlServer::defaultPath() {
    const QString o = qEnvironmentVariable("VREQUENCY_CONTROL_SOCKET");
    if (!o.isEmpty()) return o;
    const QString xdg = qEnvironmentVariable("XDG_RUNTIME_DIR");
    if (!xdg.isEmpty() && QFileInfo(xdg).isDir()) return xdg + "/vrequency.sock";
    return store::baseDir() + "/control.sock";
}

bool ControlServer::start(const QString &p) {
    stop();
    sockPath = p.isEmpty() ? defaultPath() : p;
    if (QFile::exists(sockPath)) { // something left over: is anyone there?
        QLocalSocket probe;
        probe.connectToServer(sockPath);
        if (probe.waitForConnected(300)) { err = "another vrequency already answers on " + sockPath; sockPath.clear(); return false; }
        QLocalServer::removeServer(sockPath); // a crashed run's file
    }
    QDir().mkpath(QFileInfo(sockPath).absolutePath());
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.setMaxPendingConnections(kMaxClients);
    if (!server.listen(sockPath)) { err = server.errorString(); sockPath.clear(); return false; }
    QFile::setPermissions(sockPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner); // 0600
    return true;
}

void ControlServer::stop() {
    for (Waiter &wt : waiters) delete wt.timer;
    waiters.clear();
    w.chatWaiting(0);
    for (Conn *c : conns) {
        if (c->sock) { c->sock->disconnect(this); c->sock->disconnectFromServer(); c->sock->deleteLater(); }
        delete c;
    }
    conns.clear();
    for (auto it = liveNotes.begin(); it != liveNotes.end(); ++it) inst.note(it.value(), it.key(), 0, false);
    liveNotes.clear();
    if (server.isListening()) server.close();
    if (!sockPath.isEmpty()) { QFile::remove(sockPath); }
}

void ControlServer::onConnect() {
    while (QLocalSocket *s = server.nextPendingConnection()) {
        if ((int)conns.size() >= kMaxClients) { s->disconnectFromServer(); s->deleteLater(); continue; }
        Conn *c = new Conn;
        c->sock = s;
        conns.push_back(c);
        connect(s, &QLocalSocket::readyRead, this, [this, c] { onData(c); });
        connect(s, &QLocalSocket::disconnected, this, [this, c] { drop(c); });
    }
}

void ControlServer::drop(Conn *c) {
    for (size_t i = waiters.size(); i-- > 0;)
        if (!waiters[i].sock || waiters[i].sock == c->sock) { delete waiters[i].timer; waiters.erase(waiters.begin() + i); }
    updateWaiters();
    for (size_t i = 0; i < conns.size(); ++i)
        if (conns[i] == c) { conns.erase(conns.begin() + i); break; }
    if (c->sock) c->sock->deleteLater();
    delete c;
}

void ControlServer::onData(Conn *c) {
    if (!c->sock) return;
    c->buf += c->sock->readAll();
    int handled = 0;
    for (;;) {
        const int nl = c->buf.indexOf('\n');
        if (nl < 0) {
            if (c->buf.size() > kMaxLine) { // a line that never ends: refuse it and throw it away
                c->buf.clear();
                if (!c->discarding) reply(c->sock, QJsonValue(), false, QJsonObject(), "request line too long");
                c->discarding = true;
            }
            return;
        }
        QByteArray ln = c->buf.left(nl);
        c->buf.remove(0, nl + 1);
        if (c->discarding) { c->discarding = false; continue; } // the tail of a refused line
        if (ln.size() > kMaxLine) { reply(c->sock, QJsonValue(), false, QJsonObject(), "request line too long"); continue; }
        line(c, ln);
        if (!c->sock) return;
        if (++handled >= kLinesPerTurn) { // the rest after the window has had its turn
            QPointer<QLocalSocket> s = c->sock;
            QTimer::singleShot(0, this, [this, c, s] {
                for (Conn *k : conns) if (k == c && s) { onData(c); return; }
            });
            return;
        }
    }
}

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
        w.chatSeen();
        bool ready = false;
        for (const ChatMsg &m : w.chat) ready = ready || (m.user && m.seq > since);
        if (ready || ms == 0) {
            QJsonObject r; r["messages"] = chatJson(since, 200); r["last_seq"] = w.chatSeq; r["timed_out"] = !ready;
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

void ControlServer::updateWaiters() { w.chatWaiting((int)waiters.size()); }

void ControlServer::finishWaiter(size_t i, bool timedOut) {
    Waiter wt = waiters[i];
    waiters.erase(waiters.begin() + i);
    updateWaiters();
    QJsonObject r;
    r["messages"] = chatJson(wt.since, 200);
    r["last_seq"] = w.chatSeq;
    r["timed_out"] = timedOut;
    reply(wt.sock, wt.id, true, r, QString());
    delete wt.timer;
}

void ControlServer::onUserChat() {
    for (size_t i = waiters.size(); i-- > 0;) {
        bool user = false;
        for (const ChatMsg &m : w.chat) user = user || (m.user && m.seq > waiters[i].since);
        if (user) finishWaiter(i, false);
    }
}

QJsonArray ControlServer::chatJson(int since, int limit) const {
    QJsonArray a;
    for (const ChatMsg &m : w.chat) if (m.seq > since) a.append(msgJson(m));
    while (a.size() > limit) a.removeFirst();
    return a;
}

// --------------------------------------------------------------------- commands

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
    if (cmd == "backing") return cmdBacking(a, e);
    if (cmd == "beat") return cmdBeat(a, e);
    if (cmd == "track") return cmdTrack(a, e);
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

QJsonObject ControlServer::cmdState() {
    const UiState u = inst.uiState();
    QJsonObject o;
    o["page"] = pageNames[qBound(0, w.pg, (int)MainWindow::PG_COUNT - 1)];
    QJsonObject snd;
    snd["name"] = QString::fromUtf8(w.lead.name);
    snd["slot"] = w.curSlot >= 0 ? QJsonValue(w.curSlot + 1) : QJsonValue();
    snd["slots"] = [&] { QJsonArray s; for (int k = 0; k < 10; ++k) s.append(QString::fromUtf8(w.bank[k].name)); return s; }();
    QJsonArray fav;
    for (const store::NamedPatch &f : w.favs) fav.append(QString::fromUtf8(f.patch.name));
    snd["favourites"] = fav;
    QJsonObject pr;
    for (int p = 0; p < P_COUNT; ++p) pr[paramInfo(p).key] = (double)w.lead.p[p];
    snd["params"] = pr;
    o["sound"] = snd;
    QJsonObject sc;
    sc["name"] = scaleDef(u.set.tuning.scale).name;
    sc["index"] = u.set.tuning.scale;
    sc["root"] = noteName(u.set.tuning.root);
    sc["octave"] = u.set.tuning.octave;
    o["scale"] = sc;
    o["tempo"] = (double)u.set.bpm;
    o["volume"] = (double)u.set.volume;
    QJsonObject lp;
    lp["state"] = loopNames[qBound(0, u.loopState, 5)];
    lp["layers"] = u.layers; lp["bars"] = u.loopBars;
    o["looper"] = lp;
    QJsonObject bt;
    bt["playing"] = u.beatPlaying; bt["rec"] = u.beatRec; bt["kit"] = QString(kitName(u.set.kit)).toLower();
    QJsonObject steps;
    for (int d = 0; d < kDrums; ++d) {
        QString s;
        for (int k = 0; k < kDrumSteps; ++k) s += (u.beatPat[d] >> k) & 1 ? 'x' : '.';
        steps[QString(drumName(d)).toLower().remove(' ')] = s;
    }
    bt["steps"] = steps;
    o["beat"] = bt;
    QJsonObject bk;
    bk["playing"] = u.backPlaying;
    bk["mode"] = QString(backModeName(u.set.backMode)).toLower();
    bk["style"] = QString(backStyleName(u.set.backStyle)).toLower();
    bk["seventh"] = u.set.seventh;
    QJsonArray ch, deg;
    for (int i = 0; i < u.progLen && i < kProgMax; ++i) {
        ch.append(QString::fromUtf8(buildChord(u.set.tuning, u.prog[i], u.set.seventh).name));
        deg.append(u.prog[i] + 1);
    }
    bk["chords"] = ch; bk["degrees"] = deg;
    bk["position"] = u.progPos;
    o["backing"] = bk;
    QJsonObject tr;
    tr["playing"] = u.trackPlaying; tr["bars"] = w.track.bars;
    QJsonArray clips, lanes;
    static const char *kinds[] = {"beat", "chords", "loop"};
    for (size_t i = 0; i < w.track.clips.size(); ++i) {
        QJsonObject c;
        c["index"] = (int)i; c["name"] = QString::fromUtf8(w.track.clips[i]->name);
        c["kind"] = kinds[qBound(0, w.track.clips[i]->kind, 2)]; c["bars"] = w.track.clips[i]->bars;
        clips.append(c);
    }
    static const char *laneN[kLanes] = {"beat", "chords", "loop_a", "loop_b"};
    for (int l = 0; l < kLanes; ++l) {
        QJsonArray pl;
        for (const Placement &p : w.track.lane[l]) { QJsonObject x; x["clip"] = p.clip; x["bar"] = p.bar; x["bars"] = p.bars; x["mute"] = p.mute; pl.append(x); }
        QJsonObject ln; ln["lane"] = laneN[l]; ln["placements"] = pl; lanes.append(ln);
    }
    tr["clips"] = clips; tr["lanes"] = lanes;
    tr["exporting"] = w.exportStatus() == 1;
    o["track"] = tr;
    o["playing"] = u.voices > 0 || u.backPlaying || u.beatPlaying || u.trackPlaying || u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    o["voices"] = u.voices;
    o["toast"] = w.toastText;
    QJsonObject cp;
    cp["listening"] = w.copilotWaiters > 0 || w.clock.elapsed() - w.copilotSeenAt < kListenSecs;
    cp["last_seq"] = w.chatSeq;
    cp["unread"] = w.chatUnread;
    o["chat"] = cp;
    o["chat_tail"] = chatJson(qMax(0, w.chatSeq - 10), 10);
    QJsonObject hf;
    hf["page"] = pageNames[qBound(0, w.pg, (int)MainWindow::PG_COUNT - 1)];
    hf["highlighted"] = QJsonArray::fromStringList(w.highlightedZones());
    hf["dock_open"] = w.dockIsOpen() && w.pg != MainWindow::PG_CHAT;
    hf["dock_focus"] = w.dockHasFocus();
    hf["last_question_zone"] = w.lastQuestionZone();
    hf["topic"] = w.dockTopic();
    o["help_focus"] = hf;
    o["tour"] = w.tour().stateJson();
    return o;
}

// The pitch a request asks for: `degKey` is a scale degree on the string (0 = its first key,
// negative or past an octave allowed), `midiKey` an exact MIDI note.
bool ControlServer::notePitch(const QJsonObject &a, int string, const char *degKey, const char *midiKey, float &midi, QString &e) const {
    Args A{a, e};
    const bool hasDeg = A.has(degKey), hasMidi = A.has(midiKey);
    if (hasDeg == hasMidi) { A.fail(QString("give either '%1' or '%2'").arg(degKey, midiKey)); return false; }
    if (hasDeg) {
        const int d = A.integer(degKey, -48, 48, 0);
        if (!e.isEmpty()) return false;
        midi = (float)inst.uiState().set.tuning.noteFor(string, d, false);
    } else {
        midi = (float)A.num(midiKey, 12, 120, 60);
    }
    return e.isEmpty();
}

int ControlServer::startNote(int string, float midi, float vel, int durMs) {
    int id = w.nextNoteId++;
    if (w.nextNoteId > 0x7ff) w.nextNoteId = 0x400;
    liveNotes[id] = string;
    inst.note(string, id, midi, true, vel);
    QTimer::singleShot(durMs, this, [this, id] { releaseNote(id); });
    return id;
}

void ControlServer::releaseNote(int id) {
    auto it = liveNotes.find(id);
    if (it == liveNotes.end()) return;
    inst.note(it.value(), id, 0, false);
    liveNotes.erase(it);
}

QJsonObject ControlServer::cmdNote(const QJsonObject &a, QString &e, bool chord) {
    Args A{a, e};
    struct N { int string; float midi, vel; int dur; };
    std::vector<N> list;
    const int dur0 = A.integer("dur_ms", 20, 30000, 500);
    const float vel0 = (float)A.num("velocity", 0, 1, 1);
    if (!e.isEmpty()) return {};
    auto one = [&](const QJsonObject &o, int forcedString) {
        Args B{o, e};
        N n;
        n.string = forcedString >= 0 ? forcedString : B.integer("string", 0, kStrings - 1, 0, true);
        n.dur = B.integer("dur_ms", 20, 30000, dur0);
        n.vel = (float)B.num("velocity", 0, 1, vel0);
        if (!e.isEmpty()) return;
        if (!notePitch(o, n.string, "degree", "midi", n.midi, e)) return;
        list.push_back(n);
    };
    if (!chord && !a.contains("notes")) {
        one(a, -1);
    } else if (a.contains("notes") && a["notes"].isArray()) {
        const QJsonArray arr = a["notes"].toArray();
        if (arr.isEmpty() || arr.size() > kStrings * 4) A.fail("'notes' must hold 1 to 12 notes");
        for (const QJsonValue &v : arr) {
            if (!e.isEmpty()) break;
            if (!v.isObject()) { A.fail("every note must be an object"); break; }
            one(v.toObject(), -1);
        }
    } else if (a.contains("degrees") || a.contains("midis")) { // a chord, one note on each string from the lowest
        const bool deg = a.contains("degrees");
        const QJsonValue v = a[deg ? "degrees" : "midis"];
        if (!v.isArray() || v.toArray().isEmpty() || v.toArray().size() > kStrings) A.fail("give 1 to 3 values for the strings, lowest first");
        const QJsonArray arr = v.toArray();
        for (int i = 0; i < arr.size() && e.isEmpty(); ++i) {
            if (!arr[i].isDouble()) { A.fail("every value must be a number"); break; }
            QJsonObject o; o[deg ? "degree" : "midi"] = arr[i];
            one(o, i);
        }
    } else {
        A.fail("give 'notes', 'degrees' or 'midis'");
    }
    if (!e.isEmpty()) return {};
    QJsonArray played;
    for (const N &n : list) {
        const int id = startNote(n.string, n.midi, n.vel, n.dur);
        QJsonObject p; p["string"] = n.string; p["midi"] = (double)n.midi; p["hz"] = midiToHz(n.midi); p["id"] = id;
        played.append(p);
    }
    QJsonObject r;
    r["played"] = played;
    return r;
}

QJsonObject ControlServer::cmdSlide(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const int s = A.integer("string", 0, kStrings - 1, 0, true);
    const int ms = A.integer("ms", 50, 30000, 600, false);
    const double vel = A.num("velocity", 0, 1, 1);
    const bool midi = A.str("unit", "degree", 8) == "midi";
    if (!e.isEmpty()) return {};
    if (!a.contains("from") || !a.contains("to")) { e = "slide needs 'from' and 'to'"; return {}; }
    QJsonObject f, t;
    f[midi ? "midi" : "degree"] = a["from"]; t[midi ? "midi" : "degree"] = a["to"];
    float m0, m1;
    if (!a["from"].isDouble() || !a["to"].isDouble()) { e = "'from' and 'to' must be numbers"; return {}; }
    if (!notePitch(f, s, "degree", "midi", m0, e) || !notePitch(t, s, "degree", "midi", m1, e)) return {};
    // the first note sounds, then the second is pressed while it is held: the string glides at the sound's glide time
    const int id = startNote(s, m0, (float)vel, ms);
    const int hold = qMin(ms / 3, 800);
    QTimer::singleShot(hold, this, [this, s, m1, ms, hold, id, vel] {
        if (!liveNotes.contains(id)) return;
        const int id2 = startNote(s, m1, (float)vel, qMax(20, ms - hold));
        (void)id2;
        releaseNote(id); // the first key lifts: the string stays on the second
    });
    QJsonObject r;
    r["from_midi"] = (double)m0; r["to_midi"] = (double)m1;
    return r;
}

QJsonObject ControlServer::cmdRelease(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const int s = A.integer("string", -1, kStrings - 1, -1);
    if (!e.isEmpty()) return {};
    const QList<int> ids = liveNotes.keys();
    int n = 0;
    for (int id : ids) if (s < 0 || liveNotes[id] == s) { releaseNote(id); ++n; }
    QJsonObject r; r["released"] = n;
    return r;
}

QJsonObject ControlServer::cmdParams(const QJsonObject &a, QString &e, bool set) {
    Args A{a, e};
    QJsonObject r;
    if (set) {
        const QString name = A.str("name", "", 24, true);
        const double v = A.num("value", -1e9, 1e9, 0, true);
        if (!e.isEmpty()) return {};
        int p = -1;
        for (int i = 0; i < P_COUNT; ++i) if (name == paramInfo(i).key) p = i;
        if (p < 0) { e = "unknown parameter '" + name + "' (get_params lists them)"; return {}; }
        const ParamInfo &m = paramInfo(p);
        float x = (float)qBound((double)m.min, v, (double)m.max);
        if (m.integer) x = std::round(x);
        w.setParam(p, x);
        w.curSlot = -1;
        r["name"] = name; r["value"] = (double)x; r["clamped"] = (double)x != v;
        return r;
    }
    QJsonArray arr;
    for (int p = 0; p < P_COUNT; ++p) {
        const ParamInfo &m = paramInfo(p);
        QJsonObject o;
        o["name"] = m.key; o["label"] = m.label; o["value"] = (double)w.lead.p[p];
        o["min"] = (double)m.min; o["max"] = (double)m.max; o["default"] = (double)m.def; o["integer"] = m.integer;
        arr.append(o);
    }
    r["sound"] = QString::fromUtf8(w.lead.name);
    r["params"] = arr;
    return r;
}

QJsonObject ControlServer::cmdSound(const QJsonObject &a, QString &e) {
    QJsonObject r;
    QJsonValue v = a.contains("slot") ? a["slot"] : a["name"];
    if (v.isDouble()) {
        const double d = v.toDouble();
        if (!(d >= 1 && d <= 10) || d != std::floor(d)) { e = "'slot' must be a whole number from 1 to 10"; return {}; }
        w.slotSave = false;
        w.selectSlot((int)d - 1);
    } else if (v.isString()) {
        const QString s = v.toString().trimmed();
        if (s.size() > 40) { e = "name too long"; return {}; }
        if (s.compare("random", Qt::CaseInsensitive) == 0) w.randomise();
        else if (s.compare("mutate", Qt::CaseInsensitive) == 0) w.mutate();
        else if (s.compare("undo", Qt::CaseInsensitive) == 0) {
            if (w.undo.empty()) { e = "nothing to undo"; return {}; }
            w.undoSound();
        } else {
            bool found = false;
            for (const store::NamedPatch &f : w.favs)
                if (!found && QString::fromUtf8(f.patch.name).compare(s, Qt::CaseInsensitive) == 0) {
                    found = true;
                    w.pushUndo();
                    w.lead = f.patch;
                    w.applyLead(true);
                }
            if (!found) { e = "no favourite called '" + s + "'"; return {}; }
        }
    } else {
        e = "give 'slot' (1 to 10) or 'name' (a favourite, random, mutate or undo)";
        return {};
    }
    r["name"] = QString::fromUtf8(w.lead.name);
    r["slot"] = w.curSlot >= 0 ? QJsonValue(w.curSlot + 1) : QJsonValue();
    return r;
}

QJsonObject ControlServer::cmdFavourite(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString name = A.str("name", "", 24, true).trimmed();
    if (e.isEmpty() && (name.isEmpty() || name.contains(QRegularExpression("[\\x00-\\x1f]")))) e = "'name' must be 1 to 24 printable characters";
    if (!e.isEmpty()) return {};
    Patch p = w.lead;
    p.setName(name.toUtf8().constData());
    bool updated = false;
    const QString path = store::savePatch("favourites", p, true, &updated);
    w.refreshLists();
    if (path.isEmpty()) { e = "could not write the favourite"; return {}; }
    w.toast((updated ? "Updated " : "Favourite saved: ") + name);
    QJsonObject r; r["path"] = path; r["name"] = name; r["updated"] = updated;
    return r;
}

QJsonObject ControlServer::cmdPlayControls(const QString &cmd, const QJsonObject &a, QString &e) {
    Args A{a, e};
    QJsonObject r;
    if (cmd == "scale") {
        if (!A.has("name") && !A.has("root")) { e = "give 'name' and/or 'root'"; return {}; }
        int sc = -1, root = -1;
        if (A.has("name")) {
            const QJsonValue v = a["name"];
            if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() < SC_COUNT && v.toDouble() == std::floor(v.toDouble())) sc = (int)v.toDouble();
            else if (v.isString()) {
                const QString s = v.toString().toLower().remove(' ').remove('-').remove('_');
                for (int i = 0; i < SC_COUNT; ++i)
                    if (s == QString(scaleDef(i).name).toLower().remove(' ') || s == QString(scaleDef(i).shortName).toLower().remove(' '))
                        sc = i;
                if (s == "minorpentatonic") sc = SC_MINPENT;
                if (s == "majorpentatonic") sc = SC_MAJPENT;
            }
            if (sc < 0) { e = "unknown scale (major, minor, majpent, minpent, blues, dorian, mixolydian, chromatic)"; return {}; }
        }
        if (A.has("root")) {
            root = parseRoot(a["root"]);
            if (root < 0) { e = "'root' must be a note name like A, C# or Bb, or 0 to 11"; return {}; }
        }
        if (sc >= 0) inst.set(S_SCALE, (float)sc);
        if (root >= 0) inst.set(S_ROOT, (float)root);
        if (sc >= 0) r["scale"] = scaleDef(sc).name;
        if (root >= 0) r["root"] = noteName(root);
    } else if (cmd == "tempo") {
        const double bpm = A.num("bpm", 40, 240, 100, true);
        if (!e.isEmpty()) return {};
        inst.set(S_BPM, (float)bpm);
        r["bpm"] = bpm;
    } else if (cmd == "octave") {
        const int cur = inst.uiState().set.tuning.octave;
        int target = cur;
        if (A.has("value")) target = A.integer("value", -3, 3, cur);
        else if (A.has("delta")) target = qBound(-3, cur + A.integer("delta", -6, 6, 0), 3);
        else A.fail("give 'value' (-3 to 3) or 'delta'");
        if (!e.isEmpty()) return {};
        for (int i = 0; i < qAbs(target - cur); ++i) inst.cmd(EV_OCTAVE, target > cur ? 1 : -1);
        r["octave"] = target;
    } else if (cmd == "bend") {
        const double v = A.num("value", -1, 1, 0, true);
        const int ms = A.integer("ms", 0, 10000, 0);
        if (!e.isEmpty()) return {};
        Event ev; ev.type = EV_BEND; ev.f = (float)v; inst.push(ev);
        if (ms > 0) QTimer::singleShot(ms, this, [this] { Event z; z.type = EV_BEND; z.f = 0; inst.push(z); });
        r["value"] = v;
    } else { // sustain
        const bool on = A.boolean("on", true);
        if (!e.isEmpty()) return {};
        inst.cmd(EV_SUSTAIN, on ? 1 : 0);
        r["on"] = on;
    }
    return r;
}

QJsonObject ControlServer::cmdLoop(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 12, true);
    if (!e.isEmpty()) return {};
    const int st = inst.uiState().loopState;
    if (act == "record") {
        if (st == L_COUNTIN || st == L_REC) { e = "already recording"; return {}; }
        inst.cmd(EV_LOOP, LC_PEDAL);
    } else if (act == "play") {
        if (st == L_EMPTY) { e = "nothing recorded"; return {}; }
        if (st == L_STOPPED) inst.cmd(EV_LOOP, LC_PLAYSTOP);
        else if (st == L_REC || st == L_COUNTIN) inst.cmd(EV_LOOP, LC_PEDAL); // closes the recording and plays
    } else if (act == "stop") {
        if (st == L_PLAY || st == L_OVERDUB) inst.cmd(EV_LOOP, LC_PLAYSTOP);
        else { e = "the loop is not playing"; return {}; }
    } else if (act == "clear") inst.cmd(EV_LOOP, LC_CLEAR);
    else if (act == "undo") inst.cmd(EV_LOOP, LC_UNDO);
    else { e = "action must be record, play, stop, clear or undo"; return {}; }
    QJsonObject r; r["action"] = act;
    return r;
}

QJsonObject ControlServer::cmdBacking(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 12);
    const bool seventh = A.has("seventh") ? A.boolean("seventh", false) : inst.uiState().set.seventh;
    const int beats = A.integer("beats", 1, 8, 0);
    const QString style = A.str("style", "", 12);
    if (!e.isEmpty()) return {};
    std::vector<int> chords;
    if (a.contains("chords")) {
        if (!a["chords"].isArray() || a["chords"].toArray().isEmpty() || a["chords"].toArray().size() > kProgMax) {
            e = "'chords' must be a list of 1 to 8 chords (1 to 7 or I to VII)"; return {};
        }
        for (const QJsonValue &v : a["chords"].toArray()) {
            const int d = parseDegree(v);
            if (d < 0) { e = "a chord must be 1 to 7 or a numeral I to VII"; return {}; }
            chords.push_back(d);
        }
    } else if (act.isEmpty() && !a.contains("seventh") && !a.contains("beats") && style.isEmpty()) {
        e = "give 'chords', 'action' (play, stop, clear, remove_last), 'style', 'seventh' or 'beats'"; return {};
    }
    int styleIdx = -1;
    if (!style.isEmpty()) {
        const QString s = style.toLower().remove(' ').remove('-');
        static const char *N[] = {"sustain", "pulse", "arpup", "arpdown", "arpupdn"};
        for (int i = 0; i < BS_COUNT; ++i) if (s == N[i]) styleIdx = i;
        if (styleIdx < 0) { e = "style must be sustain, pulse, arp up, arp down or arp up-dn"; return {}; }
    }
    if (!act.isEmpty() && act != "play" && act != "stop" && act != "clear" && act != "remove_last") { e = "action must be play, stop, clear or remove_last"; return {}; }
    const UiState u = inst.uiState();
    if (a.contains("seventh")) inst.set(S_SEVENTH, seventh ? 1.0f : 0.0f);
    if (beats > 0) inst.set(S_CHORDBEATS, (float)beats);
    if (styleIdx >= 0) inst.set(S_BACKSTYLE, (float)styleIdx);
    if (!chords.empty()) {
        inst.cmd(EV_BACK, BC_CLEAR);
        inst.set(S_BACKMODE, (float)BM_PROG);
        for (int d : chords) inst.cmd(EV_CHORD, d);
    }
    if (act == "clear") inst.cmd(EV_BACK, BC_CLEAR);
    else if (act == "remove_last") inst.cmd(EV_BACK, BC_REMOVE_LAST);
    else if (act == "play" && !u.backPlaying) inst.cmd(EV_BACK, BC_PLAYSTOP);
    else if (act == "stop" && u.backPlaying) inst.cmd(EV_BACK, BC_PLAYSTOP);
    QJsonObject r; r["chords"] = (int)chords.size();
    return r;
}

QJsonObject ControlServer::cmdBeat(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 8);
    if (!e.isEmpty()) return {};
    if (!a.contains("action") && !a.contains("kit") && !a.contains("set_step") && !a.contains("hit")) {
        e = "give 'action' (play, stop, clear), 'kit', 'set_step' or 'hit'"; return {};
    }
    if (!act.isEmpty() && act != "play" && act != "stop" && act != "clear") { e = "action must be play, stop or clear"; return {}; }
    int kit = -1;
    if (a.contains("kit")) {
        const QJsonValue v = a["kit"];
        if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() < kKits && v.toDouble() == std::floor(v.toDouble())) kit = (int)v.toDouble();
        else if (v.isString()) for (int i = 0; i < kKits; ++i) if (v.toString().compare(kitName(i), Qt::CaseInsensitive) == 0) kit = i;
        if (kit < 0) { e = "kit must be soft, crunchy or glitchy"; return {}; }
    }
    struct Step { int drum, step; bool on; };
    std::vector<Step> steps;
    if (a.contains("set_step")) {
        QJsonArray arr = a["set_step"].isArray() ? a["set_step"].toArray() : QJsonArray{a["set_step"]};
        if (arr.isEmpty() || arr.size() > kDrums * kDrumSteps) { e = "'set_step' must hold 1 to 128 steps"; return {}; }
        for (const QJsonValue &v : arr) {
            if (!v.isObject()) { e = "every set_step must be an object {drum, step, on}"; return {}; }
            Args B{v.toObject(), e};
            Step s;
            s.drum = parseDrum(v.toObject()["drum"]);
            if (s.drum < 0) { e = "'drum' must be 0 to 7 or a name (kick, snare, hat, openhat, clap, rim, tom, cowbell)"; return {}; }
            s.step = B.integer("step", 0, kDrumSteps - 1, 0, true);
            s.on = B.boolean("on", true);
            if (!e.isEmpty()) return {};
            steps.push_back(s);
        }
    }
    int hit = -1;
    if (a.contains("hit")) {
        const QJsonValue v = a["hit"].isObject() ? a["hit"].toObject()["drum"] : a["hit"];
        hit = parseDrum(v);
        if (hit < 0) { e = "'hit' needs a drum: 0 to 7 or a name"; return {}; }
    }
    const UiState u = inst.uiState();
    if (kit >= 0) inst.set(S_KIT, (float)kit);
    for (const Step &s : steps) inst.beatDo(BT_STEP, s.drum, s.step, s.on ? 1 : 0);
    if (act == "clear") inst.beatDo(BT_ERASE, -1);
    else if (act == "play" && !u.beatPlaying) inst.beatDo(BT_PLAYSTOP);
    else if (act == "stop" && u.beatPlaying) inst.beatDo(BT_PLAYSTOP);
    if (hit >= 0) inst.hit(hit);
    QJsonObject r; r["steps_set"] = (int)steps.size();
    return r;
}

QJsonObject ControlServer::cmdTrack(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 8, true);
    if (!e.isEmpty()) return {};
    QJsonObject r;
    bool any = false;
    for (int ln = 0; ln < kLanes; ++ln) any = any || !w.track.lane[ln].empty();
    if (act == "keep") {
        w.trackKeep();
        r["clips"] = (int)w.track.clips.size();
    } else if (act == "clear") {
        inst.trackDo(TC_STOP);
        w.trackApply([](TrackData &t) { const bool had = !t.clips.empty(); t = TrackData(); return had; });
        w.handOn = false;
        r["clips"] = 0;
    } else if (act == "place") {
        const int clip = A.integer("clip", 0, 1000, 0, true);
        const int bar = A.integer("bar", 0, kTrackMaxBars - 1, 0, true);
        const int lane = A.integer("lane", 0, kLanes - 1, -1);
        if (!e.isEmpty()) return {};
        if (clip >= (int)w.track.clips.size()) { e = "no such clip: keep first (see state.track.clips)"; return {}; }
        w.handOn = false;
        w.trackTake(clip);
        const int l = lane >= 0 ? lane : laneForKind(w.track.clips[clip]->kind);
        const bool ok = w.trackPlace(l, bar);
        w.handOn = false;
        if (!ok) { e = "no room there"; return {}; }
        r["placed"] = true;
    } else if (act == "play") {
        if (!any) { e = "nothing on the track"; return {}; }
        if (!inst.uiState().trackPlaying) w.trackPlayStop();
    } else if (act == "stop") {
        if (inst.uiState().trackPlaying) w.trackPlayStop();
    } else if (act == "export") {
        if (!any) { e = "nothing on the track to export"; return {}; }
        if (w.exportStatus() == 1) { e = "an export is already running"; return {}; }
        w.trackExport();
        r["path"] = w.exportFile; r["async"] = true; // the file is complete when state.track.exporting is false
    } else {
        e = "action must be keep, place, play, stop, export or clear";
        return {};
    }
    return r;
}

QJsonObject ControlServer::cmdPage(const QJsonObject &a, QString &e) {
    Args A{a, e};
    QString n = A.str("name", "", 12, true).toLower();
    if (!e.isEmpty()) return {};
    if (n == "play") n = "synth";
    int p = -1;
    for (int i = 0; i < MainWindow::PG_COUNT; ++i) if (n == pageNames[i]) p = i;
    if (p < 0) { e = "page must be one of track, synth, sound, beat, backing, tune, system, help, chat"; return {}; }
    w.tour().close(); // the real page is wanted: the tour steps aside
    if (p == MainWindow::PG_CHAT && w.pg != MainWindow::PG_CHAT) w.chatReturn = w.pg;
    w.goPage(p);
    QJsonObject r; r["page"] = n;
    return r;
}

// tour: open the interactive tour on a page and a control (so the copilot can say "let me show you"), or close it.
QJsonObject ControlServer::cmdTour(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "open", 8);
    const QString page = A.str("page", "", 24);
    const QString element = A.str("element", "", 64);
    const bool guided = A.boolean("guided", false);
    if (!e.isEmpty()) return {};
    if (act != "open" && act != "close") { e = "action must be open or close"; return {}; }
    if (act == "close") { w.tour().close(); return w.tour().stateJson(); }
    QString err;
    if (!w.tour().open(page, element, guided, &err, true)) {
        e = err + " (use track, track-chop, track-sound, track-mixer, synth, sound, sound-deep, beat, backing, tune, system, key-test, help or chat)";
        return {};
    }
    return w.tour().stateJson();
}

// highlight, unhighlight, zones: pointing at the screen. See docs/CONTROL.md.
QJsonObject ControlServer::cmdGuide(const QString &cmd, const QJsonObject &a, QString &e) {
    Args A{a, e};
    QJsonObject r;
    if (cmd == "unhighlight") {
        const int n = w.highlightedZones().size();
        w.clearHighlight();
        r["cleared"] = n;
        return r;
    }
    // optional page (and, for Help, the sub page: playing, commands, beat, track)
    int page = w.pg, sub = -1;
    if (A.has("page")) {
        const QString n = A.str("page", "", 12);
        if (e.isEmpty()) { page = MainWindow::pageFromName(n); if (page < 0) e = "page must be one of track, synth, sound, beat, backing, tune, system, help, chat"; }
    }
    if (A.has("sub")) {
        const QJsonValue v = a["sub"];
        static const char *subs[] = {"playing", "commands", "beat", "track"};
        if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() <= 3 && v.toDouble() == std::floor(v.toDouble())) sub = (int)v.toDouble();
        else if (v.isString()) { for (int i = 0; i < 4; ++i) if (v.toString().toLower() == subs[i]) sub = i; }
        if (sub < 0 && e.isEmpty()) e = "'sub' must be playing, commands, beat or track (or 0-3)";
    }
    if (cmd == "zones") {
        if (!e.isEmpty()) return {};
        QJsonArray zs;
        const QVector<MainWindow::ZoneInfo> cat = w.zoneCatalogue(page, page == MainWindow::PG_HELP ? (sub >= 0 ? sub : w.helpPage) : -1);
        for (const MainWindow::ZoneInfo &z : cat) {
            QJsonObject o;
            o["name"] = z.name; o["label"] = z.label;
            o["x"] = qRound(z.r.x()); o["y"] = qRound(z.r.y()); o["w"] = qRound(z.r.width()); o["h"] = qRound(z.r.height());
            if (z.covered) o["covered_by_dock"] = true;
            zs.append(o);
        }
        r["page"] = pageNames[page];
        if (page == MainWindow::PG_HELP) { static const char *subs[] = {"playing", "commands", "beat", "track"}; r["sub"] = subs[sub >= 0 ? sub : w.helpPage]; }
        r["zones"] = zs;
        r["dock_open"] = w.dockIsOpen() && page != MainWindow::PG_CHAT;
        return r;
    }
    // highlight
    QStringList names;
    if (!a.contains("zones") || !a["zones"].isArray()) e = a.contains("zones") ? "'zones' must be an array of zone names" : "missing 'zones'";
    else {
        const QJsonArray za = a["zones"].toArray();
        if (za.isEmpty() || za.size() > 16) e = "'zones' must hold 1 to 16 names";
        for (const QJsonValue &v : za) {
            if (!v.isString()) { e = "'zones' must be an array of strings"; break; }
            if (v.toString().size() > 48) { e = "a zone name is longer than 48 characters"; break; }
            names << v.toString();
        }
    }
    QString label = A.str("label", "", 40);
    for (QChar &c : label) if (c.unicode() < 0x20 || c.unicode() == 0x7f) c = ' ';
    label = label.trimmed();
    const int ms = A.integer("ms", 100, 30000, 8000);
    if (!e.isEmpty()) return {};
    w.tour().close(); // a highlight is drawn on the real page
    if (page != w.pg || (page == MainWindow::PG_HELP && sub >= 0 && sub != w.helpPage)) { // switch first, atomically with the highlight
        if (page == MainWindow::PG_CHAT && w.pg != MainWindow::PG_CHAT) w.chatReturn = w.pg;
        w.goPage(page);
        if (page == MainWindow::PG_HELP && sub >= 0) w.helpPage = sub;
    }
    w.refreshZones();
    QStringList unknown;
    const QStringList known = w.setHighlight(names, label, ms, unknown);
    w.refreshZones();   // the dock may have moved out of the way
    r["page"] = pageNames[w.pg];
    r["highlighted"] = QJsonArray::fromStringList(known);
    r["unknown"] = QJsonArray::fromStringList(unknown);
    r["ms"] = ms;
    r["caption"] = !w.highlightCaptionRect().isNull();
    QJsonObject dock;
    dock["open"] = w.dockIsOpen() && w.pg != MainWindow::PG_CHAT;
    if (!w.dockRect().isNull()) { const QRectF d = w.dockRect(); dock["y"] = qRound(d.y()); dock["h"] = qRound(d.height()); dock["position"] = d.y() < 100 ? "top" : "bottom"; }
    r["dock"] = dock;
    return r;
}

QJsonObject ControlServer::cmdSession(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "", 6, true);
    const QString name = A.str("name", "", 40, true);
    if (e.isEmpty() && !QRegularExpression("^[A-Za-z0-9_-]{1,40}$").match(name).hasMatch()) e = "'name' may hold letters, digits, - and _";
    if (e.isEmpty() && act != "save" && act != "load") e = "action must be save or load";
    if (!e.isEmpty()) return {};
    store::ensureDirs();
    const QString path = store::baseDir() + "/sessions/" + name + ".json";
    if (act == "save") {
        if (!w.saveSession(path)) { e = "could not write the session"; return {}; }
    } else {
        if (!QFile::exists(path)) { e = "no session called '" + name + "'"; return {}; }
        if (!w.loadSession(path)) { e = "that session file is damaged"; return {}; }
    }
    QJsonObject r; r["path"] = path;
    return r;
}

QJsonObject ControlServer::cmdScreenshot(QString &e) {
    const QString dir = QDir::tempPath();
    const QString path = QString("%1/vrequency-shot-%2.png").arg(dir).arg(QDateTime::currentMSecsSinceEpoch());
    if (!w.shot().save(path)) { e = "could not write " + path; return {}; }
    shots << path;
    while (shots.size() > 6) QFile::remove(shots.takeFirst()); // only the newest few are kept
    QJsonObject r; r["path"] = path; r["width"] = 720; r["height"] = 720;
    return r;
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
        r["seq"] = w.chatAdd(from, text, kind, false);
        w.chatSeen();
        return r;
    }
    const int since = A.integer("since", 0, 2000000000, 0);
    const int limit = A.integer("limit", 1, 200, 200);
    if (!e.isEmpty()) return {};
    w.chatSeen();
    r["messages"] = chatJson(since, limit);
    r["last_seq"] = w.chatSeq;
    return r;
}
