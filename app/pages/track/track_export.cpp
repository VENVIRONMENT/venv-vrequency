// venv-vrequency — the Track page: rendering the track to a WAV file on a worker thread.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include "app/store/store.h"
#include "app/store/wav_writer.h"
#include "audio/audio.h"
#include "core/dsp.h"
#include <QFileInfo>
#include <memory>

using namespace td;
using namespace pal;

void TrackPage::trackExport() {
    if (exportState.load() == 1) { toast("Still exporting", 1200); return; }
    bool any = false;
    for (int ln = 0; ln < kLanes; ++ln) any = any || !track.lane[ln].empty();
    if (!any) { toast("Nothing on the track to export", 1600); return; }
    if (exportThread.joinable()) exportThread.join();
    store::ensureDirs();
    exportFile = store::baseDir() + "/recordings/" + store::timestampName("track", "wav");
    SessionData sd;
    inst.snapshot(sd);
    sd.settings.volume = 1.0f; sd.settings.metro = false;
    std::shared_ptr<const TrackData> t = std::make_shared<const TrackData>(track);
    const int rate = sh.audio.sampleRate();
    const QString path = exportFile;
    exportState = 1; exportPct = 0;
    exportThread = std::thread([this, sd, t, rate, path] {
        enableFlushToZero();
        auto off = std::make_unique<Instrument>((float)rate);
        off->restore(sd);
        off->setTrack(t);
        off->trackDo(TC_ONCE, 0);
        WavWriter w;
        if (!w.open(path, rate)) { exportState = 3; return; }
        const int N = 1024;
        const double total = t->bars * 4.0 * 60.0 / sd.settings.bpm * rate; // frames of music
        const qint64 limit = (qint64)total + 8 * (qint64)rate;
        std::vector<float> l(N), r(N), il(2 * N);
        qint64 done = 0, endedAt = -1;
        int quiet = 0;
        while (done < limit) {
            off->process(l.data(), r.data(), N);
            float pk = 0;
            for (int i = 0; i < N; ++i) {
                il[2 * i] = l[i]; il[2 * i + 1] = r[i];
                pk = std::max(pk, std::max(std::fabs(l[i]), std::fabs(r[i])));
            }
            w.write(il.data(), N);
            done += N;
            if (endedAt < 0 && !off->uiState().trackPlaying) endedAt = done;
            if (endedAt >= 0) {
                quiet = pk < 2e-4f ? quiet + 1 : 0;
                if (quiet > 24 || done - endedAt > 6 * (qint64)rate) break;
            }
            exportPct = (int)std::min(99.0, done * 100.0 / total);
        }
        w.close();
        exportSecs = (int)(done / rate);
        exportState = 2;
    });
    toast("Exporting the track…", 1400);
}

void TrackPage::pollExport() {
    const int es = exportState.load();
    if (es < 2) return;
    if (exportThread.joinable()) exportThread.join();
    exportState = 0;
    if (es == 2) {
        sh.setLastRecording(exportFile);
        toast(QString("Saved %1 (%2 s): Play last plays it").arg(QFileInfo(exportFile).fileName()).arg(exportSecs.load()), 3200);
    } else toast("Could not write the track's audio file", 2600);
}
