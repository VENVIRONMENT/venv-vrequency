// venv-vrequency self-test — guided help: the chat dock and the ticker that opens it.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace td;

namespace selftest {
// the dock: opening, focus, Esc, close
void guideDock(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; Client &c = link.c, &c2 = link.c2;
    w.setPage(PG_PLAY); w.shot();
    check(!w.guide.dockIsOpen() && !w.guide.dockHasFocus(), "the dock starts closed");
    ctrl(w, Qt::Key_Y); w.shot();
    check(w.guide.dockIsOpen() && w.guide.dockHasFocus() && w.page() == PG_PLAY, "Ctrl+Y opens the dock with the keyboard, without leaving the page");
    const QRectF d = w.guide.dockRect();
    check(d.height() >= 120 && d.height() <= 150 && d.bottom() > 700 && w.zoneNames().contains(zn::dock_input) && w.zoneNames().contains(zn::dock_send), QString("the dock is %1 px tall at the bottom, with an input and Send").arg(d.height()));
    const QJsonObject st = helpFocus(c);
    check(st["dock_open"].toBool() && st["dock_focus"].toBool(), "state.help_focus reports the dock open and focused");
    // letters type, they do not play
    press(w, Qt::Key_Z); press(w, Qt::Key_Q); press(w, Qt::Key_A); wait(120);
    const UiState us = inst.uiState();
    check(w.chat.draft == "zqa" && !us.str[0].gate && !us.str[1].gate && !us.str[2].gate, "letters type into the dock and do not play the strings");
    // Enter sends with the same numbering, and wakes a waiting agent
    const int seq0 = w.chat.seq;
    c2.send(QByteArray(R"({"id":"w","cmd":"chat_wait","since":)") + QByteArray::number(seq0) + R"(,"timeout_ms":3000})");
    wait(50);
    press(w, Qt::Key_Return); wait(50);
    QJsonObject wr = QJsonDocument::fromJson(c2.line(1500)).object();
    const QJsonArray wm = result(wr)["messages"].toArray();
    check(okOf(wr) && !result(wr)["timed_out"].toBool(true) && wm.size() == 1 && wm[0].toObject()["text"].toString() == "zqa" && wm[0].toObject()["seq"].toInt() == seq0 + 1,
          "Enter in the dock sends through the same path: the next seq, and a waiting chat_wait wakes");
    check(wm[0].toObject()["page"].toString() == "synth" && wm[0].toObject().contains("focus") && wm[0].toObject()["via"].toString() == "dock", "the message carries page and focus metadata");
    check(w.chat.draft.isEmpty() && w.guide.dockHasFocus(), "the input clears and keeps the keyboard");
    // Esc: the keys play again, the dock stays
    press(w, Qt::Key_Escape);
    check(w.guide.dockIsOpen() && !w.guide.dockHasFocus() && w.page() == PG_PLAY, "Esc moves the keyboard back to the page and keeps the dock");
    key(w, Qt::Key_Z, true); wait(120);
    check(inst.uiState().str[0].gate && w.chat.draft.isEmpty(), "and the keys play again");
    key(w, Qt::Key_Z, false); wait(100);
    press(w, Qt::Key_Escape); w.shot();
    check(!w.guide.dockIsOpen() && w.page() == PG_PLAY, "Esc again closes the dock");
    press(w, Qt::Key_Escape);
    check(w.page() == PG_PLAY && !w.guide.dockIsOpen(), "and Esc after that is the old Esc (back to Synth)");
    // Ctrl+Y toggles focus while the dock is open
    ctrl(w, Qt::Key_Y); ctrl(w, Qt::Key_Y);
    check(w.guide.dockIsOpen() && !w.guide.dockHasFocus(), "Ctrl+Y on the dock gives the keyboard back to the page");
    ctrl(w, Qt::Key_Y);
    check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "and again takes it");
    // touch
    w.shot(); tap(w, zn::dock_close);
    check(!w.guide.dockIsOpen(), "Hide closes the dock");
    ctrl(w, Qt::Key_Y); press(w, Qt::Key_Escape); w.shot();
    tap(w, zn::dock_input);
    check(w.guide.dockHasFocus(), "touching the input line gives it the keyboard");
    press(w, Qt::Key_H); press(w, Qt::Key_I); w.shot();
    tap(w, zn::dock_send); wait(40);
    check(w.chat.log.back().text == "hi" && w.chat.log.back().user, "touching Send sends");
    // the page behind stays usable above the dock; the dock swallows touches on itself
    const int oct0 = inst.uiState().set.tuning.octave;
    tap(w, zn::dock_close); w.shot();
    tap(w, zn::octave_up); wait(80);
    check(inst.uiState().set.tuning.octave == oct0 + 1, "with the dock closed the page's own buttons work");
    tap(w, zn::octave_down); wait(60);
    // Ctrl+Shift+Y is the full page, Ctrl+Y on it goes back
    key(w, Qt::Key_Control, true); key(w, Qt::Key_Shift, true); press(w, Qt::Key_Y); key(w, Qt::Key_Shift, false); key(w, Qt::Key_Control, false);
    check(w.page() == PG_CHAT && w.guide.dockRect().isNull(), "Ctrl+Shift+Y opens the full Chat page, where there is no dock");
    press(w, Qt::Key_A);
    check(w.chat.draft == "a", "and letters still type on the Chat page");
    press(w, Qt::Key_Backspace);
    ctrl(w, Qt::Key_Y);
    check(w.page() == PG_PLAY, "Ctrl+Y on the Chat page goes back");
    // the Full button
    ctrl(w, Qt::Key_Y); w.shot(); tap(w, zn::dock_full);
    check(w.page() == PG_CHAT, "the dock's Full button opens the Chat page");
    press(w, Qt::Key_Escape);
    w.guide.dockClose();
}

// the ticker opens the dock
void guideTicker(Context &ctx) {
    MainWindow &w = ctx.w;
    w.setPage(PG_PLAY); w.guide.dockClose(); w.shot();
    w.chat.add("rook", "A new message while you play.", "say", false);
    w.shot();
    check(w.chat.tickerShowing() && w.zoneNames().contains(zn::ticker), "a new agent message shows the ticker with a touch target");
    tap(w, zn::ticker); w.shot();
    check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "touching the ticker opens the dock with the keyboard");
    const int unread = w.chat.unread;
    w.chat.add("rook", "And another one.", "say", false);
    check(w.chat.unread == unread && !w.chat.tickerShowing(), "with the dock open there is no ticker and nothing counts as unread");
    w.guide.dockClose();
    { // the ticker does not cover a highlight: it moves to the top
        QStringList unk; w.refreshZones(); w.guide.setHighlight({zn::loop_pedal}, QString(), 20000, unk);
        w.chat.add("rook", "Look at the loop pedal.", "say", false); w.shot();
        check(w.zoneRect(zn::ticker).top() < 100 && !w.zoneRect(zn::ticker).intersects(w.zoneRect(zn::loop_pedal)), "when a highlight is at the bottom the ticker moves to the top");
        w.guide.clearHighlight(); w.shot();
        check(w.zoneRect(zn::ticker).top() > 600, "and is back at the bottom without one");
    }
    w.chat.add("rook", "Ticker again.", "say", false); w.shot();
    ctrl(w, Qt::Key_Y);
    check(w.guide.dockIsOpen() && w.guide.dockHasFocus(), "Ctrl+Y while the ticker shows also opens the dock");
    w.guide.dockClose();
}
} // namespace selftest
