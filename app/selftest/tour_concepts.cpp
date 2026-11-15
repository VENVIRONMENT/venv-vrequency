// venv-vrequency self-test — tour: the Concepts view, Related chips and the guided walk.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <vector>

namespace selftest {
// concepts
void tourConcepts(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const CardDb &db = ctx.db; const auto &check = ctx.check;
    t.open("synth"); settleTour(w);
    press(w, Qt::Key_C); w.shot();
    check(t.conceptsOpen() && w.zoneNames().contains(zn::tour_conceptlist), "C opens the Concepts view (a list)");
    press(w, Qt::Key_Tab);
    check(!ctx.tourState()["concept"].isNull(), "Tab moves through the concepts");
    t.selectConcept("recall"); w.shot();
    check(ctx.tourState()["concept"].toString() == "recall", "the Recall concept is a card like any other");
    press(w, Qt::Key_Escape);
    check(t.conceptsOpen() && ctx.tourState()["concept"].isNull(), "Esc first closes the concept card");
    press(w, Qt::Key_Escape);
    check(!t.conceptsOpen() && t.isOpen(), "then the Concepts view");
    tourTap(w, zn::tour_concepts); w.shot();
    check(t.conceptsOpen(), "the Concepts button toggles it by touch");
    tourTap(w, zn::tour_concepts);
    // a related chip jumps to its control, changing page when it must
    t.open("synth", zn::octave_down); settleTour(w);
    tourTap(w, zn::at(zn::tour_rel, 1)); w.shot();
    check(t.pageId() == "synth" && t.selectedName() == zn::hdr_tune || t.pageId() == "tune", "a Related chip jumps to that control");
    t.open("tune", zn::minus(zn::bpm)); settleTour(w);
    const HelpCard *bpm = db.find("tune", zn::minus(zn::bpm));
    bool jumped = false;
    if (bpm) for (int i = 0; i < bpm->related.size(); ++i) {
        t.open("tune", zn::minus(zn::bpm)); settleTour(w);
        const QString before = t.selectedName() + t.pageId();
        tourTap(w, zn::at(zn::tour_rel, i)); w.shot();
        if (t.selectedName() + t.pageId() != before || t.conceptsOpen()) jumped = true;
    }
    check(jumped, "Related chips change page or open a concept");
    t.close();
}

// guided start and replay
void tourGuidedWalk(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    ctx.call(R"({"cmd":"tour","action":"open","guided":true})");
    settleTour(w);
    check(ctx.tourState()["guided"].toBool() && ctx.tourState()["step"].toInt() == 1 && ctx.tourState()["page"].toString() == "synth", "Start here begins a guided walk on the Synth page");
    std::vector<QString> pages; pages.push_back(ctx.tourState()["page"].toString());
    for (int i = 0; i < 12 && ctx.tourState()["guided"].toBool(); ++i) { tourTap(w, zn::tour_next); w.shot(); if (ctx.tourState()["guided"].toBool()) pages.push_back(ctx.tourState()["page"].toString()); }
    check(pages.size() == 8 && pages[1] == "sound" && pages.back() == "chat", QString("Next walks through %1 pages in order and then lets go").arg(pages.size()));
    check(t.isOpen() && !ctx.tourState()["guided"].toBool(), "after the last step the player can explore freely");
    w.shot(); t.select(zn::chat_send); w.shot();
    check(t.selectedName() == zn::chat_send, "and select anything");
    ctx.call(R"({"cmd":"tour","action":"open","guided":true})");
    settleTour(w);
    tourTap(w, zn::tour_next); tourTap(w, zn::tour_back); w.shot();
    check(ctx.tourState()["step"].toInt() == 1, "Back steps back");
    tourTap(w, zn::tour_free); w.shot();
    check(!ctx.tourState()["guided"].toBool() && t.isOpen(), "Explore freely ends the walk and keeps the tour open");
    t.close();
    t.open(); settleTour(w);
    check(t.isOpen(), "the tour can be replayed any number of times");
    t.close();
}
} // namespace selftest
