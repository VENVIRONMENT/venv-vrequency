// venv vrequency — the 720x720 instrument panel. One custom-painted widget with a
// small immediate-mode layer: every frame draws the controls and registers
// their touch zones, so drawing and hit-testing can never disagree.
#pragma once
#include "audio.h"
#include "analysis.h"
#include "instrument.h"
#include "keymap.h"
#include "presets.h"
#include "store.h"
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
#include <thread>
#include <vector>

class QPainter;
class QProcess;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    // PG_PLAY is the instrument itself; its tab is called Synth. The app opens on it.
    enum Page { PG_TRACK, PG_PLAY, PG_SOUND, PG_BEAT, PG_BACK, PG_TUNE, PG_SYS, PG_HELP, PG_COUNT };
    enum Phase { PH_PRESS, PH_MOVE, PH_RELEASE };

    MainWindow(td::Instrument &inst, AudioOut &audio);
    ~MainWindow() override;

    // --- also used by the self-test ---
    void setPage(int p) { pg = p; update(); }
    int page() const { return pg; }
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const;
    QStringList zoneNames() const;
    QImage shot();
    const td::Patch &leadPatch() const { return lead; }
    int undoDepth() const { return (int)undo.size(); }
    bool saveSession(const QString &path);
    bool loadSession(const QString &path);
    void saveState();
    int addExtraSounds(int from = 0); // engine's extra sounds -> favourites; returns files written
    QString startRecording();
    QString stopRecording();
    bool togglePlayback();            // play the newest recording back, or stop it; false = nothing to play
    bool playingBack() const;
    void perform(int act, bool shift = false);
    void setKeyTest(bool on) { keyTest = on; }
    int maxKeysHeld() const { return maxHeld; }
    QString lastToast() const { return toastText; }
    const td::TrackData &trackData() const { return track; }
    int trackUndoDepth() const { return (int)trackUndo.size(); }
    bool clipInHand() const { return handOn; }
    int chosenLane() const { return selLane; }
    int chosenIndex() const { return selIdx; }
    bool chopping() const { return chopOpen; }
    int playLineBar() const { return playBar; }
    int exportStatus() const { return exportState.load(); } // 0 idle, 1 working
    QString lastRecordingPath() const { return lastRecording; }

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
    struct Zone { QRectF r; QString name; std::function<void(QPointF, int)> fn; };
    struct Held { int kind; int a, b; }; // what a physical key did when pressed

    td::Instrument &inst;
    AudioOut &audio;
    KeyMap km;
    QTimer frameTimer, recTimer;
    QElapsedTimer clock;

    // sounds
    td::Patch lead;
    td::Patch bank[10];
    int curSlot = 0;
    std::vector<td::Patch> undo;
    QVector<store::NamedPatch> favs;
    int favPage = 0;
    int extrasSeeded = 0;   // how many of the engine's extra sounds have been offered as favourites
    bool favDelete = false, slotSave = false;
    td::Rng rng;

    // ui state
    int pg = PG_PLAY;
    bool deepEdit = false;
    int deepPage = 0;
    int helpPage = 0;
    bool keyTest = false;
    bool chordRow = false;
    bool modChromatic = false, modChord = false, modCommand = false, bendUp = false, bendDown = false, susKey = false, susLatch = false;
    QHash<int, Held> held;
    int maxHeld = 0;
    QStringList keyLog;
    int touchCol[td::kStrings] = {-1, -1, -1};
    std::deque<float> trail[td::kStrings];
    QString toastText; qint64 toastUntil = 0;
    std::vector<qint64> taps;
    QStringList sessions;
    td::UiState u;
    int seenSinkGeneration = -1;

    // recording
    WavWriter wav;
    QString lastRecording;
    QProcess *player = nullptr;      // plays a recording back
    // beat page
    bool beatErase = false;          // Erase is armed: the next pad clears that drum
    uint32_t seenHits[td::kDrums] = {0};
    qint64 padLit[td::kDrums] = {0}; // when each pad last sounded

    // track page
    struct Dash { float a, b, pitch; };
    struct ClipDash { std::vector<Dash> d; float lo = 60, hi = 60; };
    td::TrackData track;                    // the master copy; the engine gets its own after every edit
    std::vector<td::TrackData> trackUndo, trackRedo;
    int selLane = -1, selIdx = -1;          // the chosen placed clip
    bool handOn = false, handFromShelf = false; // a clip in hand: tap a bar, or Place, puts it down
    td::Placement hand;
    int shelfPage = 0, viewBar = 0, playBar = 0;
    bool chopOpen = false;
    int chopPiece = 0;
    int removeArmed = -1; qint64 removeArmedAt = 0;
    bool dragOn = false, dragMoved = false;
    int dragGrab = 0;
    td::TrackData dragBefore;
    QTimer trackSaveTimer;
    std::thread exportThread;
    std::atomic<int> exportState{0}, exportPct{0}, exportSecs{0}; // state: 0 idle, 1 working, 2 done, 3 failed
    QString exportFile;
    QHash<const td::Clip *, ClipDash> dashCache;

    // listen
    std::thread listenThread;
    std::atomic<int> listenState{0}; // 0 idle, 1 listening, 2 result, 3 failed
    std::mutex listenMx;
    td::ListenResult listenResult;
    QString listenError;
    int listenGuess = 0;
    qint64 listenStarted = 0;

    // immediate-mode plumbing
    QPainter *g = nullptr;
    std::vector<Zone> zones;
    QHash<int, Zone> captured;
    QSet<QString> downNames;
    mutable QHash<quint32, QFont> fontCache;

    // the scope on the play page
    int tbIdx = 4, vdIdx = 1, rangeT = 0, rangeV = 0; // time/div and volts/div steps, auto-ranged
    float measFreq = 0, measPk = 0;
    bool trigd = false;
    QImage phos;      // fading trace
    float glitchVis = 0, glitchHue = 0; // colour streaks while the crusher is on
    QPixmap gratPix;  // graticule
    QPixmap facePix;  // scan lines and corner shading over the whole screen

    QPointF toCanvas(QPointF widgetPos) const;
    void toast(const QString &s, int ms = 1800);

    // drawing helpers
    QFont font(int px, bool bold) const;
    int chW(int px) const;                                  // character cell width at a size
    QString fit(const QString &s, int px, qreal width) const; // shorten to fit
    void txt(QRectF r, const QString &s, int px, QColor c, int align = Qt::AlignCenter, bool bold = false);
    void frame(QRectF r, QColor line, QColor fill = QColor());
    void tag(QRectF r, const QString &s, int px, int align = Qt::AlignCenter);
    void section(qreal y, const QString &s);
    void zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn);
    void button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                const QString &sub = QString(), int look = 0);
    void slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                std::function<void(float)> fn);
    void stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                 std::function<void()> minus, std::function<void()> plus);
    void paramSlider(QRectF r, int param);

    // pages
    void drawAll(QPainter &p, qreal W, qreal H);
    void paintPlay();
    void drawScope(QRectF xy);
    void paintSound();
    void paintBacking();
    void paintBeat();
    void hitPad(int drum);
    void paintTune();
    void paintSystem();
    void paintHelp();
    void paintTabs();
    void paintTrack();
    void paintTrackTools();
    void paintChop();
    void drawPlacement(int lane, int idx);
    void drawClipBody(const td::Clip &c, double srcA, double srcB, QRectF r, QColor ink);
    void hatch(QRectF r, QColor c);
    const ClipDash &dashes(const td::Clip &c);
    void paintKeyTest();
    void paintChordPads(QRectF area, bool withSlots);
    void paintLoopButton(QRectF r);

    // actions
    void pushUndo();
    void applyLead(bool resetSlot);
    void selectSlot(int i);
    void randomise();
    void mutate();
    void undoSound();
    void saveFavourite();
    void setParam(int p, float v);
    void set(int setting, float v) { inst.set(setting, v); }
    void tapTempo();
    void toggleRecord();
    void startListen();
    void applyListen(bool key, bool tempo);
    void updateBend();
    void releaseAllKeys();
    void refreshLists();
    void drainRecording();

    // the track
    td::Placement *chosen();
    void trackSync();
    bool trackApply(const std::function<bool(td::TrackData &)> &fn); // one undoable edit
    void chooseAt(int lane, int bar);
    void showBar(int bar);
    int maxViewBar() const;
    bool placeSpot(int &lane, int &bar) const;
    void trackTake(int clip);
    void trackKeep();
    bool trackPlace(int lane, int bar);
    void trackPlaceAtLine();
    void trackRemove();
    void trackRepeat();
    void trackSplit();
    void trackJoin();
    void trackMute();
    void trackCycle();
    void trackCopy(bool cut);
    void trackUndoDo();
    void trackRedoDo();
    void trackPlayStop();
    void trackLine(int dbar);
    void trackSelect(int dx, int dy);
    void trackMove(int dbar, int dlane);
    void trackChop();
    void trackExport();
    void pollExport();
    void laneTouch(QPointF p, int phase);
    void chopTap(double frac, double stripWidth);
    void chopEdit(const std::function<bool(td::Placement &)> &fn);
    bool trackKey(int key, int plainAct);
};

QString paramText(int param, float v);
float paramToNorm(int param, float v);
float normToParam(int param, float n);
