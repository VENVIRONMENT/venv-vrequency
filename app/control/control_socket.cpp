// venv-vrequency — the control socket: start, stop, connections and reading request lines.
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalSocket>

using namespace td;
using namespace ctl;

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
    w.chat.waiting(0);
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

