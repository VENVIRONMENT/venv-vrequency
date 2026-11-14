// venv-vrequency self-test — choosing, generating, undoing and keeping sounds.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "core/presets/presets.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QDir>
#include <cstdio>

using namespace td;

namespace selftest {
void testSounds(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- sounds\n");
    key(w, Qt::Key_2, true); key(w, Qt::Key_2, false); wait(80);
    check(QString(inst.uiState().leadName) == "Glass Bells", "key 2 selects sound 2");
    QString before = w.sounds.lead.name;
    key(w, Qt::Key_Control, true); key(w, Qt::Key_R, true); key(w, Qt::Key_R, false); wait(80);
    QString rnd = inst.uiState().leadName;
    check(rnd != before && !rnd.isEmpty(), QString("Ctrl+R generates a new sound (%1)").arg(rnd));
    QDir favDir(store::baseDir() + "/favourites");
    int favBefore = favDir.entryList({"*.json"}).size();
    check(favBefore == kExtraCount && favDir.exists("haze.json") && favDir.exists("grain.json"),
          QString("first start puts the %1 dream sounds in favourites").arg(kExtraCount));
    key(w, Qt::Key_F, true); key(w, Qt::Key_F, false); wait(60);
    check(favDir.entryList({"*.json"}).size() == favBefore + 1, "Ctrl+F saves it as a favourite file");
    key(w, Qt::Key_Z, true); key(w, Qt::Key_Z, false); wait(80);
    check(QString(inst.uiState().leadName) == before, "Ctrl+Z undoes back to the previous sound");
    key(w, Qt::Key_Control, false);
    w.setPage(PG_SOUND); wait(100);
    tap(w, zn::at(zn::favitem, 0)); wait(80);
    check(QString(inst.uiState().leadName) == rnd, "favourite recalled from the sound page");
    tap(w, zn::at(zn::favitem, 1)); wait(80);
    check(QString(inst.uiState().leadName) == "Haze", "the dream sounds follow in order: Haze is next");
    tap(w, zn::at(zn::favitem, 5)); wait(80);
    check(QString(inst.uiState().leadName) == "Grain", "and Grain is last on the first page");
    check(w.sounds.addExtraSounds() == 0 && favDir.entryList({"*.json"}).size() == favBefore + 1, "they are not added twice");
    w.setPage(PG_PLAY); wait(100);
    key(w, Qt::Key_1, true); key(w, Qt::Key_1, false); wait(60);
}
} // namespace selftest
