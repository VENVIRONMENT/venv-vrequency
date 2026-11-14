// venv-vrequency — the Track page: the lane mixer, its faders, meters, mutes and solos.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::trackMixer() {
    mixOpen = !mixOpen;
    if (mixOpen) {
        chopOpen = false;
        if (soundOpen) { soundOpen = false; hearEnd(true); }
        if (selLane >= 0) mixLane = selLane;
    }
}

void TrackPage::mixApply(int lane, const std::function<bool(LaneMix &)> &fn) {
    if (lane < 0 || lane >= kLanes) return;
    mixLane = lane;
    trackApply([&](TrackData &t) {
        LaneMix m = t.mix[lane];
        if (!fn(m) || sameMix(m, t.mix[lane])) return false;
        t.mix[lane] = m;
        return true;
    });
}

void TrackPage::mixReset() {
    if (!trackApply([](TrackData &t) {
            bool changed = false;
            for (int ln = 0; ln < kLanes; ++ln) {
                changed = changed || !sameMix(t.mix[ln], LaneMix());
                t.mix[ln] = LaneMix();
            }
            return changed;
        }))
        toast("Every lane is already at 80", 1200);
}

// A fader follows the finger; letting go makes the whole move one undo step.
void TrackPage::mixFader(int lane, qreal y, qreal top, qreal bottom, int ph) {
    if (ph == PH_RELEASE) {
        if (mixDragged) {
            trackUndo.push_back(mixBefore);
            if (trackUndo.size() > 40) trackUndo.erase(trackUndo.begin());
            trackRedo.clear();
        }
        mixDragged = false;
        return;
    }
    if (ph == PH_PRESS) { mixBefore = track; mixDragged = false; mixLane = lane; }
    int v = qBound(0, (int)std::lround((bottom - y) / (bottom - top) * 100.0), 100);
    if (std::abs(v - kLaneLevelNormal) <= 2) v = kLaneLevelNormal; // easy to find "as recorded" again
    if (v == track.mix[lane].level) return;
    track.mix[lane].level = v;
    mixDragged = true;
    trackSync();
}

void TrackPage::paintMixer() {
    const int L = Qt::AlignLeft | Qt::AlignVCenter;
    mixLane = qBound(0, mixLane, kLanes - 1);
    QString solos;
    for (int ln = 0; ln < kLanes; ++ln)
        if (track.mix[ln].solo) solos += (solos.isEmpty() ? "" : ", ") + QString(tl::laneLabel(ln));
    ui.section(414, solos.isEmpty() ? QString("Mixer   %1 chosen   tap or drag a fader").arg(tl::laneLabel(mixLane))
                                 : QString("Mixer   %1 chosen   only %2 heard").arg(tl::laneLabel(mixLane)).arg(solos));

    const qreal W = 132, G = 7, y0 = 438, y1 = 638;
    for (int ln = 0; ln < kLanes; ++ln) {
        const LaneMix &m = track.mix[ln];
        const bool chosenLane = ln == mixLane, heard = laneHeard(track, ln);
        const qreal x = 16 + ln * (W + G);
        const QRectF r(x, y0, W, y1 - y0);
        ui.frame(r, chosenLane ? INK : LOW, BG);
        const QRectF hd(x, y0, W, 24);
        if (chosenLane) ui.g->fillRect(hd, INK);
        ui.txt(hd, tl::laneLabel(ln), 13, chosenLane ? BG : (heard ? INK : LOW), Qt::AlignCenter, true);
        ui.g->fillRect(QRectF(x + 1, y0 + 24, W - 2, 1), GRAT);
        ui.zone(hd, QString("mixer_lane%1").arg(ln), [this, ln](QPointF, int ph) { if (ph == PH_PRESS) mixLane = ln; });

        // the fader: the bar is the level; the notch beside it is 80, as recorded
        const qreal top = y0 + 36, bottom = y0 + 150;
        const QRectF fr(x + 24, top, 38, bottom - top);
        ui.frame(fr, heard ? LOW : GRAT, BG);
        const qreal h = std::floor((fr.height() - 6) * m.level / 100.0);
        ui.g->fillRect(QRectF(fr.x() + 3, fr.bottom() - 3 - h, fr.width() - 6, h), heard ? INK : LOW);
        const qreal ny = std::floor(fr.bottom() - 3 - (fr.height() - 6) * kLaneLevelNormal / 100.0);
        ui.g->fillRect(QRectF(fr.x() - 7, ny, 5, 1), MID);
        ui.txt(QRectF(x + 90, top, 40, 22), QString::number(m.level), 17, heard ? INK : LOW, L, true);
        ui.txt(QRectF(x + 90, top + 22, 40, 16), m.level == kLaneLevelNormal ? "as is" : (m.level > kLaneLevelNormal ? "louder" : "softer"), 11, MID, L);

        // the meter: twenty steps from -48 dB to full scale, only while the track plays
        const QRectF mr(x + 72, top, 12, bottom - top);
        ui.frame(mr, GRAT, BG);
        const float pk = u.trackPlaying ? u.lanePeak[ln] : 0.0f;
        const float db = pk > 1e-6f ? 20.0f * std::log10(pk) : -120.0f;
        const qreal seg = (mr.height() - 4) / 20.0;
        for (int sgm = 0; sgm < 20; ++sgm) {
            const bool lit = db > -48.0f + sgm * 2.4f;
            const qreal sy = std::floor(mr.bottom() - 2 - (sgm + 1) * seg);
            ui.g->fillRect(QRectF(mr.x() + 2, sy + 1, mr.width() - 4, std::max<qreal>(1, std::floor(seg) - 1)), lit ? INK : GRAT);
        }
        ui.zone(QRectF(x + 4, top - 8, 82, bottom - top + 16), QString("mixer_fader%1").arg(ln),
             [this, ln, top, bottom](QPointF p, int ph) { mixFader(ln, p.y(), top + 3, bottom - 3, ph); });

        const qreal by = y0 + 160, bw = 56;
        ui.button(QRectF(x + 8, by, bw, 34), QString("mixer_mute%1").arg(ln), "M", m.mute,
               [this, ln] { mixApply(ln, [](LaneMix &mm) { mm.mute = !mm.mute; return true; }); });
        ui.button(QRectF(x + 8 + bw + 4, by, bw, 34), QString("mixer_solo%1").arg(ln), "S", m.solo,
               [this, ln] { mixApply(ln, [](LaneMix &mm) { mm.solo = !mm.solo; return true; }); });
    }

    const qreal x = 16 + kLanes * (W + G), w = 704 - x;
    const bool cyc = track.cycleTo > track.cycleFrom;
    const int from = cyc && (playBar < track.cycleFrom || playBar >= track.cycleTo) ? track.cycleFrom : qMin(playBar, track.bars - 1);
    ui.button(QRectF(x, y0, w, 62), "mixer_play", u.trackPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.trackPlaying,
           [this] { trackPlayStop(); }, u.trackPlaying ? QString("bar %1").arg((int)(u.trackPos / 4.0) + 1) : QString("from bar %1").arg(from + 1));
    ui.button(QRectF(x, y0 + 68, w, 62), "mixer_reset", "Reset", false, [this] { mixReset(); }, "all back to 80");
    ui.button(QRectF(x, y0 + 136, w, 64), "mixer_done", "Done", false, [this] { mixOpen = false; }, "back to tools");

    ui.txt(QRectF(18, 652, 684, 18), QString("Keys: left right choose a lane (or 1-4), up down its level, 0 back to 80,"), 12, MID, L);
    ui.txt(QRectF(18, 670, 684, 18), QString("M mute, S solo, %1 play, %2 or X done. %3 + Z undoes a change.")
            .arg(sh.km.keyFor(A_SUSTAIN, false)).arg(sh.km.keyFor(A_HOME, false)).arg(sh.km.keyFor(A_COMMAND, false)), 12, MID, L);
    ui.txt(QRectF(18, 692, 684, 18), "Levels, mutes and solos are kept with the track and heard in an export.", 12, LOW, L);
}
