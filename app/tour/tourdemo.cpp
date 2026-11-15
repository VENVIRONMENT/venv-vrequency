// venv-vrequency — the tour's demo: sets up the scratch window and a demo state, then draws every page.
#include "app/tour/tour.h"
#include "app/tour/tourdemo.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include <QThread>
#include <array>

using namespace td;

// Draws every tour page off-screen in a scratch copy of the app (see tour.h).
void TourDemo::build(std::vector<TourView::Pic> &pics, SinkProbe *sink) {
    Instrument inst(48000.0f);
    AudioOut audio; // never started: the System page only reads its name
    MainWindow w(inst, audio, true, sink);
    Rig rig{inst, w};
    pics.assign(TourView::picCount(), TourView::Pic());

    // demo favourites and sessions, so those lists are not empty
    w.sounds.favs.clear();
    for (int i = 0; i < kExtraCount; ++i) { store::NamedPatch np; np.file = "demo"; np.patch = extraPatch(i); w.sounds.favs.push_back(np); }
    w.systemPage.sessions = QStringList{"/demo/jam-friday.json", "/demo/slow-pad.json", "/demo/beat-practice.json"};

    // ---- a beat, a held chord and a loop ----
    inst.set(S_VOLUME, 0.5f);
    static const char *pat[kDrums] = {"x......x..x.....", "....x.......x...", "x.x.x.x.x.x.x.x.", "...............x",
                                      "............x...", "..x......x......", "..............x.", "................"};
    for (int d = 0; d < kDrums; ++d)
        for (int st = 0; st < kDrumSteps; ++st) inst.beatDo(BT_STEP, d, st, pat[d][st] == 'x');
    inst.set(S_BEATVOL, 0.7f);
    pump(inst, 60);
    SessionData sd; inst.snapshot(sd);
    sd.pads = extraPatch(1);
    sd.holdChord = 5;
    sd.loopBeats = 8;
    sd.layers = {melody(extraPatch(0), {{0, 1.5, 64}, {1.5, 1.5, 67}, {3.5, 0.5, 69}, {4, 2, 72}, {6, 1, 71}, {7, 0.9, 67}})};
    inst.restore(sd);
    pump(inst, 80);
    inst.cmd(EV_LOOP, LC_PLAYSTOP);
    pump(inst, 80);

    playPages(rig, pics);
    for (int i = 0; i < (int)pics.size(); ++i) extras(TourView::picPage(i), pics[i]);
}

void tourBuildPictures(std::vector<TourView::Pic> &pics, SinkProbe *sink) { TourDemo::build(pics, sink); }
