// venv-vrequency — the 720x720 instrument panel. One custom-painted widget with a
// small immediate-mode layer: every frame draws the controls and registers
// their touch zones, so drawing and hit-testing can never disagree.
#pragma once
#include "audio/audio.h"
#include "audio/capture.h"
#include "core/analysis/analysis.h"
#include "core/presets/presets.h"
#include "app/store/store.h"
#include "app/chat/chat.h"
#include "app/guide/guide.h"
#include "app/pages/backing/backing_page.h"
#include "app/pages/chat/chat_page.h"
#include "app/pages/help/help_page.h"
#include "app/pages/pulse/pulse_page.h"
#include "app/pages/sound/sound_page.h"
#include "app/pages/synth/chord_pads.h"
#include "app/pages/synth/synth_page.h"
#include "app/pages/system/system_page.h"
#include "app/pages/tune/tune_page.h"
#include "app/shell/recorder.h"
#include "app/shell/shell.h"
#include "app/shell/sink_probe.h"
#include "app/sounds/sounds.h"
#include <QElapsedTimer>
#include <QFont>
#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QSet>
#include <QTimer>
#include <QWidget>
#include <atomic>
#include <functional>
#include <mutex>
#include <memory>
#include <thread>
#include <vector>

class QPainter;
class ControlServer;
class TourView;

class MainWindow : public QWidget, public Shell {
    Q_OBJECT
    friend struct TourDemo;
public:
    // `scratch`: an off-screen copy used only to draw the tour's pictures: it reads and writes no files
    MainWindow(td::Instrument &inst, AudioOut &audio, bool scratch = false, SinkProbe *sharedSink = nullptr);
    ~MainWindow() override;

    Recorder recorder{*this};
    Sounds sounds{*this};
    Chat chat{*this};
    Guide guide{*this, chat};
    ChatPage chatPage{*this, chat};
    ChordPads chordPads{*this};              // shared by the Synth and Backing pages
    SynthPage synthPage{*this, chordPads};
    SoundPage soundPage{*this};
    PulsePage pulsePage{*this};
    BackingPage backingPage{*this, chordPads};
    TunePage tunePage{*this};
    SystemPage systemPage{*this};
    HelpPage helpPage{*this, chat, guide};
    Page *pageOf[PG_COUNT] = {};             // every page, by PageId

    // --- also used by the self-test ---
    void setPage(int p) { goPage(p); update(); }
    void goPage(int p) override {
        if (p == pg) return;
        pg = p;
        for (Page *q : pageOf) if (q) q->disarm();
        update();
    }
    void toast(const QString &s, int ms = 1800) override;
    void redraw() override { update(); }
    void refreshLists() override;
    Sounds &soundShelf() override { return sounds; }
    const Recorder &recorderNow() const override { return recorder; }
    bool noAudioDevice() const override { return sink.dummy; }
    void saveSoon() override { saveTimer.start(2500); }
    int page() const override { return pg; }
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const { return ui.zoneRect(name); }
    QStringList zoneNames() const { return ui.zoneNames(); }
    QImage shot();
    bool saveSession(const QString &path) override;
    bool loadSession(const QString &path) override;
    const SinkProbe &sinkNow() const override { return sink; }
    void saveState() override;
    void perform(int act, bool shift = false) override;
    void setKeyTest(bool on) { keys.testOpen = on; }
    QString armedModes() const { QStringList l; if (soundPage.favDelete) l << "delete"; if (sounds.slotSave) l << "store"; return l.join(","); }
    void armAllModes() { soundPage.favDelete = sounds.slotSave = true; }
    QString zoneAt(QPointF c) const { return ui.zoneAt(c); }
    int maxKeysHeld() const { return keys.maxHeld; }
    QString lastToast() const { return toastText; }
    // --- the catalogue of zones (app/shell/zone_catalogue.cpp) ---
    struct ZoneInfo { QString name; QRectF r; QString label; bool covered = false; };
    void refreshZones() { shot(); }                            // draw once so zoneRect/zoneNames are current
    QVector<ZoneInfo> zoneCatalogue(int page, int helpSub = -1); // every zone of a page (draws it off-screen, then puts things back)

    // --- the interactive tour (app/tour.cpp) ---
    TourView &tour() { return *tourView; }
    bool tourOpen() const;
    bool tourSuspended = false;                              // set while the real page is drawn for a catalogue
    // what the tour remembers between runs (kept in the state file), and the output device it shows
    bool tourSeen() const { return tourSeenFlag; }
    void setTourSeen(bool v) { tourSeenFlag = v; }
    QString tourLastPage() const { return tourLastPageName; }
    void setTourLastPage(const QString &p) { tourLastPageName = p; }
    SinkProbe &outputSink() { return sink; }
    void tourAsk(const QString &page, const QString &zone, const QString &text); // Ask Rook from a tour card

signals:
    void userChatArrived();

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    bool event(QEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    QTimer frameTimer;

    // ui state
    int pg = PG_PLAY;
    QString toastText; qint64 toastUntil = 0;
    bool scratchWindow = false;
    bool tourSeenFlag = false;                               // the player has answered the first-run offer
    QString tourLastPageName;                                // the tour page last looked at (saved)
    std::unique_ptr<TourView> tourView;
    // synthetic notes from the control socket: string -> ids held
    std::vector<qint64> taps;
    int seenSinkGeneration = -1;
    SinkProbe ownSink;
    SinkProbe &sink;                         // a scratch copy shows the real window's output device

    QTimer saveTimer;                       // an edit is on disk a moment later
    QPixmap facePix;  // scan lines and corner shading over the whole screen

    QPointF toCanvas(QPointF widgetPos) const;

    void drawAll(QPainter &p, qreal W, qreal H);
    void paintTabs();
    void paintToast();
    void loadState(td::SessionData &sd);     // the state file, read at start-up

    // actions
    void tapTempo();
    void updateBend();
    void nudgeCutoff(double octaves);   // the lead's filter cutoff, by octaves (window_perform.cpp)
    void sweepFilter();                 // every frame: Ctrl+C / Ctrl+X held
    void releaseAllKeys();
};
