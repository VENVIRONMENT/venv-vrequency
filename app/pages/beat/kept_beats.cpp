// venv-vrequency — kept beats on the Beat page.
//
// Every beat you make keeps its own place in a row under the pads, like the patterns of a drum
// machine. The engine plays one beat at a time: the one shown. Edits to it are copied into its
// place as they happen, so there is nothing to save by hand; + New starts an empty beat and keeps
// the others; tapping another switches to it (on the next bar line while the beat plays).
// They are written to the state file with everything else.

#include "app/pages/beat/beat_page.h"
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

void BeatPage::load(const QJsonObject &st, const SessionData &sd) {
    beats.clear();
    for (const QJsonValue &v : st["beats"].toArray()) {
        const QJsonObject o = v.toObject();
        Slot b;
        b.name = o["name"].toString().left(12);
        b.kit = qBound(0, o["kit"].toInt(0), kKits - 1);
        store::beatFromJson(o["beat"].toArray(), b.pat);
        if (b.name.isEmpty()) b.name = QString("Beat %1").arg(beats.size() + 1);
        beats.push_back(b);
    }
    if (beats.empty()) { // the first time: the beat you already have becomes Beat 1
        Slot b;
        b.name = "Beat 1";
        b.kit = sd.settings.kit;
        std::copy(sd.beat, sd.beat + kDrums, b.pat);
        beats.push_back(b);
    }
    cur = qBound(0, st["beatShown"].toInt(0), (int)beats.size() - 1);
    nameNext = qMax((int)beats.size() + 1, st["beatNameNext"].toInt(0));
    // the one shown is the one the engine starts with (the state's own beat, if they ever differ)
    Slot &here = beats[cur];
    std::copy(sd.beat, sd.beat + kDrums, here.pat);
    here.kit = sd.settings.kit;
}

void BeatPage::save(QJsonObject &o) const {
    QJsonArray a;
    for (const Slot &b : beats) {
        QJsonObject bo;
        bo["name"] = b.name;
        bo["kit"] = b.kit;
        bo["beat"] = store::beatToJson(b.pat);
        a.append(bo);
    }
    o["beats"] = a;
    o["beatShown"] = cur;
    o["beatNameNext"] = nameNext;
}

// Every frame: what the engine plays is the beat shown, so copy it into its place.
void BeatPage::watch() {
    if (beats.empty() || u.beatSwitching) return;
    Slot &here = beats[cur];
    if (holdUntil) { // just switched: wait until the engine has the new beat
        if ((samePattern(u.beatPat, here.pat) && u.set.kit == here.kit) || sh.clock.elapsed() > holdUntil) holdUntil = 0;
        return;
    }
    if (samePattern(u.beatPat, here.pat) && u.set.kit == here.kit) return;
    std::copy(u.beatPat, u.beatPat + kDrums, here.pat);
    here.kit = u.set.kit;
    sh.saveSoon(); // on disk a moment later, like a track edit
}

void BeatPage::switchTo(int i) {
    if (i < 0 || i >= (int)beats.size()) return;
    row = beats.size() > kCells - 1 ? i / (kCells - 2) : 0;
    if (i == cur && !u.beatSwitching) return;
    if (!holdUntil && !u.beatSwitching) { // keep what is playing now in its own place first
        Slot &here = beats[cur];
        std::copy(u.beatPat, u.beatPat + kDrums, here.pat);
        here.kit = u.set.kit;
    }
    cur = i;
    const Slot &b = beats[i];
    for (int d = 0; d < kDrums; ++d) inst.beatDo(BT_NEXTROW, d, (int16_t)b.pat[d]);
    inst.beatDo(BT_SWITCH, -1, 0, b.kit);
    erase = false;
    holdUntil = sh.clock.elapsed() + 10000;
    sh.saveSoon();
    if (u.beatPlaying) sh.toast(QString("%1 starts at the next bar").arg(b.name), 1400);
}

void BeatPage::newBeat() {
    if (emptyBeat(beats[cur].pat) && !u.beatSwitching && emptyBeat(u.beatPat)) {
        sh.toast(QString("%1 is empty already: tap the pads").arg(beats[cur].name), 1800);
        return;
    }
    Slot b;
    b.name = QString("Beat %1").arg(nameNext++);
    b.kit = u.set.kit;
    beats.push_back(b);
    switchTo((int)beats.size() - 1);
    sh.toast(u.beatPlaying ? QString("%1 starts empty at the next bar; the others are kept").arg(b.name)
                        : QString("%1: an empty beat. The others are kept").arg(b.name), 2000);
}

// The row: with seven beats or fewer, all of them and + New after them. With more, six a page, More and + New.
void BeatPage::paintKept(qreal y) {
    const int n = (int)beats.size();
    const bool paged = n > kCells - 1;
    const int per = paged ? kCells - 2 : kCells - 1, pages = qMax(1, (n + per - 1) / per);
    row = qBound(0, row, pages - 1);
    QString head = QString("Beats  %1 kept   tap one to switch, + New for a fresh one").arg(n);
    if (u.beatSwitching) head = QString("Beats  %1 starts at the next bar").arg(beats[cur].name);
    ui.section(y, head);
    const qreal cw = (688 - (kCells - 1) * 6) / (qreal)kCells, top = y + 20;
    const int newCell = paged ? kCells - 1 : n; // + New sits right after the last beat
    for (int cell = 0; cell < kCells; ++cell) {
        const QRectF r(std::floor(16 + cell * (cw + 6)), top, std::floor(cw), 48);
        if (cell == newCell) { ui.button(r, "beatnew", "+ New", false, [this] { newBeat(); }, "empty beat"); continue; }
        if (paged && cell == kCells - 2) {
            ui.button(r, "beatmore", "More", false, [this, pages] { row = (row + 1) % pages; }, QString("page %1/%2").arg(row + 1).arg(pages));
            continue;
        }
        const int i = row * per + cell;
        if (cell >= per || i >= n) { ui.frame(r, GRAT); continue; }
        const Slot &b = beats[i];
        const bool on = i == cur;
        const QString zn = QString("beatslot%1").arg(cell);
        const bool waiting = on && u.beatSwitching;
        ui.button(r, zn, b.name, on && !waiting, [this, i] { switchTo(i); },
               waiting ? QString("next bar") : (emptyBeat(b.pat) ? QString("empty") : nice(kitName(b.kit))), waiting ? LOOK_ARMED : LOOK_PLAIN);
        ui.txt(QRectF(r.x() + 4, r.y() + 2, 14, 13), QString::number(i + 1), 10, on && !waiting ? BG : LOW, Qt::AlignLeft | Qt::AlignVCenter);
    }
}
