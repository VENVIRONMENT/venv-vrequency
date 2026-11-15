// venv-vrequency — the window: construction, the frame timer, closing, toasts and tap tempo.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/store/store.h"
#include <QCloseEvent>
#include <memory>

using namespace td;

MainWindow::MainWindow(Instrument &i, AudioOut &a, bool scratch, SinkProbe *sharedSink)
    : Shell(i, a), scratchWindow(scratch), sink(sharedSink ? *sharedSink : ownSink) {
    setWindowTitle("venv-vrequency");
    setAttribute(Qt::WA_AcceptTouchEvents);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::StrongFocus);
    resize(720, 720);
    clock.start();
    pageOf[PG_CHAT] = &chatPage;
    pageOf[PG_PLAY] = &synthPage;
    pageOf[PG_SOUND] = &soundPage;
    pageOf[PG_PULSE] = &pulsePage;
    pageOf[PG_FREQ] = &freqPage;
    pageOf[PG_BACK] = &backingPage;
    pageOf[PG_TUNE] = &tunePage;
    pageOf[PG_SYS] = &systemPage;
    pageOf[PG_HELP] = &helpPage;
    chat.attach(guide);
    chat.onUserMessage = [this] { emit userChatArrived(); };
    guide.rowLabels = &helpPage.desc;

    if (!scratch) store::ensureDirs();
    km = scratch ? KeyMap::defaults() : KeyMap::loadOrCreate(store::baseDir() + "/keymap.json");
    SessionData sd;
    loadState(sd);                                           // the state file, if there is one
    sd.lead = sounds.lead; sd.backing = sounds.lead;
    inst.restore(sd);
    saveTimer.setSingleShot(true);
    connect(&saveTimer, &QTimer::timeout, this, [this] { saveState(); }); // an edit is on disk a moment later
    if (scratch) {
        sounds.extrasSeeded = kExtraCount;
        return;                                              // a picture-drawing copy: no timers, no probes, no files
    }
    if (sounds.extrasSeeded < kExtraCount) sounds.addExtraSounds(sounds.extrasSeeded); // once: deleting one keeps it gone
    refreshLists();
    tourView = std::make_unique<TourView>(*this);

    connect(&frameTimer, &QTimer::timeout, this, [this] {
        // 25 fps while anything moves; a slow tick when the instrument is idle (battery).
        // Key and touch input call update() themselves, so response stays instant.
        static int skip = 0;
        UiState s = inst.uiState();
        bool busy = s.voices > 0 || ui.touching() || s.backPlaying || s.loopState != L_EMPTY || s.peak > 0.002f ||
                    clock.elapsed() < toastUntil + 200 || tunePage.listener.state == 1 || recorder.recording() || !keys.held.isEmpty() ||
                    s.pulsing || s.freqSounding || recorder.playingBack() || clock.elapsed() < chat.tickerEnds() + 200 || pg == PG_CHAT || guide.dockOpen || clock.elapsed() < guide.hl.until + 200 || tourOpen() || (tourView && tourView->promptShowing());
        if (busy || ++skip >= 12) { skip = 0; update(); }
    });
    frameTimer.start(40);
    sink.start();
}

MainWindow::~MainWindow() {
    recorder.shutdown();
}

void MainWindow::closeEvent(QCloseEvent *e) {
    saveState();
    inst.cmd(EV_PANIC);
    e->accept();
}

void MainWindow::refreshLists() {
    sounds.favs = store::listPatches("favourites");
    systemPage.sessions = store::listFiles("sessions", "json");
}

void MainWindow::toast(const QString &s, int ms) {
    toastText = s;
    toastUntil = clock.elapsed() + ms;
}

void MainWindow::tapTempo() {
    qint64 now = clock.elapsed();
    if (!taps.empty() && now - taps.back() > 2000) taps.clear();
    taps.push_back(now);
    if (taps.size() > 6) taps.erase(taps.begin());
    if (taps.size() >= 2) {
        double avg = (taps.back() - taps.front()) / double(taps.size() - 1);
        if (avg > 0) set(S_BPM, (float)(60000.0 / avg));
    }
}
