// venv-vrequency self-test — tour: nothing done in the tour reaches the synth.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

using namespace td;

namespace selftest {
namespace {
// the state a player could hear or feel, as bytes: settings, sounds, backing, loop, pulse pads
QByteArray synthBytes(MainWindow &w, Instrument &inst) {
    SessionData sd; inst.snapshot(sd);
    QJsonObject o = store::sessionToJson(sd);
    QJsonObject extra;
    extra["page"] = w.page();
    extra["armed"] = w.armedModes();
    extra["lead"] = QString::fromUtf8(w.sounds.lead.name);
    extra["undo"] = (int)w.sounds.undo.size();
    extra["favs"] = QDir(store::baseDir() + "/favourites").entryList(QDir::Files).size();
    extra["help"] = w.helpPage.sub;
    o["_w"] = extra;
    return QJsonDocument(o).toJson(QJsonDocument::Compact);
}
} // namespace

// nothing in the tour reaches the synth
void tourReachesNothing(TourContext &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; TourView &t = ctx.t; const auto &check = ctx.check;
    inst.cmd(EV_PANIC); wait(150);
    w.setPage(PG_SOUND); w.shot();
    const QByteArray before = synthBytes(w, inst);
    QJsonObject sb = result(ctx.call(R"({"cmd":"state"})")); sb.remove("tour"); sb.remove("help_focus");
    const QByteArray stateBefore = QJsonDocument(sb).toJson(QJsonDocument::Compact);
    const QByteArray paramsBefore = QJsonDocument(result(ctx.call(R"({"cmd":"get_params"})"))).toJson(QJsonDocument::Compact);
    const uint64_t dropped0 = inst.uiState().droppedEvents;
    int sounded = 0, taps = 0, keys = 0;
    t.open("synth"); settleTour(w);
    for (int p = 0; p < TourView::pageCount(); ++p) {
        tourTap(w, zn::at(zn::tour_page, p)); w.shot();
        for (int v = 0; v < t.variantsOnPage(); ++v) {
            if (v) press(w, Qt::Key_V);
            w.shot();
            const auto &hot = t.picture(TourView::variantPic(p, v)).hot;
            for (const TourView::Hot &h : hot) {
                const QRectF zr = w.zoneRect(zn::tour_hot + h.name);
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
    QJsonObject sa = result(ctx.call(R"({"cmd":"state"})")); sa.remove("tour"); sa.remove("help_focus");
    check(taps > 300 && keys > 150, QString("a long sequence of tour taps and keys ran (%1 taps, %2 key presses)").arg(taps).arg(keys));
    check(sounded == 0 && heldInTour == 0 && inst.uiState().voices == 0, "no note ever sounded while the tour was open");
    check(before == after, "the synth's state (settings, sounds, backing, loop, pulse pads, page, armed modes) is byte-identical before and after");
    check(QJsonDocument(sa).toJson(QJsonDocument::Compact) == stateBefore, "and so is the state the copilot reads");
    check(QJsonDocument(result(ctx.call(R"({"cmd":"get_params"})"))).toJson(QJsonDocument::Compact) == paramsBefore, "and the sound parameters");
    check(inst.uiState().droppedEvents == dropped0 && w.armedModes().isEmpty() && w.page() == PG_SOUND, "no event was dropped, nothing was armed, the page is the one it was");
    w.setPage(PG_PLAY); w.shot();
}
} // namespace selftest
