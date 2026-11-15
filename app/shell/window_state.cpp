// venv-vrequency — the window's saving and loading: the state file, and sessions.
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include <QJsonArray>

using namespace td;

// The state file, read at start-up: settings, sounds, the pulse pads and the chat. Nothing
// in a scratch window. An old file's "beat" and "beats" are left out.
void MainWindow::loadState(SessionData &sd) {
    QJsonObject st;
    if (!scratchWindow && store::readJson(store::baseDir() + "/state.json", st)) {
        sd.settings = store::settingsFromJson(st["settings"].toObject());
        sounds.load(st);
        keys.chordRow = st["chordRow"].toBool(false);
        store::pulseFromJson(st["pulse"].toArray(), sd.pulse);
        chat.load(st);
        tourSeenFlag = st["tourSeen"].toBool(false);
        tourLastPageName = st["tourPage"].toString();
    }
    pulsePage.load(st);
}

void MainWindow::saveState() {
    if (scratchWindow) return;
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o;
    o["settings"] = store::settingsToJson(sd.settings);
    sounds.save(o);
    o["chordRow"] = keys.chordRow;
    o["pulse"] = store::pulseToJson(sd.pulse);
    pulsePage.save(o);
    chat.save(o);
    o["tourSeen"] = tourSeenFlag;
    if (!tourLastPageName.isEmpty()) o["tourPage"] = tourLastPageName;
    store::writeJson(store::baseDir() + "/state.json", o);
}

bool MainWindow::saveSession(const QString &path) {
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o = store::sessionToJson(sd);
    bool ok = store::writeJson(path, o);
    refreshLists();
    return ok;
}

bool MainWindow::loadSession(const QString &path) {
    QJsonObject o;
    SessionData sd;
    if (!store::readJson(path, o) || !store::sessionFromJson(o, sd)) return false;
    releaseAllKeys();
    inst.restore(sd);
    sounds.lead = sd.lead; sounds.curSlot = -1;
    return true;
}
