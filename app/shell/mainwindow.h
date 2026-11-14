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
#include "app/pages/chat/chat_page.h"
#include "app/pages/sound/sound_page.h"
#include "app/pages/synth/chord_pads.h"
#include "app/pages/synth/synth_page.h"
#include "app/pages/track/track_page.h"
#include "app/pages/tune/listener.h"
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
#include <deque>
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
    friend class ControlServer;
    friend class TourView;
    friend struct TourDemo;
public:

    // `scratch`: an off-screen copy used only to draw the tour's pictures: it reads and writes no files
    MainWindow(td::Instrument &inst, AudioOut &audio, bool scratch = false, SinkProbe *sharedSink = nullptr);
    ~MainWindow() override;

    Recorder recorder{*this};
    Sounds sounds{*this};
    Listener listener{*this};
    Chat chat{*this};
    Guide guide{*this, chat};
    ChatPage chatPage{*this, chat};
    TrackPage trackPage{*this};
    ChordPads chordPads{*this};              // shared by the Synth and Backing pages
    SynthPage synthPage{*this, chordPads};
    SoundPage soundPage{*this};
    Page *pageOf[PG_COUNT] = {};             // the pages that are classes, by PageId (the rest are still methods here)

    // --- also used by the self-test ---
    void setPage(int p) { goPage(p); update(); }
    void goPage(int p) override {
        if (p == pg) return;
        pg = p; sounds.slotSave = false; beatErase = false;
        for (Page *q : pageOf) if (q) q->disarm();
        update();
    }
    void toast(const QString &s, int ms = 1800) override;
    void redraw() override { update(); }
    void refreshLists() override;
    Sounds &soundShelf() override { return sounds; }
    const Recorder &recorderNow() const override { return recorder; }
    bool noAudioDevice() const override { return sink.dummy; }
    void setLastRecording(const QString &path) override { recorder.setLastPath(path); }
    void saveSoon() override { saveTimer.start(2500); }
    int page() const override { return pg; }
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const { return ui.zoneRect(name); }
    QStringList zoneNames() const { return ui.zoneNames(); }
    QImage shot();
    bool saveSession(const QString &path);
    bool loadSession(const QString &path);
    void saveState() override;
    void perform(int act, bool shift = false) override;
    void setKeyTest(bool on) { keys.testOpen = on; }
    QString armedModes() const { QStringList l; if (soundPage.favDelete) l << "delete"; if (sounds.slotSave) l << "store"; if (beatErase) l << "erase"; if (trackPage.armed()) l << "remove"; return l.join(","); }
    void armAllModes() { soundPage.favDelete = sounds.slotSave = beatErase = true; trackPage.armAll(); }
    QString zoneAt(QPointF c) const { return ui.zoneAt(c); }
    int maxKeysHeld() const { return keys.maxHeld; }
    QString lastToast() const { return toastText; }
    int keptBeats() const { return (int)beatSlots.size(); }
    int keptBeatShown() const { return beatCur; }
    QString keptBeatName(int i) const { return i >= 0 && i < (int)beatSlots.size() ? beatSlots[i].name : QString(); }
    // --- the catalogue of zones (app/shell/zone_catalogue.cpp) ---
    struct ZoneInfo { QString name; QRectF r; QString label; bool covered = false; };
    void refreshZones() { shot(); }                            // draw once so zoneRect/zoneNames are current
    QVector<ZoneInfo> zoneCatalogue(int page, int helpSub = -1); // every zone of a page (draws it off-screen, then puts things back)
    int helpSubPage() const { return helpPage; }
    void setHelpSubPage(int i) { helpPage = qBound(0, i, 3); update(); }

    // --- the interactive tour (app/tour.cpp) ---
    TourView &tour() { return *tourView; }
    bool tourOpen() const;
    bool tourSuspended = false;                              // set while the real page is drawn for a catalogue
    void tourAsk(const QString &page, const QString &zone, const QString &text); // Ask Rook from a tour card

signals:
    void userChatArrived();

public:

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
    int helpPage = 0;
    QString toastText; qint64 toastUntil = 0;
    bool scratchWindow = false;
    bool tourSeenFlag = false;                               // the player has answered the first-run offer
    QString tourLastPage;                                    // the tour page last looked at (saved)
    std::unique_ptr<TourView> tourView;
    QString helpPrefill;
    QString helpSel;                                         // the Help row last tapped
    QHash<QString, QString> helpDesc;                        // Help row zone -> its description
    qint64 helpAskAt = 0;
    // synthetic notes from the control socket: string -> ids held
    int nextNoteId = 0x400;
    std::vector<qint64> taps;
    QStringList sessions;
    int seenSinkGeneration = -1;
    SinkProbe ownSink;
    SinkProbe &sink;                         // a scratch copy shows the real window's output device

    // recording
    // beat page
    bool beatErase = false;          // Erase is armed: the next pad clears that drum
    // kept beats: every beat you make has its own place; the one shown is the one the engine plays
    struct BeatSlot { QString name; uint16_t pat[td::kDrums] = {0}; int kit = 0; };
    std::vector<BeatSlot> beatSlots;
    int beatCur = 0, beatPage = 0, beatNameNext = 2;
    qint64 beatHoldUntil = 0;        // just switched: the engine has not caught up yet
    uint32_t seenHits[td::kDrums] = {0};
    qint64 padLit[td::kDrums] = {0}; // when each pad last sounded

    QTimer saveTimer;                       // an edit is on disk a moment later



    QPixmap facePix;  // scan lines and corner shading over the whole screen

    QPointF toCanvas(QPointF widgetPos) const;

    // pages
    void drawAll(QPainter &p, qreal W, qreal H);
    void paintBacking();
    void paintBeat();
    void hitPad(int drum);
    void beatsLoad(const QJsonObject &state, const td::SessionData &sd);
    void beatsSave(QJsonObject &state);
    void beatWatch();
    void beatSwitchTo(int i);
    void beatNew();
    void paintKeptBeats(qreal y);
    void paintTune();
    void paintSystem();
    void paintHelp();
    void helpAsk();
    void paintTabs();
    void paintKeyTest();

    // actions
    void tapTempo();
    void updateBend();
    void releaseAllKeys();
};
