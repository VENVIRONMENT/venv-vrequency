// venv-vrequency — the Track page: kept clips on the left, four lanes of bars, tools below.
//
// The page owns the track. Every edit goes through trackApply(), which keeps an undo step,
// tidies the lanes and hands the engine a fresh immutable copy. Clips themselves are shared
// and never change, so a copy of the track costs almost nothing. The code is split by job:
// track_page.cpp (layout), track_draw.cpp, track_tools.cpp, track_edit.cpp, track_clips.cpp,
// track_arrange.cpp, track_chop.cpp, track_sound*.cpp, track_mixer.cpp, track_export.cpp, track_keys.cpp.
#pragma once
#include "app/pages/page.h"
#include "core/track/track.h"
#include <QHash>
#include <QString>
#include <atomic>
#include <functional>
#include <thread>
#include <vector>

class QJsonObject;

class TrackPage : public Page {
public:
    enum PickKind { PK_KEYS, PK_SYNTH, PK_GUITAR, PK_BASS, PK_DRUMS, PK_COUNT }; // the sound picker's kinds

    explicit TrackPage(Shell &s) : Page(s) {}
    ~TrackPage() override { stopExport(); }
    void paint() override;
    bool key(int qtKey, int plainAct) override;
    void tick() override;                    // the export finishing, and hearing a clip alone
    void disarm() override { removeArmed = -1; }

    // what the window, the control socket and the self-test reach
    td::TrackData track;                     // the master copy; the engine gets its own after every edit
    bool handOn = false;                     // a clip in hand: tap a bar, or Place, puts it down
    QString exportFile;
    int shelfPage = 0;                       // which five clips the shelf shows
    bool trackApply(const std::function<bool(td::TrackData &)> &fn); // one undoable edit
    void trackPlayStop();
    void trackTake(int clip);                // pick a clip off the shelf into the hand
    void trackKeep();                        // copies of what is live now go on the shelf
    bool trackPlace(int lane, int bar);
    void trackExport();
    void loadJson(const QJsonObject &o);     // a saved session's track; one undo step
    void stopExport() { if (exportThread.joinable()) exportThread.join(); }
    bool armed() const { return removeArmed >= 0; }
    void armAll() { removeArmed = 0; removeArmedAt = sh.clock.elapsed(); }
    const td::TrackData &trackData() const { return track; }
    int trackUndoDepth() const { return (int)trackUndo.size(); }
    bool clipInHand() const { return handOn; }
    int chosenLane() const { return selLane; }
    int chosenIndex() const { return selIdx; }
    bool chopping() const { return chopOpen; }
    bool pickingSound() const { return soundOpen; }
    int soundKindShown() const { return soundKind; }
    bool hearingAlone() const { return hearOn; }
    bool mixing() const { return mixOpen; }
    int mixLaneShown() const { return mixLane; }
    int playLineBar() const { return playBar; }
    int exportStatus() const { return exportState.load(); } // 0 idle, 1 working

private:
    struct Dash { float a, b, pitch; };
    struct ClipDash { std::vector<Dash> d; float lo = 60, hi = 60; };
    void toast(const QString &s, int ms = 1800) { sh.toast(s, ms); }

    std::vector<td::TrackData> trackUndo, trackRedo;
    int selLane = -1, selIdx = -1;          // the chosen placed clip
    bool handFromShelf = false;
    td::Placement hand;
    int viewBar = 0, playBar = 0;
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
    std::thread exportThread;
    std::atomic<int> exportState{0}, exportPct{0}, exportSecs{0}; // state: 0 idle, 1 working, 2 done, 3 failed
    QHash<const td::Clip *, ClipDash> dashCache;

    // drawing (track_page.cpp, track_draw.cpp, track_tools.cpp)
    void paintTrackTools();
    void drawPlacement(int lane, int idx);
    void drawClipBody(const td::Clip &c, double srcA, double srcB, QRectF r, QColor ink);
    void hatch(QRectF r, QColor c);
    const ClipDash &dashes(const td::Clip &c);
    // bookkeeping and the undo steps (track_edit.cpp)
    td::Placement *chosen();
    void trackSync();
    void chooseAt(int lane, int bar);
    void showBar(int bar);
    int maxViewBar() const;
    bool placeSpot(int &lane, int &bar) const;
    void trackUndoDo();
    void trackRedoDo();
    void trackLine(int dbar);
    void pushUndo(const td::TrackData &before);
    // the shelf and placing (track_clips.cpp: trackTake, trackKeep, trackPlace)
    void trackPlaceAtLine();
    void trackRemove();
    // working on a placed clip (track_arrange.cpp)
    void trackRepeat();
    void trackSplit();
    void trackJoin();
    void trackMute();
    void trackCycle();
    void trackCopy(bool cut);
    void trackSelect(int dx, int dy);
    void trackMove(int dbar, int dlane);
    void laneTouch(QPointF p, int phase);
    // Chop (track_chop.cpp)
    void paintChop();
    void trackChop();
    void chopTap(double frac, double stripWidth);
    void chopEdit(const std::function<bool(td::Placement &)> &fn);
    // the sound picker and hearing a clip alone (track_sound.cpp, track_sound_view.cpp)
    void paintClipSound();
    void trackSound();
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
    // the mixer (track_mixer.cpp)
    void paintMixer();
    void trackMixer();
    void mixApply(int lane, const std::function<bool(td::LaneMix &)> &fn);
    void mixReset();
    void mixFader(int lane, qreal y, qreal top, qreal bottom, int phase);
    // export (track_export.cpp)
    void pollExport();
};
