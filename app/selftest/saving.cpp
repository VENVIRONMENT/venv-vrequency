// venv-vrequency self-test — saving: a WAV recording, a session and the state files.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include <QFile>
#include <cstdio>

using namespace td;

namespace selftest {
void testSaving(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- saving\n");
    QString wavPath = w.recorder.start();
    key(w, Qt::Key_D, true); wait(700); key(w, Qt::Key_D, false); wait(200);
    w.recorder.stop();
    QFile wf(wavPath);
    bool wavOk = wf.open(QIODevice::ReadOnly);
    QByteArray wd = wavOk ? wf.readAll() : QByteArray();
    int nz = 0;
    for (int i = 44; i + 1 < wd.size(); i += 2) if (wd[i] != 0 || wd[i + 1] != 0) ++nz;
    check(wavOk && wd.size() > 44 + 48000 * 2 && wd.startsWith("RIFF") && nz > 1000, QString("WAV export written with audio in it (%1 kB)").arg(wd.size() / 1024));
    QString sp = store::baseDir() + "/sessions/selftest.json";
    SessionData a; inst.snapshot(a);
    check(w.saveSession(sp), "session saved");
    check(a.layers.size() == 1 && !a.layers[0].ev.empty() && a.loopBeats >= 4, "the session holds the recorded loop");
    key(w, Qt::Key_Control, true);
    key(w, Qt::Key_X, true); key(w, Qt::Key_X, false);   // Ctrl+X clear backing
    key(w, Qt::Key_D, true); key(w, Qt::Key_D, false);   // Ctrl+D clear loop
    key(w, Qt::Key_Control, false);
    inst.set(S_ROOT, 3); wait(100);
    check(inst.uiState().progLen == 0 && inst.uiState().layers == 0, "backing and loop cleared");
    check(w.loadSession(sp), "session loaded");
    wait(100);
    SessionData b; inst.snapshot(b);
    bool same = b.settings.tuning.root == a.settings.tuning.root && b.progLen == a.progLen && b.layers.size() == a.layers.size() &&
                b.loopBeats == a.loopBeats && QString(b.lead.name) == QString(a.lead.name) && QString(b.backing.name) == QString(a.backing.name);
    if (same && !a.layers.empty()) same = a.layers[0].ev.size() == b.layers[0].ev.size();
    check(same, "loaded session matches what was saved (key, sounds, progression, loop)");
    inst.set(S_VOLUME, 0.06f);
    w.saveState();
    check(QFile::exists(store::baseDir() + "/state.json") && QFile::exists(store::baseDir() + "/keymap.json"), "state and keymap files written");
}
} // namespace selftest
