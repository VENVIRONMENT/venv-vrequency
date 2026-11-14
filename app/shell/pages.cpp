// venv-vrequency — the pages by name (see shell.h).
#include "app/shell/shell.h"

namespace {
const char *kNames[PG_COUNT] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
const char *kTitles[PG_COUNT] = {"Track", "Synth", "Sound", "Beat", "Backing", "Tune", "System", "Help", "Chat"};
} // namespace

const char *pageName(int p) { return p >= 0 && p < PG_COUNT ? kNames[p] : "synth"; }
const char *pageTitle(int p) { return p >= 0 && p < PG_COUNT ? kTitles[p] : "Synth"; }

int pageFromName(const QString &n0) {
    const QString n = n0.toLower() == "play" ? QString("synth") : n0.toLower();
    for (int i = 0; i < PG_COUNT; ++i) if (n == kNames[i]) return i;
    return -1;
}
