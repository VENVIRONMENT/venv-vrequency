// venv-vrequency self-test — the shared helpers (see harness.h).
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QThread>
#include <cstdio>

using namespace td;

namespace selftest {
namespace {
int fails = 0, passes = 0;
} // namespace

void check(bool ok, const QString &what) {
    if (ok) ++passes; else ++fails;
    std::printf("  %s  %s\n", ok ? "ok  " : "FAIL", what.toUtf8().constData());
    std::fflush(stdout);
}
int passed() { return passes; }
int failed() { return fails; }

void wait(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) { QCoreApplication::processEvents(QEventLoop::AllEvents, 5); QThread::msleep(2); }
}
void key(MainWindow &w, int k, bool down, bool repeat) {
    QKeyEvent e(down ? QEvent::KeyPress : QEvent::KeyRelease, k, Qt::NoModifier, QString(), repeat);
    QCoreApplication::sendEvent(&w, &e);
}
void press(MainWindow &w, int k) { key(w, k, true); key(w, k, false); }
void ctrl(MainWindow &w, int k) { key(w, Qt::Key_Control, true); press(w, k); key(w, Qt::Key_Control, false); }
void tapAt(MainWindow &w, QPointF at, int finger) {
    w.pointer(finger, at, PH_PRESS);
    w.pointer(finger, at, PH_RELEASE);
}
void tap(MainWindow &w, const QString &zone) { tapAt(w, w.zoneRect(zone).center(), 7); }
void tourTapAt(MainWindow &w, QPointF at) { tapAt(w, at, 9); }
void tourTap(MainWindow &w, const QString &zone) { tourTapAt(w, w.zoneRect(zone).center()); }

bool Client::open(const QString &path) { s.connectToServer(path); return s.waitForConnected(1000); }
QByteArray Client::line(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) {
        const int nl = buf.indexOf('\n');
        if (nl >= 0) { QByteArray l = buf.left(nl); buf.remove(0, nl + 1); return l; }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        buf += s.readAll();
        QThread::msleep(1);
    }
    return QByteArray();
}
void Client::send(const QByteArray &raw) { s.write(raw + "\n"); s.flush(); }
QJsonObject Client::call(const QByteArray &raw, int ms) {
    send(raw);
    const QByteArray l = line(ms);
    return QJsonDocument::fromJson(l).object();
}

bool okOf(const QJsonObject &reply) { return reply["ok"].toBool(); }
QJsonObject result(const QJsonObject &reply) { return reply["result"].toObject(); }
QStringList strs(const QJsonArray &a) { QStringList l; for (const QJsonValue &v : a) l << v.toString(); return l; }
QJsonObject helpFocus(Client &c) { return result(c.call(R"({"cmd":"state"})"))["help_focus"].toObject(); }
float scopeHz(Instrument &inst) {
    static float x[8192];
    inst.readScope(x, 8192);
    int first = -1, last = -1, n = 0;
    for (int i = 1; i < 8192; ++i)
        if (x[i - 1] < 0 && x[i] >= 0) { if (first < 0) first = i; last = i; ++n; }
    return n < 3 ? 0.0f : (n - 1) * 48000.0f / (last - first);
}

QJsonObject TourContext::call(const QByteArray &json) const { return ctl.handle(QJsonDocument::fromJson(json).object()); }
QJsonObject TourContext::tourState() const { return result(call(R"({"cmd":"state"})"))["tour"].toObject(); }
void settleTour(MainWindow &w) {
    QElapsedTimer t; t.start();
    while (w.tour().isBuilding() && t.elapsed() < 30000) wait(20);
    w.shot();
}
} // namespace selftest
