// venv-vrequency self-test — tour: Ask Rook hands the card's question to the chat dock.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QJsonArray>
#include <QJsonObject>

namespace selftest {
// Ask Rook
void tourAskRook(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const CardDb &db = ctx.db; const auto &check = ctx.check;
    w.guide.dockClose(); w.setPage(PG_PLAY); w.shot();
    t.open("synth", zn::octave_up); settleTour(w);
    const HelpCard *oct = db.find("synth", zn::octave_up);
    press(w, Qt::Key_Return);
    check(!t.isOpen() && w.guide.dockIsOpen(), "Enter (Ask Rook) leaves the tour and opens the chat dock");
    check(oct && w.chat.draft == oct->ask, "the dock's question is the card's own: " + w.chat.draft.left(50));
    check(w.page() == PG_PLAY && w.guide.dockTopic().toLower().contains("octave"), "the dock names what is being asked about");
    const int seq0 = w.chat.seq;
    press(w, Qt::Key_Return); // the dock has the keyboard: Enter sends
    check(w.chat.seq == seq0 + 1, "Enter in the dock sends it");
    const ChatMsg &m = w.chat.log.back();
    check(m.user && m.via == "tour" && m.focus == QStringList{zn::octave_up} && m.page == "synth",
          QString("the message carries via \"tour\", focus [octave_up] and the page (%1 %2)").arg(m.via, m.focus.join(",")));
    check(w.guide.lastQuestionZone() == zn::octave_up, "and the question's focus is that zone");
    const QJsonObject poll = result(ctx.call(R"({"cmd":"chat_poll","since":0,"limit":200})"));
    bool carried = false;
    for (const QJsonValue &v : poll["messages"].toArray()) { const QJsonObject o = v.toObject(); if (o["via"].toString() == "tour" && o["focus"].toArray().size() == 1) carried = true; }
    check(carried, "chat_poll returns via and focus to the copilot");
    w.guide.dockClose();
    // from the Ask button by touch, on a different page, and for a concept
    t.open("beat", zn::beatrec); settleTour(w);
    tourTap(w, zn::tour_ask);
    check(!t.isOpen() && w.page() == PG_BEAT && w.guide.dockIsOpen(), "the Ask Rook button works by touch and puts the player on the page the control lives on");
    w.guide.dockClose(); w.setPage(PG_PLAY); w.shot();
    t.open("synth", "concepts.recall"); settleTour(w);
    press(w, Qt::Key_Return);
    check(w.guide.dockIsOpen() && w.chat.draft.size() > 5, "a concept card can be asked about too");
    press(w, Qt::Key_Return);
    check(w.chat.log.back().focus == QStringList{"concepts.recall"} && w.chat.log.back().via == "tour", "with the concept as its focus");
    w.guide.dockClose();
    // a plain dock question afterwards is not marked as from the tour
    w.guide.dockOpenFocus();
    for (QChar ch : QString("hello")) press(w, ch.toUpper().unicode());
    press(w, Qt::Key_Return);
    check(w.chat.log.back().via == "dock", "an ordinary dock message afterwards is still via \"dock\"");
    w.guide.dockClose(); w.setPage(PG_PLAY);
}
} // namespace selftest
