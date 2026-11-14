// venv-vrequency self-test — last of all: panic silences everything, and nothing was dropped or late.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "audio/audio.h"
#include "core/instrument/instrument.h"
#include <cstdio>

using namespace td;

namespace selftest {
void testPanicAndHealth(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; AudioOut &audio = ctx.audio;
    std::printf("- panic / health\n");
    key(w, Qt::Key_Q, true); key(w, Qt::Key_A, true); wait(60);
    key(w, Qt::Key_Backspace, true); key(w, Qt::Key_Backspace, false); wait(200);
    check(inst.uiState().voices == 0, "Backspace silences everything");
    key(w, Qt::Key_Q, false); key(w, Qt::Key_A, false);
    check(inst.uiState().droppedEvents == 0, "no input events were dropped");
    std::printf("  info  engine load %.1f%% avg, %.1f%% peak, %u late blocks, backend %s\n", audio.load() * 100, audio.peakLoad() * 100, audio.overruns(), audio.backend().c_str());
    check(audio.peakLoad() < 1.0f, "audio never fell behind real time");
}
} // namespace selftest
