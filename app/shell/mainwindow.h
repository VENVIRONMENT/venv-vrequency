// venv-vrequency — the 720x720 instrument panel. One custom-painted widget with a
// small immediate-mode layer: every frame draws the controls and registers
// their touch zones, so drawing and hit-testing can never disagree.
#pragma once
#include "audio/audio.h"
#include "analysis.h"
#include "instrument.h"
#include "app/shell/keymap.h"
#include "presets.h"
#include "app/store/store.h"
#include "app/ui/ui.h"
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
class QProcess;
class ControlServer;
class TourView;

// One line of the copilot chat.
struct ChatMsg {
    int seq = 0;
    QString from, text, kind; // kind: say, tip, ask (agents) or say (the user)
    qint64 ts = 0;            // ms since the epoch
    bool user = false;
    // what the player was looking at when they sent it (user messages only)
    QString page;             // page name: "synth", "help"...
    QStringList focus;        // zone names being discussed (highlighted, or last tapped)
    QString via;              // "dock" or "chat"
};

class MainWindow : public QWidget {
    Q_OBJECT
    friend class ControlServer;
    friend class TourView;
    friend struct TourDemo;
public:
    // PG_PLAY is the instrument itself; its tab is called Synth. The app opens on it.
    enum Page { PG_TRACK, PG_PLAY, PG_SOUND, PG_BEAT, PG_BACK, PG_TUNE, PG_SYS, PG_HELP, PG_CHAT, PG_COUNT };

    // `scratch`: an off-screen copy used only to draw the tour's pictures: it reads and writes no files
    MainWindow(td::Instrument &inst, AudioOut &audio, bool scratch = false);
    ~MainWindow() override;

    // --- also used by the self-test ---
    void setPage(int p) { goPage(p); update(); }
    int page() const { return pg; }
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const { return ui.zoneRect(name); }
    QStringList zoneNames() const { return ui.zoneNames(); }
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
    int favouritesPage() const { return favPage; }
    QString armedModes() const { QStringList l; if (favDelete) l << "delete"; if (slotSave) l << "store"; if (beatErase) l << "erase"; if (removeArmed >= 0) l << "remove"; return l.join(","); }
    void armAllModes() { favDelete = slotSave = beatErase = true; removeArmed = 0; removeArmedAt = clock.elapsed(); }
    QString zoneAt(QPointF c) const { return ui.zoneAt(c); }
    int maxKeysHeld() const { return maxHeld; }
    QString lastToast() const { return toastText; }
    const td::TrackData &trackData() const { return track; }
    int trackUndoDepth() const { return (int)trackUndo.size(); }
    bool clipInHand() const { return handOn; }
    int chosenLane() const { return selLane; }
    int chosenIndex() const { return selIdx; }
    enum PickKind { PK_KEYS, PK_SYNTH, PK_GUITAR, PK_BASS, PK_DRUMS, PK_COUNT }; // the sound picker's kinds
    bool chopping() const { return chopOpen; }
    bool pickingSound() const { return soundOpen; }
    int soundKindShown() const { return soundKind; }
    bool hearingAlone() const { return hearOn; }
    bool mixing() const { return mixOpen; }
    int keptBeats() const { return (int)beatSlots.size(); }
    int keptBeatShown() const { return beatCur; }
    QString keptBeatName(int i) const { return i >= 0 && i < (int)beatSlots.size() ? beatSlots[i].name : QString(); }
    int mixLaneShown() const { return mixLane; }
    int playLineBar() const { return playBar; }
    int exportStatus() const { return exportState.load(); } // 0 idle, 1 working
    QString lastRecordingPath() const { return lastRecording; }
    // --- the copilot chat ---
    int chatAdd(const QString &from, const QString &text, const QString &kind, bool user,
                const QString &page = QString(), const QStringList &focus = QStringList(), const QString &via = QString()); // returns its seq
    const std::deque<ChatMsg> &chatLog() const { return chat; }
    int chatLastSeq() const { return chatSeq; }
    int chatUnreadCount() const { return chatUnread; }
    QString chatDraftText() const { return chatDraft; }
    void chatSeen() { copilotSeenAt = clock.elapsed(); update(); }   // a control client polled or is waiting
    void chatWaiting(int n) { copilotWaiters = n; update(); }
    bool chatTickerShowing() const { return clock.elapsed() < tickerUntil; }
    void chatToggle();

    // --- guided help: the highlight overlay and the chat dock (app/guide.cpp) ---
    struct ZoneInfo { QString name; QRectF r; QString label; bool covered = false; };
    static const char *pageName(int p);
    static int pageFromName(const QString &n);                 // -1 when unknown; "play" is the Synth page
    QStringList highlightedZones();                            // active highlights on this page
    // Highlights the named zones of the current page (names may be aliases or `prefix*`); returns the ones
    // that exist and puts the others in `unknown`. The zones must have been drawn: call refreshZones() first.
    QStringList setHighlight(const QStringList &names, const QString &label, int ms, QStringList &unknown);
    void clearHighlight();
    QRectF highlightCaptionRect() const { return hlCaption; }
    QString highlightLabel() const { return hl.label; }
    void refreshZones() { shot(); }                            // draw once so zoneRect/zoneNames are current
    QVector<ZoneInfo> zoneCatalogue(int page, int helpSub = -1); // every zone of a page (draws it off-screen, then puts things back)
    QString zoneLabel(const QString &name) const;
    bool dockIsOpen() const { return dockOpen; }
    bool dockHasFocus() const { return dockOpen && dockFocus && pg != PG_CHAT; }
    QRectF dockRect() const { return dockOpen && pg != PG_CHAT ? dockR : QRectF(); }
    void dockOpenFocus();
    void dockClose();
    QString dockTopic() const { return topicLabel; }
    QString lastQuestionZone() const { return lastQuestion; }
    QStringList guideFocus();                                  // what a message sent now would carry as `focus`
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
    // The one way to change page: every armed mode (Delete, Store, Erase, track remove, deep edit) is
    // dropped whenever the page really changes, whatever the route (tab, Tab, Esc, control socket, highlight).
    void goPage(int p) {
        if (p == pg) return;
        pg = p; deepEdit = false; favDelete = false; slotSave = false; beatErase = false; removeArmed = -1;
        update();
    }
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
    // chat
    std::deque<ChatMsg> chat;
    int chatSeq = 0, chatUnread = 0, chatScroll = 0, chatReturn = PG_PLAY;
    QString chatDraft, pendingText;
    qint64 copilotSeenAt = -100000, tickerUntil = 0;
    int copilotWaiters = 0;
    ChatMsg ticker;
    // guided help
    struct Hl { QStringList zones; QString label; qint64 until = 0, issued = -100000; int page = -1; };
    Hl hl;
    QString topicLabel; QStringList topicZones; int topicPage = -1;  // what the last agent message was about
    QString lastTapped, lastQuestion; int lastTappedPage = -1;
    QRectF hlCaption, dockR;
    bool dockOpen = false, dockFocus = false;
    int dockLines = 0;
    bool scratchWindow = false;
    bool tourSeenFlag = false;                               // the player has answered the first-run offer
    QString tourLastPage;                                    // the tour page last looked at (saved)
    std::unique_ptr<TourView> tourView;
    QStringList askFocus; QString askPage, askVia;                   // a question started from the tour: sent with via "tour"
    QString helpPrefill;
    QString helpSel;                                         // the Help row last tapped
    QHash<QString, QString> helpDesc;                        // Help row zone -> its description
    qint64 helpAskAt = 0;
    // synthetic notes from the control socket: string -> ids held
    int nextNoteId = 0x400;
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
    // kept beats: every beat you make has its own place; the one shown is the one the engine plays
    struct BeatSlot { QString name; uint16_t pat[td::kDrums] = {0}; int kit = 0; };
    std::vector<BeatSlot> beatSlots;
    int beatCur = 0, beatPage = 0, beatNameNext = 2;
    qint64 beatHoldUntil = 0;        // just switched: the engine has not caught up yet
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
    // the sound picker: kinds of sound across the top, six cells below, for the chosen placed clip
    bool soundOpen = false;
    int soundKind = PK_SYNTH, soundPage = 0;
    int soundForLane = -1, soundForIdx = -1; // the placement the kind and page were last set up for
    bool hearOn = false, hearSeen = false;   // the chosen clip is playing alone, round and round
    int hearLane = -1, hearIdx = -1;
    qint64 hearAt = 0;
    // the lane mixer: a strip for each lane in place of the tools
    bool mixOpen = false, mixDragged = false;
    int mixLane = 0;
    td::TrackData mixBefore;                // the track before a fader drag: the whole drag is one undo step
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

    Ui ui;                              // the drawing and touch toolkit the pages share

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
    void paramSlider(QRectF r, int param);

    // pages
    void drawAll(QPainter &p, qreal W, qreal H);
    void paintPlay();
    void drawScope(QRectF xy);
    void paintSound();
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
    void paintChat();
    void paintTicker();
    void paintHighlights();
    void paintDock();
    void layoutDock();
    void helpAsk();
    QStringList expandZoneNames(const QStringList &names, QStringList &unknown) const;
    bool chatKey(int key, int plainAct, bool dock = false);
    void chatSend();
    void paintTabs();
    void paintTrack();
    void paintTrackTools();
    void paintChop();
    void paintClipSound();
    void paintMixer();
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
    void trackSound();
    void trackMixer();
    void mixApply(int lane, const std::function<bool(td::LaneMix &)> &fn);
    void mixReset();
    void mixFader(int lane, qreal y, qreal top, qreal bottom, int phase);
    void soundSetup();
    int soundCount(int kind) const;
    td::Patch soundPatch(int kind, int i) const;
    void soundApply(const std::function<void(td::SoundPick &)> &fn);
    void soundPickAt(int cell);
    void soundKindStep(int d);
    void soundScope();
    td::TrackData hearTrack();
    void hearStart();
    void hearEnd(bool stop);
    void hearToggle();
    void hearWatch();
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
