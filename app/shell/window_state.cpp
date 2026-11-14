// venv-vrequency — the window's saving and loading: the state file, and sessions.
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include <QJsonArray>
#include <memory>

using namespace td;

// The state file, read at start-up: settings, sounds, the track, the beats and the chat. Nothing
// in a scratch window, and the beats are set up even when there is no file.
void MainWindow::loadState(SessionData &sd) {
    QJsonObject st;
    if (!scratchWindow && store::readJson(store::baseDir() + "/state.json", st)) {
        sd.settings = store::settingsFromJson(st["settings"].toObject());
        sounds.load(st);
        keys.chordRow = st["chordRow"].toBool(false);
        store::beatFromJson(st["beat"].toArray(), sd.beat);
        if (st.contains("track")) store::trackFromJson(st["track"].toObject(), trackPage.track);
        chat.load(st);
        tourSeenFlag = st["tourSeen"].toBool(false);
        tourLastPage = st["tourPage"].toString();
    }
    beatPage.load(st, sd);
}

void MainWindow::saveState() {
    if (scratchWindow) return;
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o;
    o["settings"] = store::settingsToJson(sd.settings);
    sounds.save(o);
    o["chordRow"] = keys.chordRow;
    o["beat"] = store::beatToJson(sd.beat);
    o["track"] = store::trackToJson(trackPage.track);
    beatPage.save(o);
    chat.save(o);
    o["tourSeen"] = tourSeenFlag;
    if (!tourLastPage.isEmpty()) o["tourPage"] = tourLastPage;
    store::writeJson(store::baseDir() + "/state.json", o);
}

bool MainWindow::saveSession(const QString &path) {
    SessionData sd;
    inst.snapshot(sd);
    QJsonObject o = store::sessionToJson(sd);
    o["track"] = store::trackToJson(trackPage.track);
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
    if (o.contains("track")) { // sessions saved before the track existed leave the track alone
        trackPage.loadJson(o["track"].toObject());
    }
    return true;
}
