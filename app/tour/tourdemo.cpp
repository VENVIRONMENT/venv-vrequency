// venv-vrequency — the tour's pictures. A scratch copy of the window (no files, no timers) is given a
// demo state and drawn page by page; each page's real zones become hotspots, with a few extra
// named hotspots for things that are on screen but are not controls (the level strip, the scope readouts...).
#include "app/tour/tour.h"
#include "audio/audio.h"
#include "instrument.h"
#include "app/shell/mainwindow.h"
#include "presets.h"
#include "sounds.h"
#include <QApplication>
#include <QDir>
#include <QThread>
#include <algorithm>
#include <array>
#include <cstdio>
#include <set>

using namespace td;

namespace {
// The engine has no audio thread here: this runs it by hand, faster than real time.
void pump(Instrument &inst, int ms) {
    static float l[256], r[256];
    for (int n = ms * 48 / 256 + 1; n > 0; --n) inst.process(l, r, 256);
}

struct Rig {
    Instrument &inst;
    MainWindow &w;
    void key(int k, bool down) { w.handleKey(k, down); }
    void press(int k) { w.shot(); key(k, true); key(k, false); }  // the window is drawn first, as it would be on a screen: its zones and readouts are current
    // Escape drops what is in hand or closes a panel; on the Track page with nothing to drop it would leave for the Synth
    void esc() { if (w.clipInHand() || w.chopping() || w.pickingSound() || w.mixing()) press(Qt::Key_Escape); }
    void ctrl(int k) { key(Qt::Key_Control, true); press(k); key(Qt::Key_Control, false); }
    void tap(const QString &zone) {
        w.shot();
        const QRectF r = w.zoneRect(zone);
        w.pointer(7, r.center(), PH_PRESS);
        w.pointer(7, r.center(), PH_RELEASE);
    }
    void touch(QPointF a) { w.shot(); w.pointer(3, a, PH_PRESS); w.pointer(3, a, PH_RELEASE); }
    void settle(int frames, int ms = 40) { for (int i = 0; i < frames; ++i) { pump(inst, ms); w.shot(); } }
};

bool hiddenZone(const QString &n) { return n.startsWith("dock") || n == "ticker" || n == "keytestblock" || n.startsWith("help_row_"); }

} // namespace

// The part of the picture-drawing that needs the window's private state.
struct TourDemo {
// the controls of whatever the scratch window shows now
static void capture(Rig &rig, int page, TourView::Pic &pic) {
    rig.w.tickerUntil = 0; rig.w.toastUntil = 0;
    rig.settle(3);
    pic.img = rig.w.shot();
    pic.small = pic.img.scaled(380, 380, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    std::set<QString> seen;
    size_t from = 0;
    if (page == 11) // the Key test panel swallows every touch but its own Close button
        for (size_t i = 0; i < rig.w.ui.zones().size(); ++i) if (rig.w.ui.zones()[i].name == "keytestblock") from = i + 1;
    for (size_t i = from; i < rig.w.ui.zones().size(); ++i) {
        const auto &z = rig.w.ui.zones()[i];
        if (hiddenZone(z.name) || seen.count(z.name) || z.r.width() < 4 || z.r.height() < 4) continue;
        seen.insert(z.name);
        TourView::Hot h;
        h.name = z.name; h.r = z.r;
        pic.hot.push_back(h);
    }
}

static LayerData melody(const Patch &sound, std::initializer_list<std::array<double, 3>> notes) {
    LayerData ld; ld.patches.push_back(sound);
    uint32_t seq = 0;
    for (const auto &n : notes) {
        LoopEv a; a.beat = n[0]; a.op = OP_ON; a.slot = 0; a.a = (float)n[2]; a.b = 1; a.seq = seq++;
        LoopEv b; b.beat = n[0] + n[1]; b.op = OP_OFF; b.slot = 0; b.a = b.b = 0; b.seq = seq++;
        ld.ev.push_back(a); ld.ev.push_back(b);
    }
    return ld;
}

// extra named controls: things on screen that are not zones (their cards live under `concepts`)
static void extras(int page, TourView::Pic &pic) {
    auto add = [&](const char *name, QRectF r, const char *ref) { TourView::Hot h; h.name = name; h.r = r; h.ref = ref; h.real = false; pic.hot.push_back(h); };
    switch (page) {
    case 4: // synth
        add("level_strip", QRectF(16, 76, 688, 12), "concepts.level-strip");
        add("scope_readouts_top", QRectF(18, 93, 470, 18), "concepts.scope-readouts");
        add("scope_readouts_bottom", QRectF(18, 298, 480, 18), "concepts.scope-readouts");
        add("string_brackets", QRectF(235, 400, 48, 40), "concepts.string-brackets");
        break;
    case 7: add("step_grid", QRectF(16, 80, 688, 130), "concepts.step-grid"); break;
    case 9: add("listen_result", QRectF(240, 494, 456, 72), "concepts.listen-result"); break;
    case 10: add("status_box", QRectF(16, 424, 688, 206), "concepts.status-box"); break;
    case 12: add("key_list_rows", QRectF(16, 100, 688, 474), "concepts.help-rows"); break;
    default: break;
    }
}
static void build(std::vector<TourView::Pic> &pics) {
    Instrument inst(48000.0f);
    AudioOut audio; // never started: the System page only reads its name
    MainWindow w(inst, audio, true);
    Rig rig{inst, w};
    pics.assign(TourView::picCount(), TourView::Pic());
    auto P = [&](const char *id) -> TourView::Pic & { return pics[TourView::pageIndex(id)]; };

    // demo favourites and sessions, so those lists are not empty
    w.favs.clear();
    for (int i = 0; i < kExtraCount; ++i) { store::NamedPatch np; np.file = "demo"; np.patch = extraPatch(i); w.favs.push_back(np); }
    w.sessions = QStringList{"/demo/jam-friday.json", "/demo/slow-pad.json", "/demo/beat-practice.json"};

    // ---- a beat, a progression and a loop ----
    inst.set(S_VOLUME, 0.5f);
    static const char *pat[kDrums] = {"x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "...............x",
                                      "............x...", "..x......x......", "..............x.", "................"};
    for (int d = 0; d < kDrums; ++d)
        for (int st = 0; st < kDrumSteps; ++st) inst.beatDo(BT_STEP, d, st, pat[d][st] == 'x');
    inst.set(S_BEATVOL, 0.7f);
    pump(inst, 60);
    SessionData sd; inst.snapshot(sd);
    sd.prog[0] = 5; sd.prog[1] = 3; sd.prog[2] = 0; sd.prog[3] = 4; sd.progLen = 4; sd.settings.chordBeats = 4;
    sd.backing = extraPatch(1);
    sd.loopBeats = 8;
    sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
    inst.restore(sd);
    pump(inst, 80);
    inst.cmd(EV_LOOP, LC_PLAYSTOP);
    pump(inst, 80);

    // ---- the playing pages, with two notes held (a slide) so the scope and the strings are lit ----
    w.setPage(PG_PLAY);
    rig.key(Qt::Key_A, true); pump(inst, 250);
    rig.key(Qt::Key_F, true);
    rig.settle(30);
    capture(rig, 4, P("synth"));
    w.setPage(PG_SOUND); rig.settle(3);
    capture(rig, 5, P("sound"));
    w.deepEdit = true; w.deepPage = 0; rig.settle(2);
    capture(rig, 6, P("sound-deep"));
    for (int dp = 1; dp < 4; ++dp) { w.deepPage = dp; rig.settle(2); capture(rig, 13 + dp, pics[13 + dp]); } // the other parameters
    w.deepEdit = false;
    rig.key(Qt::Key_F, false); rig.key(Qt::Key_A, false);
    w.setPage(PG_BEAT); rig.settle(2);
    for (int i = 0; i < 7; ++i) { // eight kept beats: the row pages, with More
        w.beatNew(); rig.settle(2);
        inst.beatDo(BT_STEP, D_KICK, 0, 1); rig.settle(2);
    }
    w.beatSwitchTo(0); rig.settle(3);
    inst.beatDo(BT_PLAYSTOP); pump(inst, 700);
    inst.beatDo(BT_PLAYSTOP); pump(inst, 60);
    QThread::msleep(700);                                                     // the pads that just sounded fade
    capture(rig, 7, P("beat"));
    w.setPage(PG_BACK); rig.settle(2);
    capture(rig, 8, P("backing"));
    w.setPage(PG_TUNE); rig.settle(2);
    w.listenResult = td::ListenResult();
    w.listenResult.ok = true;
    w.listenResult.keys[0].root = 9; w.listenResult.keys[0].minor = true; w.listenResult.keys[0].score = 0.8f;
    w.listenResult.keyConfidence = 0.7f; w.listenResult.bpm = 100; w.listenResult.bpmConfidence = 0.7f;
    w.listenState = 2; rig.settle(2);
    capture(rig, 9, P("tune"));
    w.setPage(PG_SYS); rig.settle(2);
    capture(rig, 10, P("system"));
    w.keyTest = true;
    rig.key(Qt::Key_Z, true); rig.key(Qt::Key_S, true); rig.key(Qt::Key_E, true);
    pump(inst, 60);
    capture(rig, 11, P("key-test"));
    rig.key(Qt::Key_E, false); rig.key(Qt::Key_S, false); rig.key(Qt::Key_Z, false);
    w.keyTest = false;
    w.setPage(PG_HELP); rig.settle(2);
    capture(rig, 12, P("help"));
    w.chatAdd("rook", "Hi. I can hear A minor pentatonic at 100 bpm. Want a backing under it?", "ask", false);
    w.chatAdd("you", "yes, something slow with a soft pad", "say", true);
    w.chatAdd("rook", "On it: I-vi-IV-V on sound 3, tempo down to 84. Play over it and I will listen.", "say", false);
    w.chatSeen();
    w.setPage(PG_CHAT); w.chatDraft.clear(); rig.settle(2);
    capture(rig, 13, P("chat"));
    inst.cmd(EV_LOOP, LC_PLAYSTOP);

    // ---- the Track page: two beats, a progression and two loops arranged on the lanes ----
    inst.snapshot(sd);
    sd.prog[0] = 5; sd.prog[1] = 3; sd.prog[2] = 0; sd.prog[3] = 4; sd.progLen = 4; sd.settings.chordBeats = 4;
    sd.backing = extraPatch(1);
    sd.loopBeats = 8;
    sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
    inst.restore(sd); pump(inst, 80);
    w.setPage(PG_TRACK); rig.settle(3);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 1, Chords 1, Loop 1
    inst.beatDo(BT_ERASE, D_HAT); inst.beatDo(BT_STEP, D_CLAP, 4, 1); inst.beatDo(BT_STEP, D_TOM, 13, 1); pump(inst, 80);
    inst.snapshot(sd);
    sd.loopBeats = 16;
    sd.layers = {melody(extraPatch(1), {{0, 3, 52}, {4, 2, 55}, {6, 2, 57}, {8, 3.5, 60}, {12, 1, 59}, {13, 1, 57}, {14, 1.9, 55}}),
                 melody(extraPatch(1), {{2, 1, 76}, {7, 0.5, 79}, {10, 2, 81}, {15, 0.5, 79}})};
    inst.restore(sd); pump(inst, 80);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 2, Loop 2
    inst.beatDo(BT_STEP, D_COWBELL, 7, 1); pump(inst, 80); rig.settle(2);
    rig.tap("track_keep"); pump(inst, 60);                                   // Beat 3: more clips than the shelf shows, so it pages
    rig.settle(2);
    w.shelfPage = 0;                                                        // the keys 1-5 take the clips of the first page
    rig.esc();
    rig.press(Qt::Key_Return);
    rig.press(Qt::Key_1); for (int i = 0; i < 4; ++i) rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_4); rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_1); for (int i = 0; i < 3; ++i) rig.ctrl(Qt::Key_V);
    rig.press(Qt::Key_Return); rig.press(Qt::Key_2); rig.ctrl(Qt::Key_V); rig.ctrl(Qt::Key_V);
    const QRectF lanes = w.zoneRect("track_lanes");
    auto cell = [&](int lane, double bar) { return QPointF(lanes.left() + (bar + 0.5) * lanes.width() / 8.0, lanes.top() + 2 + lane * 70 + 33); };
    rig.press(Qt::Key_3); rig.touch(cell(LN_LOOPA, 2)); rig.touch(cell(LN_LOOPA, 6));
    rig.press(Qt::Key_5); rig.touch(cell(LN_LOOPB, 4));
    rig.esc();
    rig.touch(cell(LN_BEAT, 0)); rig.ctrl(Qt::Key_J); rig.ctrl(Qt::Key_J); rig.ctrl(Qt::Key_J);
    rig.touch(cell(LN_LOOPA, 2));
    rig.press(Qt::Key_Return);
    rig.key(Qt::Key_Shift, true); rig.press(Qt::Key_Right); rig.press(Qt::Key_Right); rig.key(Qt::Key_Shift, false);
    rig.press(Qt::Key_C);
    rig.press(Qt::Key_3);
    rig.touch(cell(LN_LOOPA, 2));
    rig.settle(2);
    w.toastUntil = 0;
    capture(rig, 0, P("track"));
    rig.esc();
    rig.press(Qt::Key_C);
    rig.press(Qt::Key_E);
    rig.settle(2);
    const QRectF strip = w.zoneRect("chop_strip");
    for (double f : {0.25, 0.5, 0.75}) rig.touch(QPointF(strip.left() + strip.width() * f + 2, strip.center().y()));
    rig.touch(QPointF(strip.left() + strip.width() * 0.6, strip.center().y()));
    w.toastUntil = 0;
    capture(rig, 1, P("track-chop"));
    rig.tap("chop_done");
    rig.press(Qt::Key_I); rig.press(Qt::Key_Right); rig.press(Qt::Key_1);
    rig.press(Qt::Key_Space);                                                // not playing in the picture
    w.toastUntil = 0;
    capture(rig, 2, P("track-sound"));
    rig.press(Qt::Key_Return);
    rig.touch(cell(LN_BEAT, 5)); rig.press(Qt::Key_I);                         // a beat clip: the Drums kinds
    w.toastUntil = 0;
    capture(rig, 17, pics[17]);
    rig.press(Qt::Key_Return);
    rig.touch(cell(LN_LOOPA, 2));
    inst.cmd(EV_PANIC); pump(inst, 60);
    rig.press(Qt::Key_X);
    rig.press(Qt::Key_4); rig.press(Qt::Key_M);
    rig.press(Qt::Key_2); for (int i = 0; i < 3; ++i) rig.press(Qt::Key_Down);
    rig.press(Qt::Key_3); rig.press(Qt::Key_Up); rig.press(Qt::Key_Up);
    rig.press(Qt::Key_1);
    w.toastUntil = 0;
    capture(rig, 3, P("track-mixer"));
    inst.cmd(EV_PANIC); pump(inst, 30);

    for (int i = 0; i < (int)pics.size(); ++i) extras(TourView::picPage(i), pics[i]);
}

};

void tourBuildPictures(std::vector<TourView::Pic> &pics) { TourDemo::build(pics); }

int tourDumpZones(const QString &picDir) {
    std::vector<TourView::Pic> pics;
    tourBuildPictures(pics);
    if (!picDir.isEmpty()) {
        QDir().mkpath(picDir);
        for (int i = 0; i < (int)pics.size(); ++i) pics[i].img.save(QString("%1/%2%3.png").arg(picDir, TourView::pageInfo(TourView::picPage(i)).id).arg(i >= TourView::pageCount() ? QString("-v%1").arg(i) : QString()));
    }
    std::printf("# every real zone of every tour page, from the app's own screens (vrequency --dump-zones)\n");
    std::set<QString> lines;
    for (int i = 0; i < (int)pics.size(); ++i) {
        for (const TourView::Hot &h : pics[i].hot) if (h.real) lines.insert(QString("%1 %2").arg(TourView::pageInfo(TourView::picPage(i)).id, h.name));
    }
    for (const QString &l : lines) std::printf("%s\n", l.toUtf8().constData());
    return 0;
}
