// venv-vrequency self-test — listening back to the newest recording.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include <cstdio>

namespace selftest {
void testListeningBack(Context &ctx) {
    MainWindow &w = ctx.w;
    std::printf("- listening back\n");
    check(w.recorder.togglePlayback() && w.recorder.playingBack(), "Play last starts playing the newest recording");
    wait(300);
    check(w.recorder.togglePlayback(), "and the same button stops it");
    wait(300);
    check(!w.recorder.playingBack(), "playback has stopped");
}
} // namespace selftest
