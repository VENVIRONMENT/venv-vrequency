// venv-vrequency — the window's keys: every key press goes through handleKey, which sorts out the
// layers (modifiers, tour, chat dock, the page, commands) and then plays the strings.
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include <QKeyEvent>

using namespace td;

void MainWindow::updateBend() {
    Event e; e.type = EV_BEND; e.f = (keys.bendUp ? 1.0f : 0.0f) - (keys.bendDown ? 1.0f : 0.0f);
    inst.push(e);
}

void MainWindow::releaseAllKeys() {
    for (auto it = keys.held.begin(); it != keys.held.end(); ++it)
        if (it.value().kind == HK_STRING) inst.key(it.value().a, it.value().b, false);
        else if (it.value().kind == HK_PAGE && pageOf[it.value().a]) pageOf[it.value().a]->keyUp(it.key());
    keys.held.clear();
    synthPage.releaseTouches();
    keys.chromatic = keys.chord = keys.command = false;
    keys.filterDir = 0;
    if (keys.bendUp || keys.bendDown) { keys.bendUp = keys.bendDown = false; updateBend(); }
    if (keys.susKey) { keys.susKey = false; inst.cmd(EV_SUSTAIN, keys.susLatch ? 1 : 0); }
}

void MainWindow::handleKey(int key, bool down) {
    update();
    // bookkeeping for the key-test view
    if (down) {
        keys.log.prepend(QString("DOWN  %1").arg(keyName(key)));
    } else {
        keys.log.prepend(QString("up    %1").arg(keyName(key)));
    }
    while (keys.log.size() > 12) keys.log.removeLast();

    if (!down) {
        auto it = keys.held.find(key);
        if (it == keys.held.end()) return;
        Held h = it.value();
        keys.held.erase(it);
        if (h.kind == HK_STRING) inst.key(h.a, h.b, false);
        else if (h.kind == HK_PAGE) { if (pageOf[h.a]) pageOf[h.a]->keyUp(key); }
        else if (h.kind == HK_ACT) {
            switch (h.a) {
            case A_SUSTAIN: keys.susKey = false; inst.cmd(EV_SUSTAIN, keys.susLatch ? 1 : 0); break;
            case A_CHROMATIC: keys.chromatic = false; break;
            case A_CHORD_LAYER: keys.chord = false; break;
            case A_COMMAND: keys.command = false; break;
            case A_BEND_UP: keys.bendUp = false; updateBend(); break;
            case A_BEND_DOWN: keys.bendDown = false; updateBend(); break;
            case A_FILTER_OPEN: case A_FILTER_CLOSE: keys.filterDir = 0; break;
            }
        }
        return;
    }
    if (keys.held.contains(key)) return; // already down (missed release or repeat)
    if ((int)keys.held.size() + 1 > keys.maxHeld) keys.maxHeld = keys.held.size() + 1;

    int plainAct = km.plain.value(key, A_NONE);
    // modifiers first, so they work in any layer
    if (plainAct == A_CHROMATIC) { keys.chromatic = true; keys.held[key] = {HK_ACT, A_CHROMATIC, 0}; return; }
    if (plainAct == A_CHORD_LAYER) { keys.chord = true; keys.held[key] = {HK_ACT, A_CHORD_LAYER, 0}; return; }
    if (plainAct == A_COMMAND) { keys.command = true; keys.held[key] = {HK_ACT, A_COMMAND, 0}; return; }

    if (tourView && tourView->promptKey(key)) { keys.held[key] = {HK_NONE, 0, 0}; return; } // Enter / Esc answer the first-run offer
    if (tourOpen()) { // the tour is a picture: its keys never reach the synth
        tourView->key(key, keys.text, keys.chromatic);
        keys.text.clear();
        keys.held[key] = {HK_NONE, 0, 0};
        return;
    }
    if (guide.dockOpen && pg != PG_CHAT && !keys.testOpen && !keys.command) { // the chat dock
        if (guide.dockFocus && chat.key(key, plainAct, true)) { keys.held[key] = {HK_NONE, 0, 0}; return; }
        if (!guide.dockFocus && plainAct == A_HOME) { guide.dockClose(); keys.held[key] = {HK_NONE, 0, 0}; return; } // Esc again closes it
    }
    if (pageOf[pg] && !keys.testOpen && pageOf[pg]->key(key, plainAct)) { keys.held[key] = {HK_PAGE, pg, 0}; return; }
    if (keys.command) {
        const int act = km.command.value(key, A_NONE);
        if (act == A_FILTER_OPEN || act == A_FILTER_CLOSE) { // held: sweeps until the key comes up
            keys.filterDir = act == A_FILTER_OPEN ? 1 : -1;
            keys.held[key] = {HK_ACT, act, 0};
            return;
        }
        perform(act, keys.chromatic);
        keys.held[key] = {HK_NONE, 0, 0};
        return;
    }
    int row, col;
    if (km.stringKey(key, row, col)) {
        if (row == 0 && (keys.chordRow || keys.chord)) {
            if (col < 7) inst.cmd(EV_CHORD, col);
            keys.held[key] = {HK_NONE, 0, 0};
            return;
        }
        inst.key(row, col, true, keys.chromatic);
        keys.held[key] = {HK_STRING, row, col};
        return;
    }
    keys.held[key] = {HK_ACT, plainAct, 0};
    switch (plainAct) {
    case A_SUSTAIN: keys.susKey = true; inst.cmd(EV_SUSTAIN, 1); break;
    case A_BEND_UP: keys.bendUp = true; updateBend(); break;
    case A_BEND_DOWN: keys.bendDown = true; updateBend(); break;
    default: perform(plainAct, keys.chromatic); break;
    }
}

void MainWindow::keyPressEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) return; // a held key is one note, not many
    keys.text = e->text();
    handleKey(e->key(), true);
    keys.text.clear();
}

void MainWindow::keyReleaseEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) return;
    handleKey(e->key(), false);
}

void MainWindow::focusOutEvent(QFocusEvent *) { releaseAllKeys(); }
