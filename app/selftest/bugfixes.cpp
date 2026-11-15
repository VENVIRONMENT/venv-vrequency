// venv-vrequency self-test — the bugs fixed in 0.12.1 stay fixed.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QDir>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cmath>
#include <cstdio>
#include <functional>

using namespace td;

namespace selftest {
namespace {
// every armed mode is dropped whenever the page changes, by any route
void modesDropOnPageChange(Context &ctx, Client &c) {
    MainWindow &w = ctx.w;
    struct Route { const char *name; std::function<void()> go; };
    const Route routes[] = {
        {"a tab tap", [&] { tap(w, zn::tab("pulse")); }},
        {"the Tab key", [&] { press(w, Qt::Key_Tab); }},
        {"Esc", [&] { press(w, Qt::Key_Escape); }},
        {"the control socket page command", [&] { c.call(R"({"cmd":"page","name":"tune"})"); }},
        {"a highlight that names a page", [&] { c.call(R"({"cmd":"highlight","zones":["loop_pedal"],"page":"synth","ms":500})"); }},
        {"Ctrl+Shift+Y (Chat page)", [&] { key(w, Qt::Key_Shift, true); ctrl(w, Qt::Key_Y); key(w, Qt::Key_Shift, false); }},
        {"Ctrl+H (Help)", [&] { ctrl(w, Qt::Key_H); }},
    };
    for (const Route &r : routes) {
        w.setPage(PG_SOUND); w.shot();
        w.armAllModes();
        check(w.armedModes() == "delete,store", "both modes can be armed");
        r.go(); w.shot();
        check(w.page() != PG_SOUND && w.armedModes().isEmpty(), QString("leaving the page by %1 clears Delete and Store").arg(r.name));
        w.guide.clearHighlight(); w.guide.dockClose();
    }
    w.setPage(PG_SOUND); w.shot();
    const int favN = QDir(store::baseDir() + "/favourites").entryList({"*.json"}).size();
    tap(w, zn::favdel); w.shot();
    check(w.armedModes() == "delete", "Delete arms on the Sound page");
    tap(w, zn::tab("tune")); w.shot(); tap(w, zn::tab("sound")); w.shot();
    tap(w, zn::at(zn::favitem, 0)); wait(60);
    check(QDir(store::baseDir() + "/favourites").entryList({"*.json"}).size() == favN, "the favourite tapped after coming back is recalled, not deleted");
    w.setPage(PG_PLAY); w.shot();
}

// the key test panel and the ticker swallow touches
void overlaysSwallowTouches(Context &ctx) {
    MainWindow &w = ctx.w;
    w.setPage(PG_SYS); w.shot();
    const QPointF q = w.zoneRect(zn::quit).center(), fs = w.zoneRect(zn::factory).center();
    check(w.zoneAt(q) == zn::quit && w.zoneAt(fs) == zn::factory, "(control) Quit and Factory sounds take their own touches");
    w.setKeyTest(true); w.shot();
    check(w.zoneAt(q) == zn::keytestblock && w.zoneAt(fs) == zn::keytestblock && w.zoneAt(QPointF(4, 4)) == zn::keytestblock, "with the key test open, touches over Quit, Factory sounds and the tabs go to the panel");
    check(w.zoneAt(w.zoneRect(zn::keytestclose).center()) == zn::keytestclose, "but Close still works");
    w.setKeyTest(false);
    w.setPage(PG_PLAY); w.setKeyTest(true); w.shot();
    const QPointF m = w.zoneRect(zn::menu).center();
    w.pointer(5, m, PH_PRESS); w.pointer(5, m, PH_RELEASE);
    check(w.page() == PG_PLAY, "a tap on Menu under the key test panel does nothing");
    w.setKeyTest(false);
    w.setPage(PG_SYS); w.guide.dockClose(); w.shot();
    w.chat.add("rook", "Ticker over the bottom row.", "say", false); w.shot();
    check(w.chat.tickerShowing() && w.zoneAt(q) == zn::ticker && w.zoneAt(fs) == zn::ticker, "the ticker takes the touches over Quit and Factory sounds");
    tap(w, zn::ticker); w.shot();
    check(w.guide.dockIsOpen() && w.zoneAt(w.guide.dockRect().center()).startsWith(zn::dock_prefix), "and the chat dock takes every touch inside it");
    w.guide.dockClose(); w.setPage(PG_PLAY);
}

// favourites: overwrite by name; the paging button goes both ways
void favouritesOverwriteAndPage(Context &ctx, Client &c) {
    MainWindow &w = ctx.w;
    QDir fd(store::baseDir() + "/favourites");
    c.call(R"({"cmd":"save_favourite","name":"Same name"})");
    const int n1 = fd.entryList({"*.json"}).size();
    const QJsonObject r2 = c.call(R"({"cmd":"save_favourite","name":"Same name"})");
    check(fd.entryList({"*.json"}).size() == n1 && r2["result"].toObject()["updated"].toBool() && !fd.exists("same-name-2.json"),
          "saving a favourite under an existing name updates it instead of adding a copy");
    for (int i = 1; i <= 14; ++i) c.call(QString(R"({"cmd":"save_favourite","name":"Pg %1"})").arg(i).toUtf8());
    const int total = fd.entryList({"*.json"}).size(), pages = (total + 5) / 6;
    check(pages >= 3, QString("%1 favourites make %2 pages").arg(total).arg(pages));
    w.setPage(PG_SOUND); w.shot();
    const QRectF fr = w.zoneRect(zn::favprev);
    auto at = [&](double fx) { const QPointF p(fr.left() + fr.width() * fx, fr.center().y()); w.pointer(6, p, PH_PRESS); w.pointer(6, p, PH_RELEASE); w.shot(); };
    const int p0 = w.soundPage.favouritesPage();
    at(0.8); const int p1 = w.soundPage.favouritesPage();
    at(0.2); const int p2 = w.soundPage.favouritesPage();
    at(0.2); const int p3 = w.soundPage.favouritesPage();
    at(0.8); const int p4 = w.soundPage.favouritesPage();
    check(p0 == 0 && p1 == 1 && p2 == 0 && p3 == pages - 1 && p4 == 0, "right half pages forward, left half pages back, both wrapping");
    tap(w, zn::favprev); w.shot();
    check(w.soundPage.favouritesPage() == 1, "a tap in the middle (the control socket) still goes forward");
    w.setPage(PG_PLAY); w.shot();
}

// one tempo step everywhere
void tempoStepsByOne(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    inst.set(S_BPM, 100); wait(40);
    w.setPage(PG_BACK); w.shot(); tap(w, zn::plus(zn::bpm2)); wait(60);
    check(std::lround(inst.uiState().set.bpm) == 101, "Backing page tempo steps by 1");
    w.shot(); tap(w, zn::minus(zn::bpm2)); wait(60);
    check(std::lround(inst.uiState().set.bpm) == 100, "Backing page tempo steps back by 1");
    w.setPage(PG_TUNE); w.shot(); tap(w, zn::plus(zn::bpm)); wait(60);
    check(std::lround(inst.uiState().set.bpm) == 101, "Tune page tempo steps by 1");
    w.setPage(PG_PLAY); w.shot();
}
} // namespace

void testBugFixes(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- bug fixes (0.12.1)\n");
    QTemporaryDir bdir;
    const QString bsock = bdir.path() + "/vrequency.sock";
    ControlServer bctl(w, inst);
    check(bctl.start(bsock), "a control socket for the bug-fix checks");
    Client c;
    check(c.open(bsock), "a client connects");
    w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY); w.shot();

    modesDropOnPageChange(ctx, c);
    overlaysSwallowTouches(ctx);
    favouritesOverwriteAndPage(ctx, c);
    tempoStepsByOne(ctx);
}
} // namespace selftest
