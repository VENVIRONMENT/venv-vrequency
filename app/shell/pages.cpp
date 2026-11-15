// venv-vrequency — the pages by name (see shell.h).
#include "app/shell/shell.h"

namespace {
const char *kNames[PG_COUNT] = {"synth", "sound", "pulse", "tune", "system", "help", "chat"};
const char *kTitles[PG_COUNT] = {"Synth", "Sound", "Pulse", "Tune", "System", "Help", "Chat"};
} // namespace

const char *pageName(int p) { return p >= 0 && p < PG_COUNT ? kNames[p] : "synth"; }
const char *pageTitle(int p) { return p >= 0 && p < PG_COUNT ? kTitles[p] : "Synth"; }

int pageFromName(const QString &n0) {
    QString n = n0.toLower();
    if (n == "play") n = "synth";
    if (n == "beat") n = "pulse"; // the page was called Beat before 1.0
    for (int i = 0; i < PG_COUNT; ++i) if (n == kNames[i]) return i;
    return -1;
}
