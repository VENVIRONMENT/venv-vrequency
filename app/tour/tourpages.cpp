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
    {"pulse", "Pulse", "Pulse", PG_PULSE},
    {"freq", "Freq", "Freq", PG_FREQ},
    {"backing", "Backing", "Bkng", PG_BACK},
    {"tune", "Tune", "Tune", PG_TUNE},
    {"system", "System", "Sys", PG_SYS},
    {"key-test", "System: Key test", "Keys", PG_SYS},
    {"help", "Help", "Help", PG_HELP},
    {"chat", "Chat", "Chat", PG_CHAT},
};
const int kPageCount = (int)(sizeof(kPages) / sizeof(kPages[0]));
// "Start here": the pages in the order a new player needs them
const char *kPath[] = {"synth", "sound", "tune", "backing", "pulse", "freq", "system", "help", "chat"};
const int kPathLen = (int)(sizeof(kPath) / sizeof(kPath[0]));
} // namespace

int tourPathLen() { return kPathLen; }
const char *tourPathAt(int i) { return kPath[qBound(0, i, kPathLen - 1)]; }

// concepts worth reading beside each page (the READ ALSO chips of a page's intro card)
QStringList tourReadAlso(const QString &page) {
    if (page == "synth") return {"concepts.strings", "concepts.slides", "concepts.scale-lock", "concepts.loop-pedal"};
    if (page == "sound" || page == "sound-deep") return {"concepts.adsr", "concepts.warp", "concepts.undo-map", "concepts.sessions-favourites-state"};
    if (page == "pulse") return {"concepts.pulse-kinds", "concepts.warp", "concepts.latch-sync", "concepts.loop-pedal"};
    if (page == "freq") return {"concepts.beating", "concepts.warp", "concepts.latch-sync", "concepts.scale-lock"};
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

// Two pages need more than one picture: the All-parameters editor (one a page of parameters, the last
// the Warp view) and Pulse (its sound sliders, then with WARP on). Their other pictures come after
// the pages' own, in this order.
namespace {
struct Variants { const char *page; int count; };
const Variants kVariants[] = {{"sound-deep", kDeepPages}, {"pulse", 2}};
} // namespace
int TourView::variantCount(int page) {
    for (const Variants &v : kVariants) if (page == pageIndex(v.page)) return v.count;
    return 1;
}
int TourView::variantBase(int page) {
    int base = kPageCount;
    for (const Variants &v : kVariants) { if (page == pageIndex(v.page)) return base; base += v.count - 1; }
    return -1;
}
int TourView::picIndex() const { return variant <= 0 || variantBase(pageIdx) < 0 ? pageIdx : variantBase(pageIdx) + variant - 1; }
int TourView::picCount() {
    int n = kPageCount;
    for (const Variants &v : kVariants) n += v.count - 1;
    return n;
}
int TourView::picPage(int pic) {
    if (pic < kPageCount) return pic;
    int base = kPageCount;
    for (const Variants &v : kVariants) { if (pic < base + v.count - 1) return pageIndex(v.page); base += v.count - 1; }
    return 0;
}

void TourView::setVariant(int v) {
    const int n = variantCount(pageIdx);
    variant = n > 1 ? ((v % n) + n) % n : 0;
    selIdx = -1;
    scrollA = scrollB = 0;
}
