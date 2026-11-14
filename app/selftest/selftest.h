// venv-vrequency — the built-in self-test and the documentation pictures: what the rest of the app calls.
// The self-test drives the real window with real Qt key and pointer events while the real audio thread
// runs, then checks what the engine did. (It cannot press the physical keys or listen to the speaker:
// those two checks need a human.)
#pragma once
#include <functional>

class AudioOut;
class MainWindow;
class QString;
namespace td { class Instrument; }

// `vrequency --selftest [--shots DIR]`: every check, in order. Returns 0 when all of them passed.
int runSelfTest(MainWindow &w, td::Instrument &inst, AudioOut &audio, const QString &shotsDir);
// The tour's part of the self-test: it reports each result through `check`.
void runTourTests(MainWindow &w, td::Instrument &inst, AudioOut &audio, const std::function<void(bool, const QString &)> &check);
// `vrequency --audio null --render DIR`: pictures of every page, for the documentation.
int renderShots(MainWindow &w, td::Instrument &inst, const QString &dir);
