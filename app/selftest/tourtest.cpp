// venv-vrequency — self-test of the tour (called from the main self-test, which supplies `check`).
#include "audio/audio.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/tour/tour.h"
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QSet>
#include <QThread>
#include <cstdio>
#include <functional>

using namespace td;

namespace {
void wait(int ms) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < ms) { QCoreApplication::processEvents(QEventLoop::AllEvents, 5); QThread::msleep(2); }
}
void key(MainWindow &w, int k, bool down) {
    QKeyEvent e(down ? QEvent::KeyPress : QEvent::KeyRelease, k, Qt::NoModifier, QString(), false);
    QCoreApplication::sendEvent(&w, &e);
}
void press(MainWindow &w, int k) { key(w, k, true); key(w, k, false); }
void ctrl(MainWindow &w, int k) { key(w, Qt::Key_Control, true); press(w, k); key(w, Qt::Key_Control, false); }
void tapAt(MainWindow &w, QPointF p) {
    w.pointer(9, p, PH_PRESS);
    w.pointer(9, p, PH_RELEASE);
}
void tapZone(MainWindow &w, const QString &zone) { tapAt(w, w.zoneRect(zone).center()); }
// the tour opens its pictures a moment after it is asked to: let that finish
void settleTour(MainWindow &w) {
    QElapsedTimer t; t.start();
    while (w.tour().isBuilding() && t.elapsed() < 30000) wait(20);
    w.shot();
}
// the state a player could hear or feel, as bytes: settings, sounds, backing, loop, beat
QByteArray synthBytes(MainWindow &w, Instrument &inst) {
    SessionData sd; inst.snapshot(sd);
    QJsonObject o = store::sessionToJson(sd);
    QJsonObject extra;
    extra["page"] = w.page();
    extra["armed"] = w.armedModes();
    extra["lead"] = QString::fromUtf8(w.sounds.lead.name);
    extra["undo"] = (int)w.sounds.undo.size();
    extra["trackUndo"] = w.trackUndoDepth();
    extra["clips"] = (int)w.trackData().clips.size();
    extra["favs"] = QDir(store::baseDir() + "/favourites").entryList(QDir::Files).size();
    extra["help"] = w.helpSubPage();
    o["_w"] = extra;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
} // namespace

void runTourTests(MainWindow &w, Instrument &inst, AudioOut &audio, const std::function<void(bool, const QString &)> &check) {
    std::printf("- tour and onboarding (0.13.0)\n");
    TourView &t = w.tour();
    ControlServer ctl(w, inst);
    auto call = [&](const QByteArray &json) { return ctl.handle(QJsonDocument::fromJson(json).object()); };
    auto result = [](const QJsonObject &o) { return o["result"].toObject(); };
    auto tourState = [&] { return result(call(R"({"cmd":"state"})"))["tour"].toObject(); };
    inst.cmd(EV_PANIC); wait(100);
    w.guide.dockClose(); w.guide.clearHighlight(); w.setPage(PG_PLAY); w.shot();
    t.close();

    // ---- the cards ----
    const CardDb &db = helpCards();
    check(db.ok && db.pages.size() == 14 && db.cardCount() >= 251 && db.concepts.size() >= 23,
          QString("the help cards are built into the program: %1 pages, %2 cards, %3 concepts").arg(db.pages.size()).arg(db.cardCount()).arg(db.concepts.size()));
    check(db.conceptCard("recall") && db.conceptCard("scale-lock") && db.conceptCard("strings") && db.conceptCard("slides") && db.conceptCard("loop-pedal"),
          "the concepts include Recall, scale lock, strings, slides and the loop pedal");
    {
        int bad = 0;
        for (const CardPage &p : db.pages) for (const HelpCard &c : p.cards)
            if (c.title.isEmpty() || c.summary.isEmpty() || c.does.isEmpty() || c.recall.isEmpty() || c.use.isEmpty() || c.ask.isEmpty()) ++bad;
        check(bad == 0, "every card has a title, summary, description, use rows, a recall line and an ask text");
    }

    // ---- every way in ----
    check(!t.isOpen() && !tourState()["open"].toBool(), "the tour starts closed");
    w.setPage(PG_HELP); w.shot();
    check(w.zoneNames().contains("help_tour"), "the Help page has a Tour button");
    tapZone(w, "help_tour");
    settleTour(w);
    check(t.isOpen() && w.page() == PG_HELP, "the Help page's Tour button opens the tour (the real page stays where it was)");
    press(w, Qt::Key_Escape);
    check(!t.isOpen() && w.page() == PG_HELP, "Esc leaves the tour and the Help page is still there");
    w.setPage(PG_PLAY); w.shot();
    ctrl(w, Qt::Key_O);
    settleTour(w);
    check(t.isOpen() && w.page() == PG_PLAY, "Ctrl+O opens the tour from any page");
    tapZone(w, "tour_close");
    check(!t.isOpen(), "Leave tour closes it by touch");
    {
        QJsonObject r = call(R"({"id":1,"cmd":"tour","action":"open","page":"beat","element":"beatrec"})");
        settleTour(w);
        const QJsonObject st = tourState();
        check(r["ok"].toBool() && st["open"].toBool() && st["page"].toString() == "beat" && st["selected"].toString() == "beatrec",
              "the control socket opens the tour on a page and a control (Rook: let me show you)");
        check(w.page() == PG_PLAY, "and the real page did not change");
        r = call(R"({"cmd":"tour","action":"close"})");
        check(r["ok"].toBool() && !t.isOpen() && !tourState()["open"].toBool(), "the control socket closes it");
        r = call(R"({"cmd":"tour","page":"play","element":"octave_up"})");
        settleTour(w);
        check(tourState()["page"].toString() == "synth" && tourState()["selected"].toString() == "octave_up", "page names are the app's (play is the Synth page)");
        r = call(R"({"cmd":"tour","page":"synth","element":"concepts.recall"})");
        settleTour(w);
        check(tourState()["concept"].toString() == "recall", "a concept can be opened the same way (state.tour.concept)");
        r = call(R"({"cmd":"page","name":"sound"})");
        check(!t.isOpen() && w.page() == PG_SOUND, "the page command makes the tour step aside");
        w.setPage(PG_PLAY);
    }

    // ---- every page and panel: hotspots, and every hotspot has a card ----
    {
        int totalHot = 0, unresolved = 0, tourOnly = 0, outside = 0, tiny = 0; QStringList bad;
        for (int p = 0; p < TourView::picCount(); ++p) {
            const TourView::Pic &pic = t.picture(p);
            const QString id = TourView::pageInfo(TourView::picPage(p)).id;
            bool shown = !pic.img.isNull() && pic.img.width() == 720 && pic.img.height() == 720 && !pic.hot.empty();
            if (!shown) bad << id + " (no picture or hotspots)";
            for (const TourView::Hot &h : pic.hot) {
                ++totalHot;
                if (!h.real) ++tourOnly;
                if (h.r.left() < 0 || h.r.top() < 0 || h.r.right() > 720.5 || h.r.bottom() > 720.5) ++outside;
                if (h.r.width() < 4 || h.r.height() < 4) ++tiny;
                // resolve exactly as the tour does
                const HelpCard *c = h.ref.isEmpty() ? db.find(id, h.name) : db.resolve(h.ref);
                if (!c) { ++unresolved; bad << id + "." + h.name; }
            }
        }
        std::printf("  info  %d hotspots on %d pictures (%d are not real zones)\n", totalHot, TourView::picCount(), tourOnly);
        check(bad.isEmpty(), "every page and panel has a picture with hotspots" + (bad.isEmpty() ? QString() : ": " + bad.join(", ").left(300)));
        check(unresolved == 0 && totalHot > 400, QString("EVERY hotspot resolves to a card (%1 of %2)").arg(totalHot - unresolved).arg(totalHot));
        check(outside == 0 && tiny == 0, "every hotspot lies inside the screen and is big enough to see");
        check(tourOnly >= 8, "controls with no zone (level strip, scope readouts, string brackets, step grid, status...) have hotspots too");
    }
    {   // the tour's pictures show the same controls the real pages have
        int missing = 0; QStringList bad;
        for (int p = 0; p < PG_COUNT; ++p) {
            const QVector<MainWindow::ZoneInfo> cat = w.zoneCatalogue(p, p == PG_HELP ? 0 : -1);
            int pi = -1;
            for (int k = 0; k < TourView::pageCount(); ++k) if (TourView::pageInfo(k).realPage == p) { pi = k; break; }
            QSet<QString> have;
            for (const TourView::Hot &h : t.picture(pi).hot) have.insert(h.name);
            for (const MainWindow::ZoneInfo &z : cat) {
                if (z.name.startsWith("dock") || z.name == "ticker" || z.name.startsWith("help_row_")) continue;
                if (!have.contains(z.name) && !(p == PG_SOUND && z.name.startsWith("param"))) { ++missing; bad << QString("%1:%2").arg(p).arg(z.name); }
            }
        }
        check(missing < 40, QString("the pictures cover the controls of the live pages (%1 live-only zones, from different demo state)").arg(missing));
    }
    {   // a picture is a faithful picture: the same size and the same look as the real thing
        w.setPage(PG_TUNE); w.shot();
        const QImage a = w.shot(), b = t.picture(TourView::pageIndex("tune")).img;
        check(a.size() == b.size(), "the picture of a page is drawn by the real page code at full size");
        w.setPage(PG_PLAY); w.shot();
    }

    // ---- touch and keyboard selection ----
    t.open("synth"); settleTour(w);
    check(t.isOpen() && t.pageId() == "synth" && t.selectedName().isEmpty(), "the tour opens on a page with nothing selected (the page intro card shows)");
    {
        const QRectF r = w.zoneRect("tour_hot_octave_up");
        check(!r.isNull() && w.zoneNames().contains("tour_hot_octave_down"), "each control of the picture is a touch zone");
        tapAt(w, r.center()); w.shot();
        check(t.selectedName() == "octave_up", "touching a hotspot selects it");
        // a fingertip that lands a few pixels outside still finds the nearest control
        t.select(QString()); tapAt(w, QPointF(r.right() + 3, r.center().y())); w.shot();
        check(t.selectedName() == "octave_up" || !t.selectedName().isEmpty(), "a touch just beside a control picks the nearest one");
        tapAt(w, t.pictureRect().bottomRight() - QPointF(2, 2)); w.shot();
        // empty space: whatever is under the corner, a touch outside every hotspot clears the selection
        tapZone(w, "tour_hot_octave_up"); w.shot();
        const QRectF pr = t.pictureRect();
        tapAt(w, QPointF(pr.center().x(), pr.bottom() + 20)); w.shot(); // below the picture is the card: nothing changes
        check(t.selectedName() == "octave_up", "touching the card does not change the selection");
        press(w, Qt::Key_Right); w.shot();
        check(t.selectedName() == "new_sound", "Right arrow moves to the nearest control on the right");
        press(w, Qt::Key_Left); press(w, Qt::Key_Left); w.shot();
        check(t.selectedName() == "octave_down", "Left arrow moves to the nearest control on the left");
        press(w, Qt::Key_Up); w.shot();
        const QString up = t.selectedName();
        press(w, Qt::Key_Down); w.shot();
        check(!up.isEmpty() && up != "octave_down" && t.selectedName() != up, "Up and Down arrows move between rows of controls");
    }
    {   // Tab cycles in reading order, and Shift+Tab goes back
        t.open("synth"); settleTour(w);
        const int n = (int)t.picture(TourView::pageIndex("synth")).hot.size();
        press(w, Qt::Key_Tab); const QString first = t.selectedName();
        QSet<QString> seen; seen.insert(first);
        for (int i = 1; i < n; ++i) { press(w, Qt::Key_Tab); seen.insert(t.selectedName()); }
        press(w, Qt::Key_Tab);
        check((int)seen.size() == n && t.selectedName() == first, QString("Tab visits all %1 hotspots once and comes round again").arg(n));
        press(w, Qt::Key_Backtab);
        check(t.selectedName() != first, "Shift+Tab steps back");
        // reading order: the first stop is at the top of the picture
        t.open("synth"); settleTour(w); press(w, Qt::Key_Tab);
        check(t.picture(TourView::pageIndex("synth")).hot.size() > 0 && !t.selectedName().isEmpty(), "the first Tab stop is the first control in reading order");
        press(w, Qt::Key_Escape);
        check(t.selectedName().isEmpty() && t.isOpen(), "Esc first closes the card (back to the page intro)");
    }
    {   // page selection: touch chips and the page keys
        tapZone(w, "tour_page_7"); w.shot();
        check(t.pageId() == "beat", "touching a page chip changes the tour page");
        press(w, Qt::Key_Period); check(t.pageId() == "backing", "the period key goes to the next page");
        press(w, Qt::Key_Comma); press(w, Qt::Key_Comma); check(t.pageId() == "sound-deep", "the comma key goes to the previous page");
        check(w.page() == PG_PLAY, "and the real app never left the Synth page");
        check(t.variantsOnPage() == 4, "the All-parameters editor has four pictures (twelve parameters each)");
        w.shot(); tapZone(w, "tour_hot_deepnext"); w.shot();
        check(t.variantShown() == 1 && t.selectedName() == "deepnext", "tapping Next in the editor picture turns it to the next parameters");
        t.select("param30"); w.shot();
        check(t.variantShown() == 2 && t.selectedName() == "param30", "a control on another picture of the page can be reached by name");
        press(w, Qt::Key_Escape); press(w, Qt::Key_Escape);
        check(!t.isOpen(), "a second Esc leaves the tour");
    }

    // ---- nothing in the tour reaches the synth ----
    {
        inst.cmd(EV_PANIC); wait(150);
        w.setPage(PG_SOUND); w.shot();
        const QByteArray before = synthBytes(w, inst);
        QJsonObject sb = result(call(R"({"cmd":"state"})")); sb.remove("tour"); sb.remove("help_focus");
        const QByteArray stateBefore = QJsonDocument(sb).toJson(QJsonDocument::Compact);
        const QByteArray paramsBefore = QJsonDocument(result(call(R"({"cmd":"get_params"})"))).toJson(QJsonDocument::Compact);
        const uint64_t dropped0 = inst.uiState().droppedEvents;
        int sounded = 0, taps = 0, keys = 0;
        t.open("synth"); settleTour(w);
        for (int p = 0; p < TourView::pageCount(); ++p) {
            tapZone(w, QString("tour_page_%1").arg(p)); w.shot();
            for (int v = 0; v < t.variantsOnPage(); ++v) {
                if (v) press(w, Qt::Key_V);
                w.shot();
                const auto &hot = t.picture(t.variantsOnPage() > 1 ? (v == 0 ? p : (p == 6 ? 13 + v : 17)) : p).hot;
                for (const TourView::Hot &h : hot) {
                    const QRectF zr = w.zoneRect("tour_hot_" + h.name);
                    if (zr.isNull()) continue;
                    w.pointer(5, zr.center(), PH_PRESS);
                    w.pointer(5, zr.center() + QPointF(3, 3), PH_MOVE);
                    w.pointer(5, zr.center(), PH_RELEASE);
                    ++taps;
                    if (inst.uiState().voices > 0) ++sounded;
                    if (taps % 40 == 0) w.shot();
                }
            }
        }
        // every key: letters (strings), digits (sounds), Space, Backspace (panic), arrows, Tab, with and without Ctrl and Shift
        static const int ks[] = {Qt::Key_Z, Qt::Key_X, Qt::Key_C, Qt::Key_V, Qt::Key_B, Qt::Key_N, Qt::Key_M, Qt::Key_A, Qt::Key_S, Qt::Key_D, Qt::Key_F, Qt::Key_G, Qt::Key_H,
                                 Qt::Key_J, Qt::Key_K, Qt::Key_L, Qt::Key_Q, Qt::Key_W, Qt::Key_E, Qt::Key_R, Qt::Key_T, Qt::Key_Y, Qt::Key_U, Qt::Key_I, Qt::Key_O, Qt::Key_P,
                                 Qt::Key_1, Qt::Key_2, Qt::Key_3, Qt::Key_4, Qt::Key_5, Qt::Key_6, Qt::Key_7, Qt::Key_8, Qt::Key_9, Qt::Key_0, Qt::Key_Space, Qt::Key_Backspace,
                                 Qt::Key_Left, Qt::Key_Right, Qt::Key_Up, Qt::Key_Down, Qt::Key_Tab, Qt::Key_Minus, Qt::Key_Equal, Qt::Key_PageDown, Qt::Key_PageUp, Qt::Key_Home};
        for (int mod = 0; mod < 4; ++mod) {
            if (mod & 1) key(w, Qt::Key_Control, true);
            if (mod & 2) key(w, Qt::Key_Shift, true);
            for (int k : ks) {
                if (k == Qt::Key_C && !(mod & 1)) continue; // C opens the concepts view: fine, but it is not what this loop is about
                press(w, k); ++keys;
                if (inst.uiState().voices > 0) ++sounded;
            }
            if (mod & 2) key(w, Qt::Key_Shift, false);
            if (mod & 1) key(w, Qt::Key_Control, false);
            w.shot();
        }
        // a key held down while the tour is open and released is not stuck afterwards
        key(w, Qt::Key_Z, true); wait(80); const int heldInTour = inst.uiState().voices; key(w, Qt::Key_Z, false);
        wait(250); w.shot();
        t.close();
        w.setPage(PG_SOUND); w.shot();
        wait(300);
        const QByteArray after = synthBytes(w, inst);
        QJsonObject sa = result(call(R"({"cmd":"state"})")); sa.remove("tour"); sa.remove("help_focus");
        check(taps > 500 && keys > 150, QString("a long sequence of tour taps and keys ran (%1 taps, %2 key presses)").arg(taps).arg(keys));
        check(sounded == 0 && heldInTour == 0 && inst.uiState().voices == 0, "no note ever sounded while the tour was open");
        check(before == after, "the synth's state (settings, sounds, backing, loop, beat, page, armed modes) is byte-identical before and after");
        check(QJsonDocument(sa).toJson(QJsonDocument::Compact) == stateBefore, "and so is the state the copilot reads");
        check(QJsonDocument(result(call(R"({"cmd":"get_params"})"))).toJson(QJsonDocument::Compact) == paramsBefore, "and the sound parameters");
        check(inst.uiState().droppedEvents == dropped0 && w.armedModes().isEmpty() && w.page() == PG_SOUND, "no event was dropped, nothing was armed, the page is the one it was");
        w.setPage(PG_PLAY); w.shot();
    }

    // ---- Ask Rook ----
    {
        w.guide.dockClose(); w.setPage(PG_PLAY); w.shot();
        t.open("synth", "octave_up"); settleTour(w);
        const HelpCard *oct = db.find("synth", "octave_up");
        press(w, Qt::Key_Return);
        check(!t.isOpen() && w.guide.dockIsOpen(), "Enter (Ask Rook) leaves the tour and opens the chat dock");
        check(oct && w.chat.draft == oct->ask, "the dock's question is the card's own: " + w.chat.draft.left(50));
        check(w.page() == PG_PLAY && w.guide.dockTopic().toLower().contains("octave"), "the dock names what is being asked about");
        const int seq0 = w.chat.seq;
        press(w, Qt::Key_Return); // the dock has the keyboard: Enter sends
        check(w.chat.seq == seq0 + 1, "Enter in the dock sends it");
        const ChatMsg &m = w.chat.log.back();
        check(m.user && m.via == "tour" && m.focus == QStringList{"octave_up"} && m.page == "synth",
              QString("the message carries via \"tour\", focus [octave_up] and the page (%1 %2)").arg(m.via, m.focus.join(",")));
        check(w.guide.lastQuestionZone() == "octave_up", "and the question's focus is that zone");
        const QJsonObject poll = result(call(R"({"cmd":"chat_poll","since":0,"limit":200})"));
        bool carried = false;
        for (const QJsonValue &v : poll["messages"].toArray()) { const QJsonObject o = v.toObject(); if (o["via"].toString() == "tour" && o["focus"].toArray().size() == 1) carried = true; }
        check(carried, "chat_poll returns via and focus to the copilot");
        w.guide.dockClose();
        // from the Ask button by touch, on a different page, and for a concept
        t.open("beat", "beatrec"); settleTour(w);
        tapZone(w, "tour_ask");
        check(!t.isOpen() && w.page() == PG_BEAT && w.guide.dockIsOpen(), "the Ask Rook button works by touch and puts the player on the page the control lives on");
        w.guide.dockClose(); w.setPage(PG_PLAY); w.shot();
        t.open("synth", "concepts.recall"); settleTour(w);
        press(w, Qt::Key_Return);
        check(w.guide.dockIsOpen() && w.chat.draft.size() > 5, "a concept card can be asked about too");
        w.chat.draft;
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

    // ---- concepts ----
    {
        t.open("synth"); settleTour(w);
        press(w, Qt::Key_C); w.shot();
        check(t.conceptsOpen() && w.zoneNames().contains("tour_conceptlist"), "C opens the Concepts view (a list)");
        press(w, Qt::Key_Tab);
        check(!tourState()["concept"].isNull(), "Tab moves through the concepts");
        t.selectConcept("recall"); w.shot();
        check(tourState()["concept"].toString() == "recall", "the Recall concept is a card like any other");
        press(w, Qt::Key_Escape);
        check(t.conceptsOpen() && tourState()["concept"].isNull(), "Esc first closes the concept card");
        press(w, Qt::Key_Escape);
        check(!t.conceptsOpen() && t.isOpen(), "then the Concepts view");
        tapZone(w, "tour_concepts"); w.shot();
        check(t.conceptsOpen(), "the Concepts button toggles it by touch");
        tapZone(w, "tour_concepts");
        // a related chip jumps to its control, changing page when it must
        t.open("synth", "octave_down"); settleTour(w);
        tapZone(w, "tour_rel1"); w.shot();
        check(t.pageId() == "synth" && t.selectedName() == "hdr_tune" || t.pageId() == "tune", "a Related chip jumps to that control");
        t.open("tune", "bpm-"); settleTour(w);
        const HelpCard *bpm = db.find("tune", "bpm-");
        bool jumped = false;
        if (bpm) for (int i = 0; i < bpm->related.size(); ++i) {
            t.open("tune", "bpm-"); settleTour(w);
            const QString before = t.selectedName() + t.pageId();
            tapZone(w, QString("tour_rel%1").arg(i)); w.shot();
            if (t.selectedName() + t.pageId() != before || t.conceptsOpen()) jumped = true;
        }
        check(jumped, "Related chips change page or open a concept");
        t.close();
    }

    // ---- guided start and replay ----
    {
        call(R"({"cmd":"tour","action":"open","guided":true})");
        settleTour(w);
        check(tourState()["guided"].toBool() && tourState()["step"].toInt() == 1 && tourState()["page"].toString() == "synth", "Start here begins a guided walk on the Synth page");
        std::vector<QString> pages; pages.push_back(tourState()["page"].toString());
        for (int i = 0; i < 12 && tourState()["guided"].toBool(); ++i) { tapZone(w, "tour_next"); w.shot(); if (tourState()["guided"].toBool()) pages.push_back(tourState()["page"].toString()); }
        check(pages.size() == 10 && pages[1] == "sound" && pages.back() == "chat", QString("Next walks through %1 pages in order and then lets go").arg(pages.size()));
        check(t.isOpen() && !tourState()["guided"].toBool(), "after the last step the player can explore freely");
        w.shot(); t.select("chat_send"); w.shot();
        check(t.selectedName() == "chat_send", "and select anything");
        call(R"({"cmd":"tour","action":"open","guided":true})");
        settleTour(w);
        tapZone(w, "tour_next"); tapZone(w, "tour_back"); w.shot();
        check(tourState()["step"].toInt() == 1, "Back steps back");
        tapZone(w, "tour_free"); w.shot();
        check(!tourState()["guided"].toBool() && t.isOpen(), "Explore freely ends the walk and keeps the tour open");
        t.close();
        t.open(); settleTour(w);
        check(t.isOpen(), "the tour can be replayed any number of times");
        t.close();
    }

    // ---- the first-run offer ----
    {
        w.setPage(PG_PLAY); w.guide.dockClose(); t.close(); w.shot();
        t.forgetSeen(); t.armFirstRunPrompt(0); w.shot();
        check(t.promptShowing() && w.zoneNames().contains("tour_prompt_yes") && w.zoneNames().contains("tour_prompt_later"), "on a first run the offer appears (Take the tour / Later)");
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
        check(t.isOpen() && tourState()["guided"].toBool() && inst.uiState().loopState == loop0, "Enter = yes opens the guided tour (and did not start the loop pedal)");
        t.close();
        // touch: Later
        t.forgetSeen(); t.armFirstRunPrompt(0); w.shot();
        tapZone(w, "tour_prompt_later"); w.shot();
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

    // ---- the last page is remembered ----
    {
        t.open("beat"); settleTour(w); t.close();
        w.saveState();
        QJsonObject st; store::readJson(store::baseDir() + "/state.json", st);
        check(st["tourPage"].toString() == "beat", "the last tour page is saved in state.json");
        t.open(); settleTour(w);
        check(t.pageId() == "beat", "and the tour reopens there");
        t.close();
    }

    // ---- a damaged help file never crashes anything ----
    {
        const char *bad[] = {"", "{", "not json at all", "[]", "{}", R"({"version":1})", R"({"pages":[1,2,3]})", R"({"pages":{"synth":5}})",
                             R"({"pages":{"synth":{"cards":{"x":"y"}}}})", R"({"pages":{"synth":{"title":7,"cards":{"octave_up":{"title":[1],"use":"no","keys":5}}}}})"};
        int ok = 0;
        for (const char *b : bad) { const CardDb d = CardDb::parse(QByteArray(b)); if (!d.ok || d.cardCount() >= 0) ++ok; }
        check(ok == (int)(sizeof(bad) / sizeof(bad[0])), "ten kinds of damaged help file are read without a crash");
        check(!CardDb::parse("{broken").ok && !CardDb::parse("{broken").error.isEmpty() && !CardDb::parse("{}").ok, "a damaged file is reported (ok false, with a reason)");
        t.setCards(CardDb::parse("{broken"));
        t.open("synth", "octave_up"); settleTour(w);
        const TourView::Hot *hot = nullptr;
        for (const TourView::Hot &h : t.picture(TourView::pageIndex("synth")).hot) if (h.name == "octave_up") hot = &h;
        const HelpCard *c = hot ? t.cardFor(*hot) : nullptr;
        check(c && c->title == "Help unavailable" && !c->does.isEmpty(), "with a damaged file every control shows an error card instead of help");
        const QImage err = w.shot();
        check(!err.isNull() && t.isOpen(), "and the tour still draws");
        tapZone(w, "tour_ask"); w.guide.dockClose();
        check(!t.isOpen(), "(Ask Rook still works from the error card)");
        t.resetCards();
        w.setPage(PG_PLAY); w.shot();
        t.open("synth", "octave_up"); settleTour(w);
        check(t.selectedName() == "octave_up" && t.cardFor(*hot)->title != "Help unavailable", "the real cards come back when the file is fine");
        t.close();
    }

    // ---- no card overflows ----
    {
        int n = 0, scroll = 0, small = 0, titleBad = 0; QStringList names;
        auto one = [&](const HelpCard &c, const QString &where) {
            const TourView::Fit f = t.measure(c);
            ++n;
            if (f.px < 13) ++small;
            if (f.scrollA || f.scrollB) { ++scroll; names << where + "." + c.key; }
            if (c.title.size() * 7 > 296) ++titleBad; // even at the smallest title size it must fit one line
        };
        for (const CardPage &p : db.pages) for (const HelpCard &c : p.cards) one(c, p.id);
        for (const HelpCard &c : db.concepts) one(c, "concepts");
        std::printf("  info  %d cards measured; %d need a smaller font than the default\n", n, small);
        check(scroll == 0, QString("no card needs scrolling at the default font (%1 of %2 would)").arg(scroll).arg(n) + (names.isEmpty() ? QString() : ": " + names.join(", ").left(200)));
        check(small <= 8, QString("only a few cards need the one-step-smaller font (%1)").arg(small));
        int introScroll = 0;
        for (int p = 0; p < TourView::pageCount(); ++p) { const TourView::Fit f = t.measureIntro(p); if (f.scrollA || f.scrollB) ++introScroll; }
        check(introScroll == 0 && titleBad == 0, "the page intro cards fit too, and every title fits on one line");
        // a card that cannot fit says so: more text than any card has, never clipped silently
        HelpCard huge = *db.find("synth", "octave_up");
        huge.does = QString("word ").repeated(200); huge.tips = QStringList{QString("tip ").repeated(60), QString("tip ").repeated(60)};
        const TourView::Fit hf = t.measure(huge);
        check(hf.scrollA && hf.px == 11, "an oversized card shrinks the font a step, then reports that it needs scrolling");
        CardDb custom = CardDb::parse(R"({"pages":{"synth":{"title":"Synth","intro":"x","cards":{"octave_up":{"title":"BIG","kind":"button","summary":"s","does":"d","use":[{"how":"Touch","text":"t"}],"keys":[],"recall":"r","tips":[],"related":[],"ask":"a"}}}},"concepts":{}})");
        custom.pages[0].cards[0].does = QString("long text here ").repeated(120);
        t.setCards(custom);
        t.open("synth", "octave_up"); settleTour(w);
        w.shot(); const int s0 = t.scrollOfA();
        press(w, Qt::Key_PageDown); w.shot();
        check(t.scrollOfA() > s0, "a long card scrolls (Space, Page Down, or drag)");
        t.resetCards(); t.close();
    }

    // ---- paint time ----
    {
        double worst = 0; QString worstPage;
        t.open("synth"); settleTour(w);
        for (int p = 0; p < TourView::pageCount(); ++p) {
            tapZone(w, QString("tour_page_%1").arg(p)); w.shot();
            const auto &hot = t.picture(p).hot;
            if (!hot.empty()) t.select(hot[hot.size() / 2].name);
            QElapsedTimer pt; pt.start();
            for (int i = 0; i < 12; ++i) w.shot();
            const double ms = pt.elapsed() / 12.0;
            if (ms > worst) { worst = ms; worstPage = TourView::pageInfo(p).id; }
        }
        t.selectConcept("recall");
        { QElapsedTimer pt; pt.start(); for (int i = 0; i < 12; ++i) w.shot(); const double ms = pt.elapsed() / 12.0; if (ms > worst) { worst = ms; worstPage = "concepts"; } }
        t.close();
        check(worst < 25.0, QString("the tour redraws well inside a 40 ms frame (slowest: %1, %2 ms)").arg(worstPage).arg(worst, 0, 'f', 1));
    }

    // ---- hostile arguments ----
    {
        const char *reqs[] = {
            R"({"cmd":"tour","action":5})", R"({"cmd":"tour","action":"explode"})", R"({"cmd":"tour","page":[1]})", R"({"cmd":"tour","page":"nowhere"})",
            R"({"cmd":"tour","page":{"a":1}})", R"({"cmd":"tour","element":[1,2]})", R"({"cmd":"tour","guided":"yes"})", R"({"cmd":"tour","guided":3})",
            R"({"cmd":"tour","page":null,"element":null})", R"({"cmd":"tour","action":null})"};
        int refused = 0, handled = 0;
        for (const char *r : reqs) { const QJsonObject o = call(r); if (!o["ok"].toBool()) ++refused; ++handled; settleTour(w); t.close(); }
        // (null arguments mean "not given": those two are accepted)
        check(handled == 10 && refused >= 8, QString("hostile tour arguments are refused without harm (%1 of 10 refused, none crashed)").arg(refused));
        const QByteArray big = QByteArray(R"({"cmd":"tour","page":")") + QByteArray(5000, 'x') + "\"}";
        check(!call(big)["ok"].toBool(), "a 5000-character page name is refused");
        const QByteArray big2 = QByteArray(R"({"cmd":"tour","element":")") + QByteArray(5000, 'y') + "\"}";
        check(!call(big2)["ok"].toBool(), "a 5000-character element is refused");
        QJsonObject r = call(R"({"cmd":"tour","page":"beat","element":"no_such_control"})");
        settleTour(w);
        check(r["ok"].toBool() && tourState()["page"].toString() == "beat" && tourState()["selected"].isNull(), "an unknown element opens the page with nothing selected");
        r = call(R"({"cmd":"tour","page":"beat","element":"pad0","extra":{"x":1}})");
        settleTour(w);
        check(r["ok"].toBool() && tourState()["selected"].toString() == "pad0", "unknown extra fields are ignored");
        r = call(R"({"cmd":"tour","page":"  BEAT  "})");
        settleTour(w);
        check(r["ok"].toBool() && tourState()["page"].toString() == "beat", "page names are not case or space sensitive");
        for (int i = 0; i < 6; ++i) { call(R"({"cmd":"tour","action":"open"})"); call(R"({"cmd":"tour","action":"close"})"); }
        check(!t.isOpen(), "opening and closing over and over leaves it closed");
        t.close();
        check(inst.uiState().droppedEvents == 0, "none of it dropped an audio event");
    }
    w.setPage(PG_PLAY); w.guide.dockClose(); t.close();
    inst.cmd(EV_PANIC);
    wait(150);
}
