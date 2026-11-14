// venv-vrequency — the System page (see system_page.h): the controls.
#include "app/pages/system/system_page.h"
#include "app/shell/recorder.h"
#include "app/sounds/sounds.h"
#include "app/store/store.h"
#include "app/ui/palette.h"
#include <QFileInfo>
#include <cmath>

using namespace td;
using namespace pal;

void SystemPage::paint() {
    ui.slider(QRectF(16, 66, 340, 58), "volume", "Master volume", u.set.volume, QString("%1%").arg((int)(u.set.volume * 100)), [this](float n) { sh.set(S_VOLUME, n); });
    ui.slider(QRectF(364, 66, 340, 58), "loopvol", "Loop volume", u.set.loopVol, QString("%1%").arg((int)(u.set.loopVol * 100)), [this](float n) { sh.set(S_LOOPVOL, n); });

    ui.section(130, "Touch pad mapping");
    qreal w6 = (688 - XY_COUNT * 6) / (XY_COUNT + 1.0);
    auto c6 = [w6](int i) { return QRectF(std::floor(16 + i * (w6 + 6)), 154, std::floor(w6), 54); };
    for (int i = 0; i < XY_COUNT; ++i)
        ui.button(c6(i), QString("xymap%1").arg(i), nice(xyMapName(i)), u.set.xyMap == i, [this, i] { sh.set(S_XYMAP, (float)i); },
               QString("%1/%2").arg(xyAxisName(i, false)).arg(xyAxisName(i, true)).toLower().replace("resonance", "reso").replace("vibrato", "vib"));
    ui.button(c6(XY_COUNT), "xyhold2", "Hold", u.set.xyHold, [this] { sh.set(S_XYHOLD, u.set.xyHold ? 0 : 1); }, "keep it on");

    qreal w5 = (688 - 4 * 6) / 5.0;
    auto c5 = [w5](int i) { return QRectF(std::floor(16 + i * (w5 + 6)), 218, std::floor(w5), 58); };
    const Recorder &rec = sh.recorderNow();
    ui.button(c5(0), "chordrow", "Chord row", sh.keys.chordRow, [this] { sh.perform(A_CHORD_ROW); }, "low row = chords");
    ui.button(c5(1), "recwav", rec.recording() ? QString::fromUtf8("■ Stop rec") : QString::fromUtf8("● Record"), rec.recording(), [this] { sh.perform(A_RECORD); },
           rec.recording() ? QString("%1 s").arg(rec.seconds()) : "what you hear");
    ui.button(c5(2), "playlast", rec.playingBack() ? QString::fromUtf8("■ Stop") : QString::fromUtf8("▶ Play last"), rec.playingBack(), [this] { sh.perform(A_PLAY_LAST); },
           rec.playingBack() ? "playing it back" : "listen back");
    ui.button(c5(3), "savesession", "Save session", false, [this] {
        QString path = store::baseDir() + "/sessions/" + store::timestampName("session", "json");
        sh.toast(sh.saveSession(path) ? QString("Saved %1").arg(QFileInfo(path).fileName()) : QString("Could not save session"), 2200);
    }, "sounds and loops");
    ui.button(c5(4), "keytest", "Key test", sh.keys.testOpen, [this] { sh.keys.testOpen = !sh.keys.testOpen; sh.keys.maxHeld = 0; }, "check the keys");

    paintSessions();
    paintStatus();

    qreal w4 = (688 - 3 * 8) / 4.0;
    auto c4 = [w4](int i, qreal y, qreal h) { return QRectF(std::floor(16 + i * (w4 + 8)), y, std::floor(w4), h); };
    ui.button(c4(0, 642, 60), "panic", "Panic", false, [this] { sh.perform(A_PANIC); }, "all notes off");
    ui.stepper(c4(1, 642, 60), "latency", "Loop fix", QString("%1 ms").arg((int)u.set.latencyMs),
            [this] { sh.set(S_LATENCY, u.set.latencyMs - 10); }, [this] { sh.set(S_LATENCY, u.set.latencyMs + 10); });
    ui.button(c4(2, 642, 60), "factory", "Factory sounds", false, [this] {
        Sounds &sounds = sh.soundShelf();
        sounds.factoryReset();
        int back = sounds.addExtraSounds();
        sh.refreshLists();
        sh.toast(back ? QString("Slots 1-0 restored, %1 favourite%2 put back").arg(back).arg(back == 1 ? "" : "s") : QString("Sound slots 1-0 restored"));
    }, "restore slots 1-0");
    ui.button(c4(3, 642, 60), "quit", "Quit", false, [this] { sh.perform(A_QUIT); }, "saves your setup");
}
