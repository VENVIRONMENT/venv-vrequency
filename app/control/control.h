// venv-vrequency — the control socket. A local Unix socket (never the network) over which a
// copilot, an AI agent such as Claude Code, can play and edit the app and chat with the player.
// Newline-delimited JSON, one request per line:
//   {"id":1,"cmd":"note","string":1,"degree":2,"dur_ms":400}
//   {"id":1,"ok":true,"result":{...}}   or   {"id":1,"ok":false,"error":"..."}
// Everything runs on the GUI thread and goes to the audio thread through the same lock-free
// queue the keyboard uses, so the engine's own limits (and the output limiter) still apply.
// The commands are listed in docs/CONTROL.md.
#pragma once
#include <QHash>
#include <QJsonArray>
#include <QStringList>
#include <QJsonObject>
#include <QLocalServer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <vector>

class QLocalSocket;
class MainWindow;
namespace td { class Instrument; }

class ControlServer : public QObject {
    Q_OBJECT
public:
    ControlServer(MainWindow &w, td::Instrument &inst);
    ~ControlServer() override;

    // Where the socket goes: $VREQUENCY_CONTROL_SOCKET, else $XDG_RUNTIME_DIR/vrequency.sock,
    // else $VREQUENCY_HOME/control.sock, else ~/vrequency/control.sock.
    static QString defaultPath();
    // A stale socket file from a crashed run is replaced; one that something answers on is not.
    bool start(const QString &path = QString());
    void stop();
    bool running() const { return server.isListening(); }
    QString path() const { return sockPath; }
    int clients() const { return (int)conns.size(); }
    QString lastError() const { return err; }

    // Run one request without a socket (the self-test uses the socket; this serves hostile-input checks).
    QJsonObject handle(const QJsonObject &req);

private:
    struct Conn {
        QPointer<QLocalSocket> sock;
        QByteArray buf;
        bool discarding = false;
    };
    struct Waiter {
        QPointer<QLocalSocket> sock;
        QJsonValue id;
        int since = 0;
        QTimer *timer = nullptr;
    };

    MainWindow &w;
    td::Instrument &inst;
    QLocalServer server;
    QString sockPath, err;
    std::vector<Conn *> conns;
    std::vector<Waiter> waiters;
    int nextNoteId = 0x400;          // synthetic note ids cycle in 0x400..0x7ff, clear of the keyboard's
    QHash<int, int> liveNotes;       // note id -> string, for notes this socket started
    QStringList shots;               // screenshots written, oldest first

    void onConnect();
    void onData(Conn *c);
    void drop(Conn *c);
    void line(Conn *c, const QByteArray &ln);
    void reply(QLocalSocket *s, const QJsonValue &id, bool ok, const QJsonObject &result, const QString &error);
    void onUserChat();
    void finishWaiter(size_t i, bool timedOut);
    void updateWaiters();

    // commands: return the result, or set `e` to refuse
    QJsonObject dispatch(const QString &cmd, const QJsonObject &a, QString &e);
    QJsonObject cmdState();
    QJsonObject cmdNote(const QJsonObject &a, QString &e, bool chord);
    QJsonObject cmdSlide(const QJsonObject &a, QString &e);
    QJsonObject cmdRelease(const QJsonObject &a, QString &e);
    QJsonObject cmdParams(const QJsonObject &a, QString &e, bool set);
    QJsonObject cmdSound(const QJsonObject &a, QString &e);
    QJsonObject cmdFavourite(const QJsonObject &a, QString &e);
    QJsonObject cmdPlayControls(const QString &cmd, const QJsonObject &a, QString &e); // scale tempo octave bend sustain
    QJsonObject cmdLoop(const QJsonObject &a, QString &e);
    QJsonObject cmdPulse(const QJsonObject &a, QString &e);
    QJsonObject cmdPage(const QJsonObject &a, QString &e);
    QJsonObject cmdTour(const QJsonObject &a, QString &e);
    QJsonObject cmdGuide(const QString &cmd, const QJsonObject &a, QString &e); // highlight, unhighlight, zones
    QJsonObject cmdSession(const QJsonObject &a, QString &e);
    QJsonObject cmdScreenshot(QString &e);
    QJsonObject cmdChat(const QString &cmd, const QJsonObject &a, QString &e);

    bool notePitch(const QJsonObject &a, int string, const char *degKey, const char *midiKey, float &midi, QString &e) const;
    int startNote(int string, float midi, float vel, int durMs);
    void releaseNote(int id);
    QJsonArray chatJson(int since, int limit) const;
};
