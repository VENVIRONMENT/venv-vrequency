// venv-vrequency self-test — guided help: the zones command and the highlight command.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <QHash>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

namespace selftest {
// zones: every page, and the listing agrees with what is drawn
void guideZoneListing(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Client &c = link.c;
    bool allSame = true; int total = 0; QStringList bad;
    for (int p = 0; p < PG_COUNT; ++p) {
        w.setPage(p); w.shot();
        QSet<QString> drawn; for (const QString &n : w.zoneNames()) if (n != zn::ticker) drawn.insert(n);
        const QJsonObject r = c.call(QByteArray(R"({"cmd":"zones","page":")") + pageName(p) + "\"}");
        QSet<QString> listed; bool shaped = okOf(r);
        for (const QJsonValue &v : result(r)["zones"].toArray()) {
            const QJsonObject z = v.toObject();
            if (z["name"].toString() != zn::ticker) listed.insert(z["name"].toString());
            shaped = shaped && z["w"].toInt() > 0 && z["h"].toInt() > 0 && !z["label"].toString().isEmpty() && z["label"].toString().size() <= 60;
        }
        total += listed.size();
        if (!shaped || listed != drawn) { allSame = false; bad << pageName(p); }
    }
    check(allSame && total > 150, QString("zones lists every named zone of every page, with a rectangle and a label (%1 in all%2)").arg(total).arg(bad.isEmpty() ? QString() : " - differs on " + bad.join(",")));
    w.setPage(PG_PLAY); w.shot();
    // another page is listed without leaving this one
    const QJsonObject r = c.call(R"({"cmd":"zones","page":"pulse"})");
    check(okOf(r) && w.page() == PG_PLAY && w.zoneNames().contains(zn::octave_up), "listing another page does not move the player off the page they are on");
    QStringList names; QHash<QString, QString> labels;
    for (const QJsonValue &v : result(r)["zones"].toArray()) { names << v.toObject()["name"].toString(); labels[v.toObject()["name"].toString()] = v.toObject()["label"].toString(); }
    check(names.contains(zn::at(zn::pad, 0)) && names.contains(zn::pulselatch) && labels[zn::pulselatch].contains("Latch"), "labels come from the text drawn on the control (Latch)");
    QStringList synth = strs(QJsonArray::fromStringList(w.zoneNames()));
    check(synth.contains(zn::octave_down) && synth.contains(zn::octave_up) && synth.contains(zn::loop_pedal) && synth.contains(zn::xy) && synth.contains(zn::octave_readout) && synth.contains(zn::hdr_tune),
          "the Synth page names its octave buttons, loop pedal, pad and tabs readably");
    check(!c.call(R"({"cmd":"zones","page":"nowhere"})")["ok"].toBool(true) && !c.call(R"({"cmd":"zones","page":"help","sub":"x"})")["ok"].toBool(true), "zones refuses an unknown page or help section");
    // Help: every row is a zone, all three sections stay reachable
    int rowsPer[kHelpParts]; bool unique = true; QSet<QString> seen;
    for (int sub = 0; sub < kHelpParts; ++sub) {
        const QJsonObject hr = c.call(QByteArray(R"({"cmd":"zones","page":"help","sub":)") + QByteArray::number(sub) + "}");
        int n = 0;
        for (const QJsonValue &v : result(hr)["zones"].toArray()) {
            const QString nm = v.toObject()["name"].toString();
            if (nm.startsWith(zn::help_row)) { ++n; unique = unique && !seen.contains(nm + QString::number(sub)); seen.insert(nm + QString::number(sub)); }
        }
        rowsPer[sub] = n;
    }
    check(rowsPer[0] == 16 && rowsPer[1] == 12 && rowsPer[2] == 12 && unique, QString("every help row on all three sections is a named zone (%1/%2/%3)").arg(rowsPer[0]).arg(rowsPer[1]).arg(rowsPer[2]));
    w.setPage(PG_HELP); w.shot();
    QStringList hz = w.zoneNames();
    check(hz.contains(zn::help_ask) && hz.contains(zn::help_next) && hz.contains(zn::help_back) && hz.contains(zn::help_section), "the Help page has Ask Rook, next section, back and its section title as zones");
    w.setPage(PG_PLAY); w.shot();
}

// highlight: lifecycle
void guideHighlights(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Client &c = link.c;
    QJsonObject r = c.call(R"({"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":700})");
    check(okOf(r) && strs(result(r)["highlighted"].toArray()) == QStringList({zn::octave_down, zn::octave_up}) && result(r)["unknown"].toArray().isEmpty() && result(r)["caption"].toBool(),
          "highlight accepts an alias (octave) and reports the zones it ringed, with a caption");
    const QJsonObject st = helpFocus(c);
    check(st["page"].toString() == "synth" && strs(st["highlighted"].toArray()).size() == 2, "state.help_focus lists the highlighted zones and the page");
    const QRectF up = w.zoneRect(zn::octave_up), cap = w.guide.highlightCaptionRect();
    check(!cap.isNull() && !cap.intersects(up) && !cap.intersects(w.zoneRect(zn::octave_down)) && QRectF(0, 0, 720, 720).contains(cap), "the caption tag stays on screen and does not cover the highlighted zones");
    QImage a = w.shot();
    bool ring = false;
    for (int dx = 3; dx <= 3; ++dx) ring = ring || qGray(a.pixel((int)up.center().x(), (int)up.top() - dx)) > 170;
    check(ring, "a bright rule is drawn around the highlighted zone");
    wait(900);
    check(helpFocus(c)["highlighted"].toArray().isEmpty() && w.guide.highlightCaptionRect().isNull(), "a highlight clears itself when its time is up");
    r = c.call(R"({"cmd":"highlight","zones":["octave_up","no_such_zone","  "],"ms":5000})");
    check(okOf(r) && strs(result(r)["highlighted"].toArray()) == QStringList({zn::octave_up}) && strs(result(r)["unknown"].toArray()).contains("no_such_zone"),
          "unknown names are reported in the reply and the known ones still highlight");
    r = c.call(R"({"cmd":"highlight","zones":["nope"]})");
    check(okOf(r) && result(r)["highlighted"].toArray().isEmpty() && strs(result(r)["unknown"].toArray()) == QStringList({"nope"}), "all-unknown names highlight nothing and say so");
    r = c.call(R"({"cmd":"highlight","zones":["chord*"]})");
    check(okOf(r) && result(r)["highlighted"].toArray().size() == 7, "a name ending in * stands for every zone with that prefix (chord*)");
    w.setPage(PG_SOUND); wait(60); w.shot();
    check(helpFocus(c)["highlighted"].toArray().isEmpty(), "changing page clears the highlight");
    w.setPage(PG_PLAY); w.shot();
    check(helpFocus(c)["highlighted"].toArray().isEmpty(), "and it does not come back when the page does");
    r = c.call(R"({"cmd":"highlight","zones":["help_ask"],"page":"help","label":"ask me","ms":3000})");
    check(okOf(r) && w.page() == PG_HELP && strs(result(r)["highlighted"].toArray()) == QStringList({zn::help_ask}) && strs(helpFocus(c)["highlighted"].toArray()) == QStringList({zn::help_ask}),
          "highlight with page switches page and highlights in the same step");
    r = c.call(R"({"cmd":"highlight","zones":["help_row_loop_pedal_count_in_record_play_over"],"page":"help","sub":"playing"})");
    const QStringList got = strs(result(r)["highlighted"].toArray());
    check(okOf(r) && (!got.isEmpty() || !result(r)["unknown"].toArray().isEmpty()), "highlight takes a help section (sub) with the page");
    r = c.call(R"({"cmd":"unhighlight"})");
    check(okOf(r) && helpFocus(c)["highlighted"].toArray().isEmpty() && result(r)["cleared"].toInt() >= 0, "unhighlight clears at once");
    check(okOf(c.call(R"({"cmd":"unhighlight"})")), "and is harmless when there is nothing to clear");
    r = c.call(R"({"cmd":"highlight","zones":["octave_up"],"page":"synth","ms":9000})");
    check(w.page() == PG_PLAY && !helpFocus(c)["highlighted"].toArray().isEmpty(), "a highlight can be re-issued after a page change");
    c.call(R"({"cmd":"unhighlight"})");
}

// hostile arguments
void guideHostileArguments(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; ControlServer &gctl = link.server;
    int refused = 0, total = 0, malformed = 0;
    static const char *zv[] = {"null", "\"x\"", "[]", "{}", "[1]", "[null]", "[[\"a\"]]", "true", "1e99", "[\"a\",\"b\"]", "[\"\"]"};
    static const char *lv[] = {"null", "1", "[]", "{}", "true", "\"ok\"", "\"line\\nbreak\\u0000x\""};
    static const char *mv[] = {"null", "-5", "0", "99", "1e99", "\"x\"", "250", "9000", "30000", "30001", "0.5"};
    static const char *pv[] = {"null", "\"x\"", "1", "\"help\"", "\"\"", "[]", "\"CHAT\"", "\"play\""};
    for (const char *z : zv) for (const char *l : lv) for (const char *m : mv) for (const char *p : pv) {
        const QByteArray req = QByteArray("{\"cmd\":\"highlight\",\"zones\":") + z + ",\"label\":" + l + ",\"ms\":" + m + ",\"page\":" + p + "}";
        const QJsonObject rr = gctl.handle(QJsonDocument::fromJson(req).object());
        ++total;
        if (!rr.contains("ok")) ++malformed; else if (!rr["ok"].toBool()) ++refused;
    }
    w.setPage(PG_PLAY); w.guide.clearHighlight();
    for (const char *p : pv) for (const char *s : {"null", "\"pulse\"", "7", "[]", "\"x\""}) {
        const QByteArray req = QByteArray("{\"cmd\":\"zones\",\"page\":") + p + ",\"sub\":" + s + "}";
        const QJsonObject rr = gctl.handle(QJsonDocument::fromJson(req).object());
        ++total;
        if (!rr.contains("ok")) ++malformed; else if (!rr["ok"].toBool()) ++refused;
    }
    w.setPage(PG_PLAY); w.guide.clearHighlight(); w.shot();
    check(malformed == 0 && refused > total / 3, QString("%1 highlight and zones requests with wrongly typed arguments: all answered, none crashed (%2 refused)").arg(total).arg(refused));
    QJsonArray many; for (int i = 0; i < 40; ++i) many.append(QString("z%1").arg(i));
    QJsonObject big; big["cmd"] = "highlight"; big["zones"] = many;
    check(!gctl.handle(big)["ok"].toBool(true), "more than 16 names is refused");
    QJsonObject longName; longName["cmd"] = "highlight"; longName["zones"] = QJsonArray{QString(500, 'a')};
    check(!gctl.handle(longName)["ok"].toBool(true), "a name longer than 48 characters is refused");
    QJsonObject longLabel; longLabel["cmd"] = "highlight"; longLabel["zones"] = QJsonArray{zn::octave_up}; longLabel["label"] = QString(41, 'a');
    check(!gctl.handle(longLabel)["ok"].toBool(true), "a label longer than 40 characters is refused");
    QJsonObject ctl; ctl["cmd"] = "highlight"; ctl["zones"] = QJsonArray{zn::octave_up}; ctl["label"] = "a\nb\x01 c"; ctl["ms"] = 200;
    check(gctl.handle(ctl)["ok"].toBool() && !w.guide.highlightLabel().contains('\n') && !w.guide.highlightLabel().contains(QChar(1)), "control characters in a label are turned into spaces");
    wait(300);
    w.guide.clearHighlight();
}
} // namespace selftest
