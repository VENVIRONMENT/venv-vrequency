// Tendril — the 720x720 instrument panel. One custom-painted widget with a
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
    enum Page { PG_PLAY, PG_SOUND, PG_BACK, PG_TUNE, PG_SYS, PG_HELP, PG_COUNT };
    enum Phase { PH_PRESS, PH_MOVE, PH_RELEASE };

    MainWindow(td::Instrument &inst, AudioOut &audio);
    ~MainWindow() override;

    // --- also used by the self-test ---
    void setPage(int p) { pg = p; update(); }
    int page() const { return pg; }
    void handleKey(int qtKey, bool down);
    void pointer(int id, QPointF canvasPos, int phase);
    QRectF zoneRect(const QString &name) const;
    QImage shot();
    const td::Patch &leadPatch() const { return lead; }
    int undoDepth() const { return (int)undo.size(); }
    bool saveSession(const QString &path);
    bool loadSession(const QString &path);
    void saveState();
    QString startRecording();
    QString stopRecording();
    void perform(int act, bool shift = false);
    void setKeyTest(bool on) { keyTest = on; }
    int maxKeysHeld() const { return maxHeld; }
    QString lastToast() const { return toastText; }

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
    QFont fDisplay, fLabel, fMono;
    mutable QHash<quint32, QFont> fontCache;

    QPointF toCanvas(QPointF widgetPos) const;
    void toast(const QString &s, int ms = 1800);

    // drawing helpers
    QFont font(const QFont &base, int px, int weight = QFont::Normal, qreal spacing = 0) const;
    void txt(QRectF r, const QString &s, const QFont &f, QColor c, int align = Qt::AlignCenter);
    void panel(QRectF r, QColor fill, QColor line = QColor(), qreal radius = 10);
    void zone(QRectF r, const QString &name, std::function<void(QPointF, int)> fn);
    void button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                const QString &sub = QString(), QColor accent = QColor());
    void slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                std::function<void(float)> fn);
    void stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                 std::function<void()> minus, std::function<void()> plus);
    void paramSlider(QRectF r, int param);

    // pages
    void drawAll(QPainter &p, qreal W, qreal H);
    void paintPlay();
    void paintSound();
    void paintBacking();
    void paintTune();
    void paintSystem();
    void paintHelp();
    void paintTabs();
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
};

QString paramText(int param, float v);
float paramToNorm(int param, float v);
float normToParam(int param, float n);
