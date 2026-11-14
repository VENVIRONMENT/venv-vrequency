// venv-vrequency self-test — Track: playing it, exporting it as a WAV file, removing clips.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/ui/zones.h"
#include <QElapsedTimer>
#include <QFile>
#include <cstdio>

using namespace td;

namespace selftest {
// playing the track, and playing over it from the Synth page
void trackPlay(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    press(w, Qt::Key_Return);
    uint32_t h0 = inst.uiState().drumHits[D_HAT];
    press(w, Qt::Key_Space); wait(200);
    UiState tu = inst.uiState();
    check(tu.trackPlaying && !tu.sustain, "Space plays the track");
    bool voiced = false, hat = false;
    for (int i = 0; i < 50 && !(voiced && hat); ++i) { wait(50); tu = inst.uiState(); voiced = voiced || tu.voices > 0; hat = hat || tu.drumHits[D_HAT] > h0; }
    check(voiced && hat, "the loop clip's notes and the beat clip's hats sound");
    const double p0 = inst.uiState().trackPos; wait(200);
    check(inst.uiState().trackPos > p0, "the play line moves");
    key(w, Qt::Key_Q, true); wait(80);
    check(!inst.uiState().str[2].gate, "on the Track page letters are commands: Q plays no string");
    key(w, Qt::Key_Q, false);
    w.setPage(PG_PLAY); wait(60);
    key(w, Qt::Key_Q, true); wait(80);
    check(inst.uiState().str[2].gate && inst.uiState().trackPlaying, "on the Synth page you play over the track");
    key(w, Qt::Key_Q, false);
    ctrl(w, Qt::Key_Space); wait(120);
    check(!inst.uiState().trackPlaying, "Ctrl+Space stops the track from any page");
    w.setPage(PG_TRACK); wait(60);
}

// Export writes the track as a WAV file in the background
void trackExport(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; AudioOut &audio = ctx.audio;
    const TrackData &T = w.trackPage.trackData();
    tap(w, zn::track_export);
    check(w.trackPage.exportStatus() == 1, "Export starts writing in the background");
    QElapsedTimer et; et.start();
    while (w.trackPage.exportStatus() != 0 && et.elapsed() < 60000) { wait(100); w.shot(); }
    QFile ef(w.recorder.lastPath());
    const bool eok = ef.open(QIODevice::ReadOnly);
    const QByteArray ed = eok ? ef.readAll() : QByteArray();
    int enz = 0;
    for (int i = 44; i + 1 < ed.size(); i += 2) if (ed[i] != 0 || ed[i + 1] != 0) ++enz;
    const double esec = (ed.size() - 44) / 4.0 / audio.sampleRate(), want = T.bars * 4 * 60.0 / inst.uiState().set.bpm;
    std::printf("  info  export: %d bars, %.1f s of audio, written in %.1f s\n", T.bars, esec, et.elapsed() / 1000.0);
    check(eok && w.recorder.lastPath().contains("/track-") && ed.startsWith("RIFF") && enz > 1000, "the track is exported as a WAV file with audio in it");
    check(esec >= want && esec < want + 9, QString("it lasts the track plus the dying echoes (%1 s for a %2 s track)").arg(esec, 0, 'f', 1).arg(want, 0, 'f', 1));
}

// Backspace deletes a placed clip, Remove a shelf clip and every copy of it
void trackRemove(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    touch(w, tr.lanes.cell(LN_BEAT, 0));
    press(w, Qt::Key_Backspace); wait(60);
    check(T.lane[LN_BEAT].size() == 1, "Backspace deletes the chosen clip");
    press(w, Qt::Key_1);
    tap(w, zn::track_remove); wait(40);
    check(T.clips.size() == 3, "Remove on a shelf clip asks twice");
    tap(w, zn::track_remove); wait(40);
    check(T.clips.size() == 2 && T.lane[LN_BEAT].empty() && T.lane[LN_CHORDS][0].clip == 0 && T.lane[LN_LOOPA][0].clip == 1,
          "the second tap deletes the clip and every copy of it; the others keep pointing at the right clips");
    ctrl(w, Qt::Key_Z);
    check(T.clips.size() == 3 && T.lane[LN_BEAT].size() == 1, "and that can be undone too");
}
} // namespace selftest
