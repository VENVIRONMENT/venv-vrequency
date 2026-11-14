// venv-vrequency — the Track page: the tools below the lanes, and the keys line.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::paintTrackTools() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const bool exporting = exportState.load() == 1;
    Placement *sel = chosen();
    QString s;
    if (handOn) s = QString("%1 in hand  tap a bar to place it").arg(QString::fromUtf8(track.clips[hand.clip]->name));
    else if (sel) s = QString("%1 chosen, %2  drag it to move").arg(QString::fromUtf8(track.clips[sel->clip]->name)).arg(tl::barsText(sel->bar, sel->bars));
    else if (track.clips.empty()) s = "No clips yet  + Keep copies what you have made";
    else s = "Tap a clip on the left, then a bar";
    ui.section(414, s);

    const qreal pw = (688 - 3 * 8) / 4.0;
    auto c4 = [pw](int i, qreal y) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), 60); };
    int spotLane = -1, spotBar = -1;
    const bool spot = handOn && placeSpot(spotLane, spotBar);
    const qreal fw = (688 - 4 * 8) / 5.0;
    auto r5 = [fw](int i) { return QRectF(std::floor(16 + i * (fw + 8)), 438, std::floor(fw), 60); };
    ui.button(r5(0), "track_place", "Place", handOn, [this] { trackPlaceAtLine(); },
           handOn ? (spot ? QString("at bar %1").arg(spotBar + 1) : QString("no room")) : QString("pick a clip first"));
    ui.button(r5(1), "track_chop", "Chop", false, [this] { trackChop(); }, "cut and reorder");
    QString plays = "keys guitar bass";
    if (sel) {
        const Clip &sc = *track.clips[sel->clip];
        const char *sn = soundName(sc, pickFor(sc, *sel));
        plays = sc.kind == CK_BEAT ? nice(sn) + " kit" : QString::fromUtf8(sn);
    }
    ui.button(r5(2), "track_sound", "Sound", false, [this] { trackSound(); }, plays);
    ui.button(r5(3), "track_repeat", "Repeat", false, [this] { trackRepeat(); }, "copy to the right");
    const bool armed = removeArmed >= 0 && sh.clock.elapsed() - removeArmedAt <= 4000;
    ui.button(r5(4), "track_remove", "Remove", false, [this] { trackRemove(); },
           armed ? QString("again to delete") : (!sel && handOn && handFromShelf ? QString("clip and copies") : QString("from the track")),
           armed ? LOOK_ARMED : LOOK_PLAIN);

    const int from = cyc && (playBar < track.cycleFrom || playBar >= track.cycleTo) ? track.cycleFrom : qMin(playBar, track.bars - 1);
    ui.button(c4(0, 506), "track_play", u.trackPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.trackPlaying, [this] { trackPlayStop(); },
           u.trackPlaying ? QString("bar %1").arg((int)(u.trackPos / 4.0) + 1) : QString("from bar %1").arg(from + 1));
    ui.button(c4(1, 506), "track_cycle", "Cycle", cyc, [this] { trackCycle(); },
           cyc ? tl::barsText(track.cycleFrom, track.cycleTo - track.cycleFrom) : (sel ? QString("round this clip") : QString("round 4 bars")));
    ui.stepper(c4(2, 506), "track_length", "Length", QString("%1 bars").arg(track.bars),
            [this] {
                int last = 1;
                for (int ln = 0; ln < kLanes; ++ln) for (const Placement &p : track.lane[ln]) last = qMax(last, p.bar + p.bars);
                if (track.bars <= last) { toast("A clip is in the way: move or remove it first", 1800); return; }
                --track.bars; trackSync();
            },
            [this] { if (track.bars < kTrackMaxBars) { ++track.bars; trackSync(); } });
    ui.button(c4(3, 506), "track_export", exporting ? QString("%1%").arg(exportPct.load()) : QString("Export"), exporting, [this] { trackExport(); },
           exporting ? "writing audio" : "save as audio");

    auto c5 = [](int i) { return QRectF(16 + i * 106, 574, 98, 60); };
    ui.button(c5(0), "track_split", "Split", false, [this] { trackSplit(); }, "at the line");
    ui.button(c5(1), "track_join", "Join", false, [this] { trackJoin(); }, "with the next");
    ui.button(c5(2), "track_mute", "Mute", sel && sel->mute, [this] { trackMute(); }, "this clip");
    ui.button(c5(3), "track_undo", "Undo", false, [this] { trackUndoDo(); }, trackUndo.empty() ? QString("nothing yet") : QString("%1 step%2").arg(trackUndo.size()).arg(trackUndo.size() == 1 ? "" : "s"));
    QString mixSub = "lane levels";
    for (int ln = 0; ln < kLanes; ++ln)
        if (!laneHeard(track, ln) || track.mix[ln].level != kLaneLevelNormal) { mixSub = "lanes changed"; break; }
    ui.button(c5(4), "track_mixer", "Mix", false, [this] { trackMixer(); }, mixSub);
    ui.stepper(QRectF(546, 574, 158, 60), "track_view", "Shown", QString("%1-%2").arg(viewBar + 1).arg(viewBar + tl::VIEW),
            [this] { viewBar = qMax(0, viewBar - 4); }, [this] { viewBar = qMin(maxViewBar(), viewBar + 4); });

    const QString cmd = sh.km.keyFor(A_COMMAND, false);
    ui.txt(QRectF(18, 644, 684, 20), QString("Keys: %1 play, %2 to bar 1, arrows choose a clip, Alt+arrows move it,")
            .arg(sh.km.keyFor(A_SUSTAIN, false)).arg(sh.km.keyFor(A_LOOP, false)), 12, MID, L);
    ui.txt(QRectF(18, 664, 684, 20), "Shift+arrows move the line. 1-5 pick a clip. K keep, E chop, I sound, M mute, X mix, C cycle.", 12, MID, L);
    ui.txt(QRectF(18, 684, 684, 20), QString("%1 + T split, J join, R repeat, X C V cut copy paste, Z undo, B export.").arg(cmd), 12, MID, L);
}
