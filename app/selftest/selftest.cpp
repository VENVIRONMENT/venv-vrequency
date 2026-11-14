// venv-vrequency — the built-in self-test: every group of checks, one after another (see areas.h).
#include "app/selftest/selftest.h"
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <cstdio>

using namespace td;
using namespace selftest;

int runSelfTest(MainWindow &w, Instrument &inst, AudioOut &audio, const QString &shotsDir) {
    std::printf("venv-vrequency self-test\n");
    inst.set(S_VOLUME, 0.06f); // keep it quiet: this plays through the real output
    wait(400);
    Context ctx{w, inst, audio, shotsDir};
    testAudio(ctx);
    testLayout(ctx);
    testKeys(ctx);
    testSounds(ctx);
    testBacking(ctx);
    testLooper(ctx);
    testTouch(ctx);
    testSaving(ctx);
    testBeat(ctx);
    testKeptBeats(ctx);
    testTrack(ctx);
    testListeningBack(ctx);
    testControlSocket(ctx);
    testGuidedHelp(ctx);
    testBugFixes(ctx);
    runTourTests(w, inst, audio, check);
    testPanicAndHealth(ctx);

    std::printf("\n%d passed, %d failed\n", passed(), failed());
    return failed() ? 1 : 0;
}
