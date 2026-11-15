// venv-vrequency self-test — what its files share: the check counter, the hands that press keys and
// touch the screen, a client for the control socket, and the state one part of a group hands to the next.
#pragma once
#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <functional>

class AudioOut;
class CardDb;
class ControlServer;
class MainWindow;
class TourView;
namespace td { class Instrument; }

namespace selftest {
// ---- the checks ----
void check(bool ok, const QString &what);   // prints "  ok    what" or "  FAIL  what", and counts it
int passed();
int failed();

// ---- time, keys and touches ----
void wait(int ms);                                                // the app's events keep running meanwhile
void key(MainWindow &w, int k, bool down, bool repeat = false);
void press(MainWindow &w, int k);                                 // down, then up
void ctrl(MainWindow &w, int k);                                  // the same with Ctrl held
// A press and release in one place. Each kind of tap keeps the finger number it has always used.
void tapAt(MainWindow &w, QPointF at, int finger);
void tap(MainWindow &w, const QString &zone);                     // the middle of a zone (finger 7)
void tourTapAt(MainWindow &w, QPointF at);                        // the tour checks' own finger (9)
void tourTap(MainWindow &w, const QString &zone);

// ---- what every group of checks is handed ----
struct Context {
    MainWindow &w;
    td::Instrument &inst;
    AudioOut &audio;
    const QString &shotsDir;   // where --shots wants its pictures, or empty
};

// ---- the control socket: a real client in this process, talking over the real socket ----
struct Client {
    QLocalSocket s;
    QByteArray buf;
    bool open(const QString &path);
    QByteArray line(int ms);                                // the next line the server sent, or empty after `ms`
    void send(const QByteArray &raw);
    QJsonObject call(const QByteArray &raw, int ms = 3000); // a request, and its reply
};
// A server started for one group of checks, and the clients connected to it.
struct Link {
    ControlServer &server;
    Client &c, &c2;
    QString path;
};
bool okOf(const QJsonObject &reply);
QJsonObject result(const QJsonObject &reply);
QStringList strs(const QJsonArray &a);
QJsonObject helpFocus(Client &c);       // state.help_focus, asked for over the socket
float scopeHz(td::Instrument &inst);    // zero crossings of the output's tail: the pitch of a plain sine

// ---- the tour ----
// What the tour checks share. They report through the `check` runTourTests was given.
struct TourContext {
    MainWindow &w;
    td::Instrument &inst;
    AudioOut &audio;
    const std::function<void(bool, const QString &)> &check;
    TourView &t;
    ControlServer &ctl;
    const CardDb &db;
    QJsonObject call(const QByteArray &json) const;   // a request handled directly, without a socket
    QJsonObject tourState() const;                    // state.tour
};
void settleTour(MainWindow &w);   // the tour opens its pictures a moment after it is asked to: let that finish
} // namespace selftest
