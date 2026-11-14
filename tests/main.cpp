// venv-vrequency — engine tests. Renders the instrument offline and measures what
// actually comes out (frequencies, envelopes, voices), rather than trusting
// internal state alone.
//
//   vrequency_tests          run all checks
//   vrequency_tests --bench  CPU benchmark at maximum polyphony
#include "tests/support.h"
#include <cstdio>
#include <cstring>

int g_fail = 0, g_pass = 0;

// The areas, in the order they run; each *_tests.cpp file holds the ones for its area.
void bench();
void testScales();
void testFrequenciesAndGlide();
void testPolyphony();
void testPresetsAndRandomiser();
void testExtraSounds();
void testDrums();
void testBeat();
void testTrackClips();
void testTrackPlayback();
void testTrackChordsAndTransport();
void testTrackPicks();
void testTrackMixer();
void testTrackPickJoin();
void testSoundShelf();
void testLimiter();
void testXY();
void testGlitch();
void testBacking();
void testLooper();
void testSessionRoundTrip();
void testPatchSwitchClicks();
void testAnalysis();

int main(int argc, char **argv) {
    if (argc > 1 && std::strcmp(argv[1], "--bench") == 0) { bench(); return 0; }
    testScales();
    testFrequenciesAndGlide();
    testPolyphony();
    testPresetsAndRandomiser();
    testExtraSounds();
    testDrums();
    testBeat();
    testTrackClips();
    testTrackPlayback();
    testTrackChordsAndTransport();
    testTrackPicks();
    testTrackMixer();
    testTrackPickJoin();
    testSoundShelf();
    testLimiter();
    testXY();
    testGlitch();
    testBacking();
    testLooper();
    testSessionRoundTrip();
    testPatchSwitchClicks();
    testAnalysis();
    std::printf("\n%d checks passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
