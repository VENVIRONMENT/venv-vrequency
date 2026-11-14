// venv-vrequency self-test — guided help: the dock keeps clear of what is highlighted, and the Help page asks from the dock.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <QImage>
#include <QJsonArray>
#include <QJsonObject>

namespace selftest {
// the dock never covers what is highlighted; what is discussed is tagged; focus travels with the message
void guideDockAvoidsHighlight(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; const QString &shotsDir = ctx.shotsDir; Client &c = link.c;
    w.setPage(PG_PLAY); w.guide.dockClose(); w.shot();
    ctrl(w, Qt::Key_Y); press(w, Qt::Key_Escape);
    QJsonObject r = c.call(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":20000})");
    const QRectF d = w.guide.dockRect(), o1 = w.zoneRect(zn::octave_down), o2 = w.zoneRect(zn::octave_up);
    check(okOf(r) && w.guide.dockIsOpen() && !d.intersects(o1) && !d.intersects(o2) && d.top() < 100, "octave is at the bottom of Synth: the dock moves to the top rather than cover it");
    check(w.guide.dockTopic() == "OCTAVE" && result(r)["dock"].toObject()["position"].toString() == "top", "the dock is tagged with what is being discussed (OCTAVE)");
    QImage im = w.shot();
    if (!shotsDir.isEmpty()) im.save(shotsDir + "/16-dock-synth.png");
    r = c.call(R"({"cmd":"highlight","zones":["hdr_sound"],"label":"the sound name"})");
    check(okOf(r) && !w.guide.dockRect().intersects(w.zoneRect(zn::hdr_sound)) && w.guide.dockRect().top() > 500, "a highlight at the top leaves the dock at the bottom");
    r = c.call(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":20000})");
    ctrl(w, Qt::Key_Y); // focus the dock
    press(w, Qt::Key_W); press(w, Qt::Key_H); press(w, Qt::Key_Y); press(w, Qt::Key_Return); wait(40);
    const ChatMsg &m = w.chat.log.back();
    check(m.user && m.page == "synth" && m.focus == QStringList({zn::octave_down, zn::octave_up}) && m.via == "dock", "a question sent from the dock says it was asked on Synth with OCTAVE highlighted");
    check(helpFocus(c)["last_question_zone"].toString() == zn::octave_down, "state.help_focus.last_question_zone is the first focused zone");
    QJsonObject pr = c.call(QByteArray(R"({"cmd":"chat_poll","since":)") + QByteArray::number(m.seq - 1) + "}");
    const QJsonObject pm = result(pr)["messages"].toArray().last().toObject();
    check(pm["page"].toString() == "synth" && strs(pm["focus"].toArray()) == QStringList({zn::octave_down, zn::octave_up}), "chat_poll returns the page and focus with the player's message");
    // the topic carries to a message that comes with the highlight, and goes after one that does not
    w.chat.add("rook", "Octave shifts every string up or down twelve notes.", "say", false);
    check(w.guide.dockTopic() == "OCTAVE", "an agent message just after a highlight keeps the topic tag");
    wait(5300);
    w.chat.add("rook", "Anything else?", "say", false);
    check(w.guide.dockTopic().isEmpty(), "a later message with no highlight clears it");
    // page change closes the topic; the dock stays
    c.call(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE"})");
    w.setPage(PG_TUNE); w.shot();
    check(w.guide.dockIsOpen() && w.guide.dockTopic().isEmpty() && helpFocus(c)["highlighted"].toArray().isEmpty(), "changing page keeps the dock but drops the highlight and the tag");
    w.guide.dockClose();
}

// the dock never covers a highlighted zone, on any page
void guideDockAvoidsEveryZone(Context &ctx) {
    MainWindow &w = ctx.w;
    int tested = 0, clashes = 0; QStringList bad;
    for (int p = 0; p < PG_COUNT; ++p) {
        if (p == PG_CHAT) continue;
        w.setPage(p); w.guide.dockClose(); w.guide.clearHighlight(); w.shot();
        const QStringList names = w.zoneNames();
        for (const QString &n : names) {
            if (n.startsWith(zn::dock_prefix) || n == zn::ticker || n.startsWith(zn::tab_prefix)) continue;
            const QRectF zr = w.zoneRect(n);
            if (zr.height() > 330) continue; // taller than any dock could leave room beside
            w.guide.dockOpenFocus();
            QStringList unk; w.guide.setHighlight({n}, "x", 20000, unk); w.shot();
            ++tested;
            if (w.guide.dockRect().intersects(zr) || w.guide.dockRect().isNull()) { ++clashes; if (bad.size() < 4) bad << QString("%1/%2").arg(pageName(p), n); }
            const QRectF cp = w.guide.highlightCaptionRect();
            if (!cp.isNull() && (cp.intersects(zr) || cp.intersects(w.guide.dockRect()) || !QRectF(0, 0, 720, 720).contains(cp))) { ++clashes; if (bad.size() < 4) bad << QString("caption %1/%2").arg(pageName(p), n); }
            w.guide.clearHighlight();
        }
        w.guide.dockClose();
    }
    check(clashes == 0 && tested > 150, QString("across every page the dock and the caption stay clear of what is highlighted (%1 zones tried%2)").arg(tested).arg(bad.isEmpty() ? QString() : ", clashes: " + bad.join(" ")));
    // two zones far apart: still no overlap, when there is room
    w.setPage(PG_PLAY); w.guide.dockOpenFocus();
    QStringList unk; w.guide.setHighlight({zn::octave_up, zn::hdr_sound}, "both ends", 20000, unk); w.shot();
    check(w.guide.dockRect().isNull() || !(w.guide.dockRect().intersects(w.zoneRect(zn::octave_up)) && w.guide.dockRect().intersects(w.zoneRect(zn::hdr_sound))), "with a highlight at both ends the dock gives way to at least one of them");
    w.guide.clearHighlight(); w.guide.dockClose();
}

// the Help page: ask from the guide without going to the Chat page
void guideHelpPage(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; const QString &shotsDir = ctx.shotsDir; Client &c = link.c;
    w.setPage(PG_HELP); w.guide.dockClose(); w.setHelpSubPage(0); w.shot();
    tap(w, zn::help_ask); w.shot();
    check(w.guide.dockIsOpen() && w.guide.dockHasFocus() && w.page() == PG_HELP && !w.chat.draft.isEmpty(), "Ask Rook on the Help page opens the dock with the keyboard and a starting question");
    QStringList hrows; for (const QString &n : w.zoneNames()) if (n.startsWith(zn::help_row)) hrows << n;
    bool visible = true;
    for (const QString &n : hrows) visible = visible && !w.guide.dockRect().intersects(w.zoneRect(n));
    check(hrows.size() == 16 && visible, "with the dock open every Help row is still visible above it");
    for (int i = 0; i < 80; ++i) press(w, Qt::Key_Backspace);
    w.guide.dockClose(); w.shot();
    tap(w, hrows.value(8)); w.shot();
    check(w.guide.dockHasFocus() && w.chat.draft.startsWith("What does"), QString("tapping a Help row asks about it in the dock (draft: %1)").arg(w.chat.draft));
    const int before = w.chat.seq;
    press(w, Qt::Key_Return); wait(40);
    const ChatMsg &hm = w.chat.log.back();
    check(w.chat.seq == before + 1 && hm.page == "help" && hm.focus == QStringList({hrows.value(8)}) && w.page() == PG_HELP,
          QString("and the question goes out with the row as its focus, still on the Help page (%1 %2)").arg(hm.page, hm.focus.join(",")));
    QJsonObject r = c.call(QByteArray(R"({"cmd":"highlight","zones":[")") + hrows.value(3).toUtf8() + R"("],"label":"this one"})");
    check(okOf(r) && strs(result(r)["highlighted"].toArray()) == QStringList({hrows.value(3)}) && !w.guide.dockRect().intersects(w.zoneRect(hrows.value(3))), "Rook can highlight a single Help row");
    QImage im = w.shot();
    if (!shotsDir.isEmpty()) im.save(shotsDir + "/17-dock-help.png");
    const int sub0 = w.helpSubPage();
    tap(w, zn::help_next);
    check(w.helpSubPage() == (sub0 + 1) % 4, "the Help sections are still reachable with the next button");
    w.guide.clearHighlight(); w.guide.dockClose(); w.setPage(PG_PLAY);
}
} // namespace selftest
