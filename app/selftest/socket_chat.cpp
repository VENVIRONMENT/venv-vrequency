// venv-vrequency self-test — control socket: the chat, from the copilot's side and from the player's.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QElapsedTimer>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace td;

namespace selftest {
// the chat
void socketChat(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; Client &c = link.c, &c2 = link.c2;
    w.setPage(PG_PLAY); wait(100);
    const int before = w.chat.seq;
    QJsonObject r = c.call(R"({"cmd":"chat_say","from":"rook","text":"Try a minor third up on the high string.","kind":"tip"})");
    wait(100);
    check(okOf(r) && w.chat.seq == before + 1 && w.chat.unread >= 1, "chat_say adds an agent message and counts it as unread");
    check(w.chat.tickerShowing(), "on another page the message shows in the ticker");
    const QImage withTicker = w.shot();
    w.setPage(PG_CHAT); wait(100);
    const QImage chatShot = w.shot();
    check(w.chat.unread == 0 && w.chat.log.back().text.startsWith("Try a minor third") && chatShot != withTicker, "the Chat page shows it and clears the unread count");
    r = c.call(R"({"cmd":"state"})");
    check(r["result"].toObject()["chat_tail"].toArray().last().toObject()["text"].toString().startsWith("Try a minor third"), "and state's chat tail holds it");
    r = c2.call(R"({"cmd":"chat_poll","since":0})");
    r = c.call(R"({"cmd":"state"})");
    check(r["result"].toObject()["chat"].toObject()["listening"].toBool(), "state says the copilot is listening after a poll");

    // a waiting copilot is woken by the player's message, typed on the keyboard
    const int seq0 = w.chat.seq;
    c2.send(QByteArray(R"({"id":"w1","cmd":"chat_wait","since":)") + QByteArray::number(seq0) + R"(,"timeout_ms":8000})");
    wait(100);
    check(c2.line(50).isEmpty(), "chat_wait holds its reply while nobody has typed");
    r = c.call(R"({"cmd":"state"})");
    check(okOf(r), "and other clients are answered meanwhile");
    const int voices0 = inst.uiState().voices;
    for (int k : {Qt::Key_H, Qt::Key_I, Qt::Key_Space, Qt::Key_O, Qt::Key_K, Qt::Key_Z}) press(w, k);
    check(w.chat.draft == "hi okz" && inst.uiState().voices == voices0 && inst.uiState().voices == 0, "on the Chat page letters type and no string plays");
    press(w, Qt::Key_Backspace);
    check(w.chat.draft == "hi ok" && inst.uiState().droppedEvents == 0, "Backspace edits the line instead of silencing");
    QElapsedTimer wt; wt.start();
    press(w, Qt::Key_Return);
    const QByteArray got = c2.line(2000);
    const QJsonObject wr = QJsonDocument::fromJson(got).object();
    const QJsonArray wm = wr["result"].toObject()["messages"].toArray();
    check(wr["id"].toString() == "w1" && wr["ok"].toBool() && !wm.isEmpty() && wm.last().toObject()["text"].toString() == "hi ok" &&
          wm.last().toObject()["user"].toBool() && wm.last().toObject()["seq"].toInt() == seq0 + 1 && !wr["result"].toObject()["timed_out"].toBool() && wt.elapsed() < 1500,
          QString("Enter sends it: the waiting chat_wait is answered at once (%1 ms) with the message").arg(wt.elapsed()));
    check(w.chat.draft.isEmpty(), "the input line empties after sending");
    r = c.call(QByteArray(R"({"cmd":"chat_poll","since":)") + QByteArray::number(seq0 - 1) + "}");
    check(r["result"].toObject()["messages"].toArray().size() == 2, "chat_poll returns messages from both directions");
    // touch: type, then tap Send
    for (int k : {Qt::Key_Y, Qt::Key_E, Qt::Key_S}) press(w, k);
    tap(w, zn::chat_send); wait(60);
    check(w.chat.log.back().text == "yes" && w.chat.log.back().user, "the Send button sends too");
    // timeout answer
    c2.send(QByteArray(R"({"id":"w2","cmd":"chat_wait","since":)") + QByteArray::number(w.chat.seq) + R"(,"timeout_ms":300})");
    const QJsonObject to = QJsonDocument::fromJson(c2.line(2000)).object();
    check(to["ok"].toBool() && to["result"].toObject()["messages"].toArray().isEmpty() && to["result"].toObject()["timed_out"].toBool(), "chat_wait times out with an empty list");
    // an already-waiting message answers immediately
    r = c2.call(QByteArray(R"({"cmd":"chat_wait","since":)") + QByteArray::number(seq0 - 1) + R"(,"timeout_ms":2000})", 500);
    check(okOf(r) && !r["result"].toObject()["messages"].toArray().isEmpty(), "chat_wait answers at once when a player message is already waiting");
    // the hotkey and Esc
    key(w, Qt::Key_Escape, true); key(w, Qt::Key_Escape, false);
    check(w.page() != PG_CHAT, "Esc leaves the Chat page");
    w.setPage(PG_PULSE);
    key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(w, Qt::Key_Y); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
    check(w.page() == PG_CHAT, "Ctrl+Shift+Y jumps to the Chat page");
    key(w, Qt::Key_Control, true); press(w, Qt::Key_Y); key(w, Qt::Key_Control, false);
    check(w.page() == PG_PULSE, "and Ctrl+Y again goes back to where you were");
    // history survives a restart
    w.saveState();
    QJsonObject sj; store::readJson(store::baseDir() + "/state.json", sj);
    bool kept = false;
    for (const QJsonValue &v : sj["chat"].toArray()) kept = kept || v.toObject()["text"].toString() == "hi ok";
    check(kept && sj["chatSeq"].toInt() == w.chat.seq, "the chat history is in the state file");
    // more than 200 messages: only the newest are kept
    for (int i = 0; i < 230; ++i) w.chat.add("rook", QString("filler %1").arg(i), "say", false);
    check((int)w.chat.log.size() == 200 && w.chat.log.back().text == "filler 229", "only the last 200 messages are kept");
    w.setPage(PG_CHAT);
    QImage full; for (int i = 0; i < 3; ++i) full = w.shot();
    check(full.width() == 720, "a full history draws");
    w.setPage(PG_PLAY);
}
} // namespace selftest
