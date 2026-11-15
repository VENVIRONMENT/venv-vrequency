// venv-vrequency — the control socket: page switch, tour, and the guided-help pointer (highlight, zones).
#include "app/control/control.h"
#include "app/control/control_internal.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"

using namespace td;
using namespace ctl;

namespace {
const char *const kHelpSubs[kHelpParts] = {"playing", "commands", "pulse"};
} // namespace

QJsonObject ControlServer::cmdPage(const QJsonObject &a, QString &e) {
    Args A{a, e};
    QString n = A.str("name", "", 12, true).toLower();
    if (!e.isEmpty()) return {};
    if (n == "play") n = "synth";
    int p = -1;
    for (int i = 0; i < PG_COUNT; ++i) if (n == pageName(i)) p = i;
    if (p < 0) { e = "page must be one of synth, sound, pulse, backing, tune, system, help, chat"; return {}; }
    w.tour().close(); // the real page is wanted: the tour steps aside
    if (p == PG_CHAT && w.page() != PG_CHAT) w.chat.returnPage = w.page();
    w.goPage(p);
    QJsonObject r; r["page"] = n;
    return r;
}

// tour: open the interactive tour on a page and a control (so the copilot can say "let me show you"), or close it.
QJsonObject ControlServer::cmdTour(const QJsonObject &a, QString &e) {
    Args A{a, e};
    const QString act = A.str("action", "open", 8);
    const QString page = A.str("page", "", 24);
    const QString element = A.str("element", "", 64);
    const bool guided = A.boolean("guided", false);
    if (!e.isEmpty()) return {};
    if (act != "open" && act != "close") { e = "action must be open or close"; return {}; }
    if (act == "close") { w.tour().close(); return w.tour().stateJson(); }
    QString err;
    if (!w.tour().open(page, element, guided, &err, true)) {
        e = err + " (use synth, sound, sound-deep, pulse, backing, tune, system, key-test, help or chat)";
        return {};
    }
    return w.tour().stateJson();
}

// highlight, unhighlight, zones: pointing at the screen. See docs/CONTROL.md.
QJsonObject ControlServer::cmdGuide(const QString &cmd, const QJsonObject &a, QString &e) {
    Args A{a, e};
    QJsonObject r;
    if (cmd == "unhighlight") {
        const int n = w.guide.highlightedZones().size();
        w.guide.clearHighlight();
        r["cleared"] = n;
        return r;
    }
    // optional page (and, for Help, the sub page: playing, commands, pulse)
    int page = w.page(), sub = -1;
    if (A.has("page")) {
        const QString n = A.str("page", "", 12);
        if (e.isEmpty()) { page = pageFromName(n); if (page < 0) e = "page must be one of synth, sound, pulse, backing, tune, system, help, chat"; }
    }
    if (A.has("sub")) {
        const QJsonValue v = a["sub"];
        if (v.isDouble() && v.toDouble() >= 0 && v.toDouble() < kHelpParts && v.toDouble() == std::floor(v.toDouble())) sub = (int)v.toDouble();
        else if (v.isString()) { for (int i = 0; i < kHelpParts; ++i) if (v.toString().toLower() == kHelpSubs[i]) sub = i; }
        if (sub < 0 && e.isEmpty()) e = "'sub' must be playing, commands or pulse (or 0-2)";
    }
    if (cmd == "zones") {
        if (!e.isEmpty()) return {};
        QJsonArray zs;
        const QVector<MainWindow::ZoneInfo> cat = w.zoneCatalogue(page, page == PG_HELP ? (sub >= 0 ? sub : w.helpPage.sub) : -1);
        for (const MainWindow::ZoneInfo &z : cat) {
            QJsonObject o;
            o["name"] = z.name; o["label"] = z.label;
            o["x"] = qRound(z.r.x()); o["y"] = qRound(z.r.y()); o["w"] = qRound(z.r.width()); o["h"] = qRound(z.r.height());
            if (z.covered) o["covered_by_dock"] = true;
            zs.append(o);
        }
        r["page"] = pageName(page);
        if (page == PG_HELP) r["sub"] = kHelpSubs[sub >= 0 ? sub : w.helpPage.sub];
        r["zones"] = zs;
        r["dock_open"] = w.guide.dockIsOpen() && page != PG_CHAT;
        return r;
    }
    // highlight
    QStringList names;
    if (!a.contains("zones") || !a["zones"].isArray()) e = a.contains("zones") ? "'zones' must be an array of zone names" : "missing 'zones'";
    else {
        const QJsonArray za = a["zones"].toArray();
        if (za.isEmpty() || za.size() > 16) e = "'zones' must hold 1 to 16 names";
        for (const QJsonValue &v : za) {
            if (!v.isString()) { e = "'zones' must be an array of strings"; break; }
            if (v.toString().size() > 48) { e = "a zone name is longer than 48 characters"; break; }
            names << v.toString();
        }
    }
    QString label = A.str("label", "", 40);
    for (QChar &c : label) if (c.unicode() < 0x20 || c.unicode() == 0x7f) c = ' ';
    label = label.trimmed();
    const int ms = A.integer("ms", 100, 30000, 8000);
    if (!e.isEmpty()) return {};
    w.tour().close(); // a highlight is drawn on the real page
    if (page != w.page() || (page == PG_HELP && sub >= 0 && sub != w.helpPage.sub)) { // switch first, atomically with the highlight
        if (page == PG_CHAT && w.page() != PG_CHAT) w.chat.returnPage = w.page();
        w.goPage(page);
        if (page == PG_HELP && sub >= 0) w.helpPage.sub = sub;
    }
    w.refreshZones();
    QStringList unknown;
    const QStringList known = w.guide.setHighlight(names, label, ms, unknown);
    w.refreshZones();   // the dock may have moved out of the way
    r["page"] = pageName(w.page());
    r["highlighted"] = QJsonArray::fromStringList(known);
    r["unknown"] = QJsonArray::fromStringList(unknown);
    r["ms"] = ms;
    r["caption"] = !w.guide.highlightCaptionRect().isNull();
    QJsonObject dock;
    dock["open"] = w.guide.dockIsOpen() && w.page() != PG_CHAT;
    if (!w.guide.dockRect().isNull()) { const QRectF d = w.guide.dockRect(); dock["y"] = qRound(d.y()); dock["h"] = qRound(d.height()); dock["position"] = d.y() < 100 ? "top" : "bottom"; }
    r["dock"] = dock;
    return r;
}

