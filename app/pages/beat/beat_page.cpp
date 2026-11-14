// venv-vrequency — the Beat page (see beat_page.h): its keys, the pad hits and the transport.
#include "app/pages/beat/beat_page.h"
#include "app/ui/palette.h"
#include <cmath>

using namespace td;
using namespace pal;

// On this page the two upper letter rows are the pads; the low string still plays.
bool BeatPage::key(int qtKey, int plainAct) {
    if (sh.keys.command) return false;       // Ctrl+key belongs to the command layer
    int drum, r, c;
    if (sh.km.drumKey(qtKey, drum)) { hitPad(drum); return true; }
    if (sh.km.stringKey(qtKey, r, c) && r != 0) return true;   // between the pad keys: nothing
    if (plainAct == A_SUSTAIN) { inst.beatDo(BT_PLAYSTOP); return true; }
    if (plainAct == A_LOOP) { inst.beatDo(BT_REC); return true; }
    if (plainAct == A_BEND_DOWN || plainAct == A_BEND_UP) { // the beat before or after
        switchTo(qBound(0, cur + (plainAct == A_BEND_UP ? 1 : -1), (int)beats.size() - 1));
        return true;
    }
    return false;
}

void BeatPage::hitPad(int drum) {
    if (drum < 0 || drum >= kDrums) return;
    if (erase) {
        inst.beatDo(BT_ERASE, drum);
        erase = false;
        sh.toast(QString("%1 erased").arg(nice(drumName(drum))), 900);
        return;
    }
    inst.hit(drum);
    padLit[drum] = sh.clock.elapsed();
    sh.redraw();
}

void BeatPage::paint() {
    const qint64 now = sh.clock.elapsed();
    for (int d = 0; d < kDrums; ++d)
        if (u.drumHits[d] != seenHits[d]) { seenHits[d] = u.drumHits[d]; padLit[d] = now; }
    paintGrid();
    paintPads(now);
    paintKept(460);
    paintTransport();
}

void BeatPage::paintTransport() {
    const qreal pw = (688 - 3 * 8) / 4.0;
    auto c4 = [pw](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (pw + 8)), y, std::floor(pw), h); };
    ui.button(c4(0, 540, 56), "beatrec", QString::fromUtf8("● Rec"), u.beatRec, [this] { inst.beatDo(BT_REC); erase = false; },
           u.beatRec ? "pads write hits" : "write what you play");
    ui.button(c4(1, 540, 56), "beatplay", u.beatPlaying ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play"), u.beatPlaying,
           [this] { inst.beatDo(BT_PLAYSTOP); }, u.beatPlaying ? "beat is playing" : "start the beat");
    ui.button(c4(2, 540, 56), "beaterase", "Erase", false, [this] { erase = !erase; }, erase ? "now tap a pad" : "one drum's hits",
           erase ? LOOK_ARMED : LOOK_PLAIN);
    ui.stepper(c4(3, 540, 56), "kit", "Kit", nice(kitName(u.set.kit)),
            [this] { sh.set(S_KIT, (float)((u.set.kit + kKits - 1) % kKits)); }, [this] { sh.set(S_KIT, (float)((u.set.kit + 1) % kKits)); });
    ui.slider(QRectF(16, 604, 340, 54), "beatvol", "Beat volume", u.set.beatVol, QString("%1%").arg((int)(u.set.beatVol * 100)), [this](float n) { sh.set(S_BEATVOL, n); });
    ui.stepper(QRectF(364, 604, 340, 54), "bpm3", "Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)),
            [this] { sh.set(S_BPM, std::round(u.set.bpm) - 1); }, [this] { sh.set(S_BPM, std::round(u.set.bpm) + 1); });
    const KeyMap &km = sh.km;
    ui.txt(QRectF(18, 666, 684, 20), QString("Keys: %1 play the top pads, %2 the bottom ones. Hits snap to the nearest step.")
            .arg(km.rows[2].left(4).split("", Qt::SkipEmptyParts).join(" ")).arg(km.rows[1].left(4).split("", Qt::SkipEmptyParts).join(" ")),
        12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 688, 684, 20), QString("%1 starts and stops the beat, %2 is Rec, left right the beat before or after.")
            .arg(km.keyFor(A_SUSTAIN, false)).arg(km.keyFor(A_LOOP, false)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
}
