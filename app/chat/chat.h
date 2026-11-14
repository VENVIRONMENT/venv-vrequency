// venv-vrequency — the conversation with the copilot (an AI agent that reaches the app through
// the control socket): the messages, the line being typed, and the ticker that shows a new agent
// message on any other page for a few seconds without taking the keyboard.
#pragma once
#include "app/shell/part.h"
#include <QString>
#include <QStringList>
#include <deque>
#include <functional>

class Guide;
class QJsonObject;

// One line of the copilot chat.
struct ChatMsg {
    int seq = 0;
    QString from, text, kind; // kind: say, tip, ask (agents) or say (the user)
    qint64 ts = 0;            // ms since the epoch
    bool user = false;
    // what the player was looking at when they sent it (user messages only)
    QString page;             // page name: "synth", "help"...
    QStringList focus;        // zone names being discussed (highlighted, or last tapped)
    QString via;              // "dock", "chat" or "tour"
};

// Break `s` into rows of at most `cols` characters, at spaces where possible.
QStringList chatWrap(const QString &s, int cols, int first = -1); // `first`: the width of the first row, when it differs

class Chat : public Part {
public:
    using Part::Part;
    void attach(Guide &g) { guide = &g; }    // the dock and the highlights the messages refer to
    std::function<void()> onUserMessage;     // the player sent something (a waiting control client hears it)

    std::deque<ChatMsg> log;
    int seq = 0, unread = 0, scroll = 0;
    int returnPage = PG_PLAY;                // where leaving the Chat page goes back to
    QString draft;                           // the line being typed

    void load(const QJsonObject &state);     // from the state file
    void save(QJsonObject &state) const;
    int add(const QString &from, const QString &text, const QString &kind, bool user,
            const QString &page = QString(), const QStringList &focus = QStringList(), const QString &via = QString()); // returns its seq
    void send();                             // the draft becomes a message from the player
    void toggle();                           // to the Chat page, or back from it
    bool key(int key, int plainAct, bool dock = false); // letters type; true when the key was used

    // ---- the copilot's presence ----
    void seen() { copilotSeenAt = sh.clock.elapsed(); sh.redraw(); }   // a control client polled or is waiting
    void waiting(int n) { copilotWaiters = n; sh.redraw(); }
    bool listening() const { return copilotWaiters > 0 || sh.clock.elapsed() - copilotSeenAt < 45000; }

    // ---- the ticker ----
    bool tickerShowing() const { return sh.clock.elapsed() < tickerUntil; }
    void hideTicker() { tickerUntil = 0; }
    qint64 tickerEnds() const { return tickerUntil; }
    void paintTicker();

private:
    Guide *guide = nullptr;
    qint64 copilotSeenAt = -100000, tickerUntil = 0;
    int copilotWaiters = 0;
    ChatMsg ticker;
};
