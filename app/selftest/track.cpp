// venv-vrequency self-test — the Track page, part by part (track_*.cpp), on one track built as it goes.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include <cstdio>

using namespace td;

namespace selftest {
void testTrack(Context &ctx) {
    std::printf("- track\n");
    TrackState tr;
    trackKeepAndPlace(ctx, tr);
    trackChopByTouch(ctx, tr);
    trackChooseCopyPaste(ctx);
    trackChopByKeys(ctx, tr);
    trackCycle(ctx);
    trackPlay(ctx);
    trackExport(ctx);
    trackRemove(ctx, tr);
    trackPickLoopSound(ctx, tr);
    trackPickBeatKit(ctx, tr);
    trackMixer(ctx);
    trackSaveAndLoad(ctx, tr);
    ctx.inst.set(S_VOLUME, 0.06f);
    ctx.w.setPage(PG_PLAY); wait(60);
}
} // namespace selftest
