// venv-vrequency — the tour's static facts: the page list, the Start-here walk, picture variants (see tour.h).
#include "app/tour/tour.h"
#include "app/tour/tourpriv.h"
#include "app/shell/mainwindow.h"
#include <QtGlobal>

namespace {
const TourView::PageInfo kPages[] = {
    {"synth", "Synth", "Synth", PG_PLAY},
    {"sound", "Sound", "Sound", PG_SOUND},
    {"sound-deep", "Sound: All parameters", "Edit", PG_SOUND},
    {"beat", "Beat", "Beat", PG_BEAT},
    {"backing", "Backing", "Bkng", PG_BACK},
    {"tune", "Tune", "Tune", PG_TUNE},
    {"system", "System", "Sys", PG_SYS},
    {"key-test", "System: Key test", "Keys", PG_SYS},
    {"help", "Help", "Help", PG_HELP},
    {"chat", "Chat", "Chat", PG_CHAT},
};
const int kPageCount = (int)(sizeof(kPages) / sizeof(kPages[0]));
// "Start here": the pages in the order a new player needs them
const char *kPath[] = {"synth", "sound", "tune", "backing", "beat", "system", "help", "chat"};
const int kPathLen = (int)(sizeof(kPath) / sizeof(kPath[0]));
} // namespace

int tourPathLen() { return kPathLen; }
const char *tourPathAt(int i) { return kPath[qBound(0, i, kPathLen - 1)]; }

// concepts worth reading beside each page (the READ ALSO chips of a page's intro card)
QStringList tourReadAlso(const QString &page) {
    if (page == "synth") return {"concepts.strings", "concepts.slides", "concepts.scale-lock", "concepts.loop-pedal"};
    if (page == "sound" || page == "sound-deep") return {"concepts.adsr", "concepts.undo-map", "concepts.sessions-favourites-state"};
    if (page == "beat") return {"concepts.kept-beats", "concepts.bar-lines", "concepts.glitch"};
    if (page == "backing") return {"concepts.backing-modes", "concepts.scale-lock", "concepts.tempo"};
    if (page == "tune") return {"concepts.scale-lock", "concepts.tempo", "concepts.slides"};
    if (page == "system" || page == "key-test") return {"concepts.q20-layers", "concepts.panic", "concepts.sessions-favourites-state"};
    if (page == "help") return {"concepts.command-layer", "concepts.q20-layers", "concepts.recall"};
    if (page == "chat") return {"concepts.copilot", "concepts.ticker", "concepts.recall"};
    return {"concepts.recall"};
}

// ---------------------------------------------------------------------------------- static facts

int TourView::pageCount() { return kPageCount; }
const TourView::PageInfo &TourView::pageInfo(int i) { return kPages[qBound(0, i, kPageCount - 1)]; }

int TourView::pageIndex(const QString &n0) {
    QString n = n0.trimmed().toLower();
    if (n == "play") n = "synth";
    else if (n == "back") n = "backing";
    else if (n == "sys") n = "system";
    else if (n == "params") n = "sound-deep";
    for (int i = 0; i < kPageCount; ++i) if (n == kPages[i].id) return i;
    return -1;
}

// One page needs more than one picture: the All-parameters editor (twelve parameters a page). Its
// other three pictures come after the pages' own.
int TourView::variantCount(int page) { return page == pageIndex("sound-deep") ? 4 : 1; }
int TourView::variantBase(int page) { return page == pageIndex("sound-deep") ? kPageCount : -1; }
int TourView::picIndex() const { return variant <= 0 || variantBase(pageIdx) < 0 ? pageIdx : variantBase(pageIdx) + variant - 1; }
int TourView::picCount() { return kPageCount + 3; }
int TourView::picPage(int pic) { return pic < kPageCount ? pic : pageIndex("sound-deep"); }

void TourView::setVariant(int v) {
    const int n = variantCount(pageIdx);
    variant = n > 1 ? ((v % n) + n) % n : 0;
    selIdx = -1;
    scrollA = scrollB = 0;
}
