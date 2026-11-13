// Tendril — the 720x720 instrument face: a bench oscilloscope front panel
// (recessed tube, soft keys, knob groups) over a legend-strip key deck.
// One custom-painted widget with a small immediate-mode layer: every paint
// draws the controls and registers their touch zones, so drawing and
// hit-testing can never disagree. Rendering lives in panel.cpp (chassis,
// controls, key deck) and crt.cpp (tube contents, soft-key menus).
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
#include <QRegion>
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

class MainWindow : public QWidget {
    Q_OBJECT
public:
    enum Menu { M_SOUND, M_EDIT, M_KEY, M_CHORD, M_TIME, M_FILE, M_UTIL, M_COUNT };
    enum View { V_SCOPE, V_HELP, V_STATUS, V_KEYTEST, V_LISTEN, V_COUNT };
    enum Phase { PH_PRESS, PH_MOVE, PH_RELEASE };

    MainWindow(td::Instrument &inst, AudioOut &audio);
    ~MainWindow() override;

    // --- also used by the self-test ---
    void setMenu(int m, int page = 0);
    int menu() const { return mnu; }
    void setView(int v);
    int view() const { return vw; }
    void pressSoft(int i);          // soft key 0..5 under the tube
    void turnEntry(float detents);  // the entry knob
    void turnKnob(const QString &name, float amount); // amount: fraction of the knob's travel
    QString softTitle(int i);
    QString softValue(int i);
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const;
    QStringList zoneNames() const;
    QImage shot();
    QImage shotPartial(const QImage &base, QRect area); // repaint only `area` over an older picture, as a partial paint event does
    const td::Patch &leadPatch() const { return lead; }
    int undoDepth() const { return (int)undo.size(); }
    bool saveSession(const QString &path);
    bool loadSession(const QString &path);
    void saveState();
    QString startRecording();
    QString stopRecording();
    void perform(int act, bool shift = false);
    void setKeyTest(bool on) { setView(on ? V_KEYTEST : V_SCOPE); maxHeld = 0; }
    int maxKeysHeld() const { return maxHeld; }
    QString lastToast() const { return toastText; }
    bool chordRowOn() const { return chordRow; }
    int timebaseIndex() const { return tbIdx; }

protected:
    void paintEvent(QPaintEvent *) override;
    void keyPressEvent(QKeyEvent *) override;
    void keyReleaseEvent(QKeyEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
    bool event(QEvent *) override;
    void focusOutEvent(QFocusEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    struct Zone { QRectF r; QString name; std::function<void(QPointF, int)> fn; };
    struct Held { int kind; int a, b; }; // what a physical key did when pressed
    struct Soft {                        // one soft key of the current menu
        QString title, value;
        bool lit = false;
        std::function<void()> press;       // tap (when it has no knob value) or second tap
        std::function<void(float)> turn;   // entry knob, in detents
    };
    struct Hist { qint64 t; float note[td::kStrings]; bool gate[td::kStrings]; };

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
    bool slotSave = false;
    td::Rng rng;

    // front panel state
    int mnu = M_SOUND, vw = V_SCOPE;
    int menuPage[M_COUNT] = {0};
    int selSoft = -1;
    Soft soft[6];
    int favIdx = 0, sessIdx = 0, delIdx = 0, helpPage = 0;
    qint64 quitArmedUntil = 0;
    float entryAngle = 0, stepAcc = 0;
    QString selKnob = "entry";
    QHash<QString, QPointF> dragLast;
    QHash<QString, float> dragAcc;
    QHash<QString, std::function<void(float)>> knobTurn;
    QString readoutLabel, readoutValue; qint64 readoutUntil = 0;

    // scope
    int tbIdx = 3, vdIdx = 1;      // time/div and volts/div steps
    float measFreq = 0, measPk = 0;
    bool trigd = false;
    QImage phos;                   // phosphor layer: fades instead of clearing
    QPixmap chassisPix, gratPix, glassPix, deckPix;
    QString deckSig;
    std::deque<Hist> hist;         // recent string pitches, for the pitch lanes

    bool chordRow = false;
    bool modChromatic = false, modChord = false, modCommand = false, bendUp = false, bendDown = false, susKey = false, susLatch = false;
    QHash<int, Held> held;
    int maxHeld = 0;
    QStringList keyLog;
    int touchCol[td::kStrings] = {-1, -1, -1};
    QString toastText; qint64 toastUntil = 0;
    std::vector<qint64> taps;
    QStringList sessions;
    td::UiState u;
    int seenSinkGeneration = -1;
    quint64 panelSig = 0, deckSigDyn = 0;

    // recording
    WavWriter wav;
    QString lastRecording;

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
    QRegion dirty; bool allDirty = true;
    std::vector<Zone> zones;
    QHash<int, Zone> captured;
    QSet<QString> downNames;

    QPointF toCanvas(QPointF widgetPos) const;
    QRect toWidget(QRectF canvasRect) const;
    void toast(const QString &s, int ms = 1800);
    void readout(const QString &label, const QString &value);
    bool vis(const QRectF &r) const;
    void zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn);

    // panel.cpp — chassis, controls, key deck
    void drawAll(QPainter &p, qreal W, qreal H);
    void buildChassis();
    void buildDeckLegends();
    void drawPanel();
    void drawUnderTube();
    void drawDeck();
    void knobCtl(QPointF c, qreal r, const QString &name, qreal angleDeg, std::function<void(float)> turn);
    void paramKnob(QPointF c, qreal r, const QString &name, int param);
    void capCtl(QRectF r, const QString &name, const QString &label, QColor lit, int style, std::function<void()> fn, qreal grow = 3);
    quint64 panelSignature() const;
    quint64 deckSignature() const;

    // crt.cpp — tube contents and soft-key menus
    void buildMenu();
    void drawCrt();
    void drawScope();
    void drawLists();
    void drawTextView();
    void crtText(QRectF r, const QString &s, int px, QColor c, int align = Qt::AlignLeft | Qt::AlignVCenter, bool bold = false);
    void crtInverse(QRectF r, const QString &s, int px, int align = Qt::AlignCenter);
    void autoScale();
    Soft paramSoft(int param);
    Soft settingSoft(const QString &title, const QString &value, std::function<void(int)> step, float perStep = 4);

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
    void sustainLatch();
};

QString paramText(int param, float v);
float paramToNorm(int param, float v);
float normToParam(int param, float n);
