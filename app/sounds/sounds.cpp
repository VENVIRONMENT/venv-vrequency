// venv-vrequency — the player's sounds (see sounds.h).
#include "app/sounds/sounds.h"
#include "core/presets/presets.h"
#include <QDateTime>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>

using namespace td;

namespace {
const size_t kUndoSteps = 24;
}

Sounds::Sounds(Shell &s) : Part(s) {
    rng.s = (uint32_t)QDateTime::currentMSecsSinceEpoch() | 1u;
    factoryReset();
    lead = bank[0];
}

void Sounds::factoryReset() {
    for (int k = 0; k < 10; ++k) bank[k] = factoryPatch(k);
}

void Sounds::load(const QJsonObject &st) {
    QJsonArray sl = st["slots"].toArray();
    for (int k = 0; k < 10 && k < sl.size(); ++k) bank[k] = store::patchFromJson(sl[k].toObject());
    curSlot = qBound(-1, st["slot"].toInt(0), 9);
    lead = st.contains("lead") ? store::patchFromJson(st["lead"].toObject()) : bank[qMax(0, curSlot)];
    extrasSeeded = st["extras"].toInt(0);
}

void Sounds::save(QJsonObject &o) const {
    QJsonArray sl;
    for (const Patch &p : bank) sl.append(store::patchToJson(p));
    o["slots"] = sl;
    o["slot"] = curSlot;
    o["lead"] = store::patchToJson(lead);
    o["extras"] = extrasSeeded;
}

// The engine's extra sounds (the sine-based dream sounds) live in the favourites folder, so they
// can be recalled, stored to a slot or deleted like any saved sound. A sound whose name is already
// there is left alone. Returns how many files were written.
int Sounds::addExtraSounds(int from) {
    const QVector<store::NamedPatch> have = store::listPatches("favourites");
    const QDateTime now = QDateTime::currentDateTime();
    int wrote = 0;
    for (int i = qMax(0, from); i < kExtraCount; ++i) {
        Patch p = extraPatch(i);
        bool there = false;
        for (const store::NamedPatch &f : have) there = there || QString::fromUtf8(f.patch.name) == QString::fromUtf8(p.name);
        if (there) continue;
        QString path = store::savePatch("favourites", p);
        if (path.isEmpty()) continue;
        // The list shows the newest file first: date them so they appear in their own order.
        QFile f(path);
        if (f.open(QIODevice::ReadWrite)) f.setFileTime(now.addSecs(-1 - i), QFileDevice::FileModificationTime);
        ++wrote;
    }
    extrasSeeded = kExtraCount;
    return wrote;
}

void Sounds::pushUndo() {
    undo.push_back(lead);
    if (undo.size() > kUndoSteps) undo.erase(undo.begin());
}

void Sounds::applyLead(bool resetSlot) {
    if (resetSlot) curSlot = -1;
    inst.setPatch(0, lead);
}

void Sounds::selectSlot(int i) {
    if (i < 0 || i > 9) return;
    if (slotSave) {
        bank[i] = lead; curSlot = i; slotSave = false;
        sh.toast(QString("Stored in slot %1").arg(i + 1));
        sh.saveState();
        return;
    }
    pushUndo();
    lead = bank[i]; curSlot = i;
    applyLead(false);
}

void Sounds::randomise() {
    pushUndo();
    lead = randomPatch(rng);
    applyLead(true);
    sh.toast(QString("New sound: %1").arg(lead.name), 1400);
}

void Sounds::mutate() {
    pushUndo();
    QByteArray nm(lead.name);
    lead = mutatePatch(lead, rng, 0.3f);
    if (!nm.endsWith("*") && nm.size() < 26) nm += "*";
    lead.setName(nm.constData());
    applyLead(true);
}

void Sounds::undoSound() {
    if (undo.empty()) { sh.toast("Nothing to undo"); return; }
    lead = undo.back(); undo.pop_back();
    applyLead(true);
    sh.toast(QString("Back to: %1").arg(lead.name), 1200);
}

void Sounds::recall(const Patch &p) {
    pushUndo();
    lead = p;
    applyLead(true);
}

void Sounds::saveFavourite() {
    bool updated = false;
    QString path = store::savePatch("favourites", lead, true, &updated);
    sh.refreshLists();
    sh.toast(path.isEmpty() ? QString("Could not save favourite")
                         : QString(updated ? "Updated %1" : "Favourite saved: %1").arg(QString::fromUtf8(lead.name)));
}

void Sounds::setParam(int p, float v) {
    lead.p[p] = v;
    Event e; e.type = EV_PATCHPARAM; e.a = (int16_t)p; e.f = v;
    inst.push(e);
}
