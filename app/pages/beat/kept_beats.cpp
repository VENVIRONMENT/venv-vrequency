// venv-vrequency — kept beats on the Beat page.
//
// Every beat you make keeps its own place in a row under the pads, like the patterns of a drum
// machine. The engine plays one beat at a time: the one shown. Edits to it are copied into its
// place as they happen, so there is nothing to save by hand; + New starts an empty beat and keeps
// the others; tapping another switches to it (on the next bar line while the beat plays).
// They are written to the state file with everything else.

#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include "app/store/store.h"

#include <QJsonArray>
#include <QJsonObject>

using namespace td;
using namespace pal;

namespace {
bool emptyBeat(const uint16_t *p) {
    for (int d = 0; d < kDrums; ++d) if (p[d]) return false;
    return true;
}
bool samePattern(const uint16_t *a, const uint16_t *b) {
    for (int d = 0; d < kDrums; ++d) if (a[d] != b[d]) return false;
    return true;
}
constexpr int kCells = 8; // places shown in the row
} // namespace

void MainWindow::beatsLoad(const QJsonObject &st, const SessionData &sd) {
    beatSlots.clear();
    for (const QJsonValue &v : st["beats"].toArray()) {
        const QJsonObject o = v.toObject();
        BeatSlot b;
        b.name = o["name"].toString().left(12);
        b.kit = qBound(0, o["kit"].toInt(0), kKits - 1);
        store::beatFromJson(o["beat"].toArray(), b.pat);
        if (b.name.isEmpty()) b.name = QString("Beat %1").arg(beatSlots.size() + 1);
        beatSlots.push_back(b);
    }
    if (beatSlots.empty()) { // the first time: the beat you already have becomes Beat 1
        BeatSlot b;
        b.name = "Beat 1";
        b.kit = sd.settings.kit;
        std::copy(sd.beat, sd.beat + kDrums, b.pat);
        beatSlots.push_back(b);
    }
    beatCur = qBound(0, st["beatShown"].toInt(0), (int)beatSlots.size() - 1);
    beatNameNext = qMax((int)beatSlots.size() + 1, st["beatNameNext"].toInt(0));
    // the one shown is the one the engine starts with (the state's own beat, if they ever differ)
    BeatSlot &cur = beatSlots[beatCur];
    std::copy(sd.beat, sd.beat + kDrums, cur.pat);
    cur.kit = sd.settings.kit;
}

void MainWindow::beatsSave(QJsonObject &o) {
    QJsonArray a;
    for (const BeatSlot &b : beatSlots) {
        QJsonObject bo;
        bo["name"] = b.name;
        bo["kit"] = b.kit;
        bo["beat"] = store::beatToJson(b.pat);
        a.append(bo);
    }
    o["beats"] = a;
    o["beatShown"] = beatCur;
    o["beatNameNext"] = beatNameNext;
}

// Every frame: what the engine plays is the beat shown, so copy it into its place.
void MainWindow::beatWatch() {
    if (beatSlots.empty() || u.beatSwitching) return;
    BeatSlot &cur = beatSlots[beatCur];
    if (beatHoldUntil) { // just switched: wait until the engine has the new beat
        if ((samePattern(u.beatPat, cur.pat) && u.set.kit == cur.kit) || clock.elapsed() > beatHoldUntil) beatHoldUntil = 0;
        return;
    }
    if (samePattern(u.beatPat, cur.pat) && u.set.kit == cur.kit) return;
    std::copy(u.beatPat, u.beatPat + kDrums, cur.pat);
    cur.kit = u.set.kit;
    trackSaveTimer.start(2500); // on disk a moment later, like a track edit
}

void MainWindow::beatSwitchTo(int i) {
    if (i < 0 || i >= (int)beatSlots.size()) return;
    beatPage = beatSlots.size() > kCells - 1 ? i / (kCells - 2) : 0;
    if (i == beatCur && !u.beatSwitching) return;
    if (!beatHoldUntil && !u.beatSwitching) { // keep what is playing now in its own place first
        BeatSlot &cur = beatSlots[beatCur];
        std::copy(u.beatPat, u.beatPat + kDrums, cur.pat);
        cur.kit = u.set.kit;
    }
    beatCur = i;
    const BeatSlot &b = beatSlots[i];
    for (int d = 0; d < kDrums; ++d) inst.beatDo(BT_NEXTROW, d, (int16_t)b.pat[d]);
    inst.beatDo(BT_SWITCH, -1, 0, b.kit);
    beatErase = false;
    beatHoldUntil = clock.elapsed() + 10000;
    trackSaveTimer.start(2500);
    if (u.beatPlaying) toast(QString("%1 starts at the next bar").arg(b.name), 1400);
}

void MainWindow::beatNew() {
    if (emptyBeat(beatSlots[beatCur].pat) && !u.beatSwitching && emptyBeat(u.beatPat)) {
        toast(QString("%1 is empty already: tap the pads").arg(beatSlots[beatCur].name), 1800);
        return;
    }
    BeatSlot b;
    b.name = QString("Beat %1").arg(beatNameNext++);
    b.kit = u.set.kit;
    beatSlots.push_back(b);
    beatSwitchTo((int)beatSlots.size() - 1);
    toast(u.beatPlaying ? QString("%1 starts empty at the next bar; the others are kept").arg(b.name)
                        : QString("%1: an empty beat. The others are kept").arg(b.name), 2000);
}

// The row: with seven beats or fewer, all of them and + New after them. With more, six a page, More and + New.
void MainWindow::paintKeptBeats(qreal y) {
    const int n = (int)beatSlots.size();
    const bool paged = n > kCells - 1;
    const int per = paged ? kCells - 2 : kCells - 1, pages = qMax(1, (n + per - 1) / per);
    beatPage = qBound(0, beatPage, pages - 1);
    QString head = QString("Beats  %1 kept   tap one to switch, + New for a fresh one").arg(n);
    if (u.beatSwitching) head = QString("Beats  %1 starts at the next bar").arg(beatSlots[beatCur].name);
    section(y, head);
    const qreal cw = (688 - (kCells - 1) * 6) / (qreal)kCells, top = y + 20;
    const int newCell = paged ? kCells - 1 : n; // + New sits right after the last beat
    for (int cell = 0; cell < kCells; ++cell) {
        const QRectF r(std::floor(16 + cell * (cw + 6)), top, std::floor(cw), 48);
        if (cell == newCell) { button(r, "beatnew", "+ New", false, [this] { beatNew(); }, "empty beat"); continue; }
        if (paged && cell == kCells - 2) {
            button(r, "beatmore", "More", false, [this, pages] { beatPage = (beatPage + 1) % pages; }, QString("page %1/%2").arg(beatPage + 1).arg(pages));
            continue;
        }
        const int i = beatPage * per + cell;
        if (cell >= per || i >= n) { frame(r, GRAT); continue; }
        const BeatSlot &b = beatSlots[i];
        const bool on = i == beatCur;
        const QString zn = QString("beatslot%1").arg(cell);
        const bool waiting = on && u.beatSwitching;
        button(r, zn, b.name, on && !waiting, [this, i] { beatSwitchTo(i); },
               waiting ? QString("next bar") : (emptyBeat(b.pat) ? QString("empty") : nice(kitName(b.kit))), waiting ? LOOK_ARMED : LOOK_PLAIN);
        txt(QRectF(r.x() + 4, r.y() + 2, 14, 13), QString::number(i + 1), 10, on && !waiting ? BG : LOW, Qt::AlignLeft | Qt::AlignVCenter);
    }
}
