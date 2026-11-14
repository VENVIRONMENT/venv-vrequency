// venv-vrequency self-test — tour: the first-run offer, and what state.json remembers of the tour.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QImage>
#include <QJsonObject>

using namespace td;

namespace selftest {
// the first-run offer
void tourFirstRunOffer(TourContext &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; AudioOut &audio = ctx.audio; TourView &t = ctx.t; const auto &check = ctx.check;
    w.setPage(PG_PLAY); w.guide.dockClose(); t.close(); w.shot();
    t.forgetSeen(); t.armFirstRunPrompt(0); w.shot();
    check(t.promptShowing() && w.zoneNames().contains(zn::tour_prompt_yes) && w.zoneNames().contains(zn::tour_prompt_later), "on a first run the offer appears (Take the tour / Later)");
    const QImage with = w.shot();
    check(w.page() == PG_PLAY, "and it does not move the player anywhere");
    // it does not block playing: a string touch under the page still sounds
    key(w, Qt::Key_A, true); wait(120);
    check(inst.uiState().voices > 0, "keys still play while the offer is up");
    key(w, Qt::Key_A, false); wait(150);
    press(w, Qt::Key_Escape);
    check(!t.promptShowing() && t.seen() && !t.isOpen(), "Esc = later: it goes away and the tour does not open");
    check(w.page() == PG_PLAY, "(the Esc was spent on the answer, not on leaving the page)");
    w.saveState();
    QJsonObject st; store::readJson(store::baseDir() + "/state.json", st);
    check(st["tourSeen"].toBool(), "tourSeen is saved in state.json");
    t.armFirstRunPrompt(0); w.shot();
    check(!t.promptShowing(), "the offer never comes back once answered");
    // Enter = yes
    t.forgetSeen(); t.armFirstRunPrompt(0); w.shot();
    const int loop0 = inst.uiState().loopState;
    press(w, Qt::Key_Return);
    settleTour(w);
    check(t.isOpen() && ctx.tourState()["guided"].toBool() && inst.uiState().loopState == loop0, "Enter = yes opens the guided tour (and did not start the loop pedal)");
    t.close();
    // touch: Later
    t.forgetSeen(); t.armFirstRunPrompt(0); w.shot();
    tourTap(w, zn::tour_prompt_later); w.shot();
    check(!t.promptShowing() && t.seen() && !t.isOpen(), "the Later button answers it by touch");
    // nobody answers: it goes away by itself, once
    t.forgetSeen(); t.armFirstRunPrompt(0, 250); w.shot();
    const bool shown = t.promptShowing();
    wait(400); w.shot(); w.shot();
    check(shown && !t.promptShowing() && t.seen(), "an offer nobody answers fades away on its own and is not repeated");
    // a fresh start reads tourSeen from state.json
    w.saveState();
    {
        MainWindow second(inst, audio);
        check(second.tour().seen(), "a fresh start remembers the offer was answered (state.json is read back)");
        second.tour().armFirstRunPrompt(0); second.shot();
        check(!second.tour().promptShowing(), "so a second launch does not offer the tour again");
    }
    {
        QJsonObject st2; store::readJson(store::baseDir() + "/state.json", st2);
        st2.remove("tourSeen"); store::writeJson(store::baseDir() + "/state.json", st2);
        MainWindow third(inst, audio);
        check(!third.tour().seen(), "a state file with no tourSeen counts as a first run");
        third.tour().armFirstRunPrompt(0); third.shot();
        check(third.tour().promptShowing(), "and gets the offer");
        third.tour().answerPrompt(false);
    }
    w.saveState();
}

// the last page is remembered
void tourRemembersLastPage(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    t.open("beat"); settleTour(w); t.close();
    w.saveState();
    QJsonObject st; store::readJson(store::baseDir() + "/state.json", st);
    check(st["tourPage"].toString() == "beat", "the last tour page is saved in state.json");
    t.open(); settleTour(w);
    check(t.pageId() == "beat", "and the tour reopens there");
    t.close();
}
} // namespace selftest
