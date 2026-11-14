// venv-vrequency — the interactive tour (see tour.h). Layout, top to bottom:
//   header        TOUR tag, the page name, Concepts, Leave
//   chip row      one chip per page (or the guided-path bar)
//   upper left    the picture of the page, 360 x 360, every control a hotspot
//   upper right   the card: kind, title, summary, what it does, RECALL
//   lower left    USE (touch / keys) and the key caps
//   lower right   tips, related controls, and the ASK ROOK button
#include "app/tour/tour.h"
#include "app/shell/mainwindow.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace pal;


namespace {
const TourView::PageInfo kPages[] = {
    {"track", "Track", "Track", PG_TRACK},
    {"track-chop", "Track: Chop", "Chop", PG_TRACK},
    {"track-sound", "Track: Sound picker", "Clip", PG_TRACK},
    {"track-mixer", "Track: Mixer", "Mix", PG_TRACK},
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
const char *kPath[] = {"synth", "sound", "tune", "backing", "beat", "track", "track-mixer", "system", "help", "chat"};
const int kPathLen = (int)(sizeof(kPath) / sizeof(kPath[0]));

// concepts worth reading beside each page (the READ ALSO chips of a page's intro card)
QStringList readAlso(const QString &page) {
    if (page == "synth") return {"concepts.strings", "concepts.slides", "concepts.scale-lock", "concepts.loop-pedal"};
    if (page == "sound" || page == "sound-deep") return {"concepts.adsr", "concepts.undo-map", "concepts.sessions-favourites-state"};
    if (page == "beat") return {"concepts.kept-beats", "concepts.bar-lines", "concepts.glitch"};
    if (page == "backing") return {"concepts.backing-modes", "concepts.scale-lock", "concepts.tempo"};
    if (page == "tune") return {"concepts.scale-lock", "concepts.tempo", "concepts.slides"};
    if (page.startsWith("track")) return {"concepts.track-flow", "concepts.bar-lines", "concepts.undo-map"};
    if (page == "system" || page == "key-test") return {"concepts.q20-layers", "concepts.panic", "concepts.sessions-favourites-state"};
    if (page == "help") return {"concepts.command-layer", "concepts.q20-layers", "concepts.recall"};
    if (page == "chat") return {"concepts.copilot", "concepts.ticker", "concepts.recall"};
    return {"concepts.recall"};
}

inline QRectF grow(const QRectF &r, qreal d) { return r.adjusted(-d, -d, d, d); }
const QRectF kRegionA(396, 62, 316, 380), kRegionB1(8, 452, 420, 222), kRegionB2(436, 452, 276, 206);
const qreal kPicSize = 380, kScale = kPicSize / 720.0;
const QRectF kAskRect(446, 668, 256, 40);
} // namespace

// ---------------------------------------------------------------------------------- static facts

int TourView::pageCount() { return kPageCount; }
const TourView::PageInfo &TourView::pageInfo(int i) { return kPages[qBound(0, i, kPageCount - 1)]; }

int TourView::pageIndex(const QString &n0) {
    QString n = n0.trimmed().toLower();
    if (n == "play") n = "synth";
    else if (n == "back") n = "backing";
    else if (n == "sys") n = "system";
    else if (n == "params") n = "sound-deep";
    else if (n == "chop") n = "track-chop";
    else if (n == "picker") n = "track-sound";
    else if (n == "mixer") n = "track-mixer";
    for (int i = 0; i < kPageCount; ++i) if (n == kPages[i].id) return i;
    return -1;
}

// A few pages need more than one picture: the All-parameters editor (twelve parameters a page) and the
// sound picker (the Drums kinds only show for a beat clip). They are pictures 14.. after the 14 pages.
int TourView::variantCount(int page) { return page == 6 ? 4 : (page == 2 ? 2 : 1); }
int TourView::variantBase(int page) { return page == 6 ? 14 : (page == 2 ? 17 : -1); }
int TourView::picIndex() const { return variant <= 0 || variantBase(pageIdx) < 0 ? pageIdx : variantBase(pageIdx) + variant - 1; }
int TourView::picCount() { return 18; }
int TourView::picPage(int pic) { return pic < kPageCount ? pic : (pic <= 16 ? 6 : 2); }

void TourView::setVariant(int v) {
    const int n = variantCount(pageIdx);
    variant = n > 1 ? ((v % n) + n) % n : 0;
    selIdx = -1;
    scrollA = scrollB = 0;
}

// ---------------------------------------------------------------------------------- lifecycle

TourView::TourView(MainWindow &win) : w(win) { pageIdx = 4; }
TourView::~TourView() = default;

const CardDb &TourView::cards() const { return useOverride ? override_ : helpCards(); }
void TourView::setCards(const CardDb &db) { override_ = db; useOverride = true; }
void TourView::resetCards() { useOverride = false; override_ = CardDb(); }

void TourView::ensureBuilt() {
    if (built) return;
    tourBuildPictures(pics, &w.sink);
    built = true;
}

const TourView::Pic &TourView::picture(int page) {
    ensureBuilt();
    return pics[qBound(0, page, picCount() - 1)];
}

bool TourView::seen() const { return w.tourSeenFlag; }
void TourView::forgetSeen() { w.tourSeenFlag = false; promptDone = false; promptOn = false; }

void TourView::markSeen() {
    promptOn = false; promptDone = true;
    if (!w.tourSeenFlag) { w.tourSeenFlag = true; }
    w.saveState();
}

bool TourView::open(const QString &page, const QString &element, bool guide, QString *err, bool defer) {
    int idx = pageIdx;
    if (!page.isEmpty()) {
        idx = pageIndex(page);
        if (idx < 0) { if (err) *err = "unknown tour page '" + page.left(40) + "'"; return false; }
    } else if (!opened) {
        const int last = pageIndex(w.tourLastPage);
        idx = last >= 0 ? last : pageIndex("synth");
    }
    if (!opened) w.guide.dockClose();
    opened = true;
    promptOn = false; promptDone = true;
    if (!built && defer) { // drawing the pictures takes a moment: say so, then do it
        pendingPage = idx; pendingElement = element; pendingGuided = guide; building = true;
        QTimer::singleShot(30, [this] { if (opened && building) { ensureBuilt(); building = false; finishOpen(pendingPage, pendingElement, pendingGuided); } });
        w.update();
        return true;
    }
    ensureBuilt();
    building = false;
    finishOpen(idx, element, guide);
    return true;
}

void TourView::finishOpen(int idx, const QString &element, bool guide) {
    conceptOn = false; conceptId.clear();
    guided = guide;
    if (guided) { step = 0; idx = pageIndex(kPath[0]); }
    setPage(idx);
    if (!element.isEmpty() && !select(element)) {
        QString id = element.startsWith("concepts.") ? element.mid(9) : element;
        if (cards().conceptCard(id)) selectConcept(id);
    }
    markSeen();
    w.update();
}

void TourView::close() {
    if (!opened) return;
    opened = false; building = false; conceptOn = false; conceptId.clear(); guided = false;
    w.tourLastPage = pageId();
    w.saveState();
    w.update();
}

void TourView::setPage(int i, bool keep) {
    pageIdx = qBound(0, i, kPageCount - 1);
    variant = 0;
    if (!keep) selIdx = -1;
    scrollA = scrollB = 0;
    w.tourLastPage = pageId();
    conceptOn = false;
}

// ---------------------------------------------------------------------------------- selection

QString TourView::selectedName() const {
    if (!built || selIdx < 0 || selIdx >= (int)pics[picIndex()].hot.size()) return QString();
    return pics[picIndex()].hot[selIdx].name;
}

QString TourView::selectedRef() const {
    if (!built || selIdx < 0 || selIdx >= (int)pics[picIndex()].hot.size()) return QString();
    const HelpCard *c = cardFor(pics[picIndex()].hot[selIdx]);
    QString pg, key;
    const Hot &h = pics[picIndex()].hot[selIdx];
    if (!h.ref.isEmpty()) return h.ref;
    return pageId() + "." + (c ? c->key : h.name);
}

const HelpCard *TourView::cardFor(const Hot &h) const {
    const CardDb &db = cards();
    if (!db.ok) {
        missing = HelpCard();
        missing.key = "error"; missing.kind = "area"; missing.title = "Help unavailable";
        missing.summary = "The help text could not be read.";
        missing.does = db.error.isEmpty() ? QString("The help file is missing or damaged.") : db.error.left(200);
        missing.recall = "The pictures still work. Reinstall or rebuild the program to get the help text back.";
        missing.ask = "The tour says its help text is missing. What can I do?";
        missing.use.push_back({"Both", "Everything else in the app works as before."});
        return &missing;
    }
    const HelpCard *c = h.ref.isEmpty() ? db.find(pageId(), h.name) : db.resolve(h.ref);
    if (c) return c;
    missing = HelpCard();
    missing.key = h.name; missing.kind = "area"; missing.title = h.name;
    missing.summary = "No help card for this control yet.";
    missing.does = "This control is on the screen but has no card. Ask Rook what it does.";
    missing.recall = "Look for it on the " + QString::fromLatin1(pageInfo(pageIdx).label) + " page.";
    missing.ask = QString("What does %1 do on the %2 page?").arg(h.name, pageInfo(pageIdx).label);
    missing.use.push_back({"Both", "Ask Rook for details."});
    return &missing;
}

// a touch on a hotspot: select it; the editor's Prev / Next also turn the picture to the other parameters
void TourView::tapHot(int idx) {
    choose(idx);
    const QString n = selectedName();
    if (pageIdx == 6 && (n == "deepnext" || n == "deepprev")) {
        setVariant(variant + (n == "deepnext" ? 1 : -1));
        const auto &hot = pics[picIndex()].hot;
        for (int i = 0; i < (int)hot.size(); ++i) if (hot[i].name == n) choose(i);
    }
}

void TourView::choose(int idx) {
    if (!built) return;
    selIdx = (idx >= 0 && idx < (int)pics[picIndex()].hot.size()) ? idx : -1;
    scrollA = scrollB = 0;
    w.update();
}

bool TourView::select(const QString &name) {
    if (!built || name.isEmpty()) return false;
    const CardDb &db = cards();
    const HelpCard *c = db.ok ? db.find(pageId(), name) : nullptr;
    for (int k = 0; k < variantCount(pageIdx); ++k) {
        const int v = (variant + k) % variantCount(pageIdx);
        const auto &hot = pics[variantPic(pageIdx, v)].hot;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < (int)hot.size(); ++i) {
                const bool hit = pass == 0 ? hot[i].name == name : (c && cardFor(hot[i]) == c);
                if (hit) { conceptOn = false; if (v != variant) setVariant(v); choose(i); return true; }
            }
    }
    return false;
}

void TourView::selectConcept(const QString &id) {
    conceptOn = true;
    conceptId = id;
    scrollA = scrollB = 0;
    const auto &cs = cards().concepts;
    for (int i = 0; i < (int)cs.size(); ++i) if (cs[i].key == id) conceptScroll = qMax(0, i - 6);
    w.update();
}

int TourView::nearestHot(QPointF p, qreal tol) const {
    // p in canvas (720 x 720) coordinates of the page picture
    const auto &hot = pics[picIndex()].hot;
    int best = -1; qreal bestArea = 1e18;
    for (int i = 0; i < (int)hot.size(); ++i) {
        const QRectF &r = hot[i].r;
        if (r.contains(p) && r.width() * r.height() < bestArea) { best = i; bestArea = r.width() * r.height(); }
    }
    if (best >= 0) return best;
    qreal bd = tol;
    for (int i = 0; i < (int)hot.size(); ++i) {
        const QRectF &r = hot[i].r;
        const qreal dx = qMax(qMax(r.left() - p.x(), 0.0), p.x() - r.right()), dy = qMax(qMax(r.top() - p.y(), 0.0), p.y() - r.bottom());
        const qreal d = std::hypot(dx, dy);
        if (d < bd) { bd = d; best = i; }
    }
    return best;
}

int TourView::variantPic(int page, int v) { return v <= 0 || variantBase(page) < 0 ? page : variantBase(page) + v - 1; }

void TourView::move(int dx, int dy) {
    if (!built) return;
    const auto &hot = pics[picIndex()].hot;
    if (hot.empty()) return;
    if (selIdx < 0) { cycle(1); return; }
    const QPointF cur = hot[selIdx].r.center();
    int best = -1; qreal bestScore = 1e18;
    for (int i = 0; i < (int)hot.size(); ++i) {
        if (i == selIdx) continue;
        const QPointF c = hot[i].r.center();
        const qreal ddx = c.x() - cur.x(), ddy = c.y() - cur.y();
        const qreal fwd = dx ? ddx * dx : ddy * dy;
        if (fwd < 1.5) continue;
        const qreal lat = dx ? std::fabs(ddy) : std::fabs(ddx);
        if (lat > fwd * 2.5 + 30) continue;                 // mostly sideways: not "in that direction"
        const qreal score = fwd + 2.0 * lat;
        if (score < bestScore) { bestScore = score; best = i; }
    }
    if (best >= 0) choose(best);
}

// reading order: top to bottom in bands, left to right inside a band
static std::vector<int> readingOrder(const std::vector<TourView::Hot> &hot) {
    std::vector<int> o(hot.size());
    for (size_t i = 0; i < o.size(); ++i) o[i] = (int)i;
    std::stable_sort(o.begin(), o.end(), [&](int a, int b) {
        const int ba = (int)std::floor(hot[a].r.center().y() / 28), bb = (int)std::floor(hot[b].r.center().y() / 28);
        if (ba != bb) return ba < bb;
        return hot[a].r.center().x() < hot[b].r.center().x();
    });
    return o;
}

void TourView::cycle(int d) {
    if (!built) return;
    if (conceptOn) {
        const auto &cs = cards().concepts;
        if (cs.empty()) return;
        int i = -1;
        for (int k = 0; k < (int)cs.size(); ++k) if (cs[k].key == conceptId) i = k;
        i = i < 0 ? (d > 0 ? 0 : (int)cs.size() - 1) : (i + d + (int)cs.size()) % (int)cs.size();
        selectConcept(cs[i].key);
        return;
    }
    const auto &hot = pics[picIndex()].hot;
    if (hot.empty()) return;
    const std::vector<int> o = readingOrder(hot);
    int pos = -1;
    for (int k = 0; k < (int)o.size(); ++k) if (o[k] == selIdx) pos = k;
    pos = pos < 0 ? (d > 0 ? 0 : (int)o.size() - 1) : (pos + d + (int)o.size()) % (int)o.size();
    choose(o[pos]);
}

void TourView::nextStep(int d) {
    if (!guided) return;
    step += d;
    if (step < 0) step = 0;
    if (step >= kPathLen) { // the end of the walk: carry on exploring freely
        guided = false;
        step = kPathLen - 1;
        noticeUntil = w.clock.elapsed() + 4000;   // said on the tour's own screen: the tour never writes to the real toast
        return;
    }
    setPage(pageIndex(kPath[step]));
}

void TourView::goRef(const QString &ref) {
    const CardDb &db = cards();
    QString pg, key;
    const HelpCard *c = db.resolve(ref, &pg, &key);
    if (!c) return;
    if (pg == "concepts") { selectConcept(key); return; }
    const int idx = pageIndex(pg);
    if (idx < 0) return;
    if (idx != pageIdx) setPage(idx);
    conceptOn = false;
    for (int k = 0; k < variantCount(pageIdx); ++k) {
        const int v = (variant + k) % variantCount(pageIdx);
        const auto &hot = pics[variantPic(pageIdx, v)].hot;
        for (int i = 0; i < (int)hot.size(); ++i) if (cardFor(hot[i]) == c) { if (v != variant) setVariant(v); choose(i); return; }
    }
    choose(-1);
}

void TourView::ask() {
    const HelpCard *c = nullptr;
    QString zone;
    QString page = pageId();
    if (conceptOn) {
        c = cards().conceptCard(conceptId);
        if (c) zone = "concepts." + c->key;
        else { c = nullptr; }
    } else if (selIdx >= 0 && selIdx < (int)pics[picIndex()].hot.size()) {
        c = cardFor(pics[picIndex()].hot[selIdx]);
        zone = pics[picIndex()].hot[selIdx].name;
    }
    QString text;
    if (c) text = c->ask.isEmpty() ? QString("What does %1 do?").arg(c->title) : c->ask;
    else text = QString("Tell me about the %1 page.").arg(pageInfo(pageIdx).label);
    w.tourAsk(page, zone, text);
}

// Ask Rook leaves the tour: the answer is about the real control, so the player is put on the real page
// it belongs to with the chat dock open and the question typed in (not sent: Enter sends it).
void MainWindow::tourAsk(const QString &tourPage, const QString &zone, const QString &text) {
    if (!tourView) return;
    tourView->close();
    const int idx = TourView::pageIndex(tourPage);
    if (idx >= 0) {
        const int real = TourView::pageInfo(idx).realPage;
        if (real == PG_CHAT && pg != PG_CHAT) chat.returnPage = pg;
        goPage(real);
    }
    chat.draft = text.left(380);
    guide.askFromTour(tourPage, zone);
    update();
}

// ---------------------------------------------------------------------------------- input

void TourView::key(int k, const QString &text, bool shift) {
    if (!opened) return;
    if (!built) { if (k == Qt::Key_Escape) close(); return; }
    const bool pageKey = k == Qt::Key_Comma || k == Qt::Key_Period || k == Qt::Key_Less || k == Qt::Key_Greater;
    switch (k) {
    case Qt::Key_Escape:
        if (conceptOn && !conceptId.isEmpty()) { conceptId.clear(); scrollA = scrollB = 0; }
        else if (conceptOn) conceptOn = false;
        else if (selIdx >= 0) choose(-1);
        else close();
        break;
    case Qt::Key_Left: if (!conceptOn) move(-1, 0); break;
    case Qt::Key_Right: if (!conceptOn) move(1, 0); break;
    case Qt::Key_Up: if (conceptOn) cycle(-1); else move(0, -1); break;
    case Qt::Key_Down: if (conceptOn) cycle(1); else move(0, 1); break;
    case Qt::Key_Tab: cycle(shift ? -1 : 1); break;
    case Qt::Key_Backtab: cycle(-1); break;
    case Qt::Key_Return: case Qt::Key_Enter: ask(); break;
    case Qt::Key_PageDown: case Qt::Key_Space: scrollA += 120; scrollB += 120; break;
    case Qt::Key_PageUp: scrollA = qMax(0, scrollA - 120); scrollB = qMax(0, scrollB - 120); break;
    case Qt::Key_Home: choose(-1); conceptId.clear(); break;
    case Qt::Key_V: if (!conceptOn) setVariant(variant + 1); break;
    case Qt::Key_C: conceptOn = !conceptOn; if (conceptOn) { conceptId.clear(); scrollA = scrollB = 0; } break;
    default:
        if (pageKey) {
            const int d = (k == Qt::Key_Comma || k == Qt::Key_Less) ? -1 : 1;
            if (guided) nextStep(d);
            else setPage((pageIdx + d + kPageCount) % kPageCount);
        }
        break;
    }
    (void)text;
    w.update();
}

// ---------------------------------------------------------------------------------- the card layout

// The card is two regions that each lay out from the top and scroll when they must:
//   which 0 = upper right: badge, title, summary, DOES, RECALL
//   which 1 = lower left: USE rows and key caps
//   which 2 = lower right: tips and related chips (ASK ROOK stays pinned below it)
// With paint false nothing is drawn: it only measures, so a layout check and the picture can never differ.
qreal TourView::regionN(int which, const HelpCard &c, int px, int scroll, bool paint, const QRectF &area, const QString &crumb) {
    const int sm = px - 1;
    const qreal x0 = area.x() + 10, wid = area.width() - 20;
    qreal y = 8;
    auto Y = [&](qreal cy) { return area.y() + cy - scroll; };
    auto cols = [&](int p, qreal wd) { return qMax(8, (int)(wd / w.ui.chW(p))); };
    auto visible = [&](qreal cy, qreal h) { return Y(cy) + h > area.top() && Y(cy) < area.bottom(); };
    const int L = Qt::AlignLeft | Qt::AlignVCenter;

    auto para = [&](const QString &s, int p, QColor col, bool bold, qreal x, qreal wd) -> qreal {
        if (s.trimmed().isEmpty()) return 0;
        const QStringList lines = chatWrap(s, cols(p, wd), -1);
        qreal h = 0;
        for (const QString &ln : lines) {
            if (paint && visible(y + h, p + 4)) w.ui.txt(QRectF(x, Y(y + h), wd, p + 4), ln, p, col, L, bold);
            h += p + 4;
        }
        return h;
    };
    auto label = [&](const QString &s) {
        if (paint && visible(y, 14)) {
            w.ui.txt(QRectF(x0, Y(y), 200, 14), s, 11, MID, L, true);
            const qreal lx = x0 + s.size() * w.ui.chW(11) + 8;
            w.ui.g->fillRect(QRectF(lx, Y(y) + 7, area.right() - 10 - lx, 1), GRAT);
        }
        y += 18;
    };
    // a row of chips: returns the height used; `zoneBase` non-empty makes them touchable
    auto chips = [&](const QStringList &items, bool tappable, bool cap, const QString &zoneBase, int firstId) -> qreal {
        qreal cx = 0, cy = 0;
        const int ch = 22;
        for (int i = 0; i < items.size(); ++i) {
            QString s = items[i];
            qreal cwid = s.size() * w.ui.chW(sm) + 16;
            if (cwid > wid) { s = w.ui.fit(s, sm, wid - 16); cwid = wid; }
            if (cx > 0 && cx + cwid > wid) { cx = 0; cy += ch + 5; }
            const QRectF r(x0 + cx, Y(y + cy), cwid, ch);
            if (paint && visible(y + cy, ch)) {
                if (cap) { // a key cap: a frame with a heavier foot
                    w.ui.frame(r, MID, BG);
                    w.ui.g->fillRect(QRectF(r.x() + 1, r.bottom() - 3, r.width() - 2, 2), LOW);
                    w.ui.txt(r.adjusted(0, -1, 0, -2), s, sm, INK, Qt::AlignCenter, true);
                } else {
                    const bool dn = w.ui.isDown(QString("%1%2").arg(zoneBase).arg(firstId + i));
                    w.ui.frame(r, dn ? INK : MID, dn ? INK : BG);
                    w.ui.txt(r, s, sm, dn ? BG : INK, Qt::AlignCenter, false);
                }
                if (tappable && r.bottom() > area.top() && r.top() < area.bottom())
                    w.ui.zone(r.intersected(area), QString("%1%2").arg(zoneBase).arg(firstId + i), [this, idx = firstId + i](QPointF, int ph) {
                        if (ph == PH_PRESS && idx < related.size()) goRef(related[idx]);
                    });
            }
            cx += cwid + 6;
        }
        return cy + (items.isEmpty() ? 0 : ch);
    };

    if (which == 0) {
        // badge row
        if (paint && visible(y, 16)) {
            const QString kind = (c.kind.isEmpty() ? QString("area") : c.kind).toUpper();
            const qreal bw = kind.size() * w.ui.chW(11) + 12;
            w.ui.tag(QRectF(x0, Y(y), bw, 16), kind, 11);
            w.ui.txt(QRectF(x0 + bw + 8, Y(y), wid - bw - 8, 16), w.ui.fit(crumb.toUpper(), 11, wid - bw - 8), 11, MID, Qt::AlignRight | Qt::AlignVCenter);
        }
        y += 24;
        // title: as big as fits on one line
        int tp = 22;
        while (tp > 14 && c.title.size() * w.ui.chW(tp) > wid) --tp;
        if (paint && visible(y, tp + 6)) w.ui.txt(QRectF(x0, Y(y), wid, tp + 6), w.ui.fit(c.title, tp, wid), tp, INK, L, true);
        y += tp + 10;
        qreal h = para(c.summary, px, INK, true, x0, wid);
        y += h + 8;
        if (paint && visible(y, 1)) w.ui.g->fillRect(QRectF(x0, Y(y), wid, 1), LOW);
        y += 9;
        label("DOES");
        h = para(c.does, px, INK, false, x0, wid);
        y += h + 10;
        // RECALL: its own box, because it is the part to remember
        if (!c.recall.trimmed().isEmpty()) {
            const QStringList lines = chatWrap(c.recall, cols(sm, wid - 20), -1);
            const qreal bh = 16 + lines.size() * (sm + 4) + 8;
            if (paint && visible(y, bh + 8)) {
                const QRectF box(x0, Y(y) + 7, wid, bh);
                w.ui.frame(box, INK, BG);
                w.ui.frame(box.adjusted(3, 3, -3, -3), GRAT);
                w.ui.tag(QRectF(box.x() + 8, box.y() - 8, 56, 15), "RECALL", 11);
                for (int i = 0; i < lines.size(); ++i)
                    w.ui.txt(QRectF(box.x() + 10, box.y() + 12 + i * (sm + 4), wid - 20, sm + 4), lines[i], sm, INK, L, false);
            }
            y += 7 + bh + 8;
        }
        return y;
    }
    if (which == 1) {
        label("USE");
        for (const HelpCard::Use &u : c.use) {
            const QString how = u.how.toUpper();
            const qreal tw = 50;
            const QStringList lines = chatWrap(u.text, cols(sm, wid - tw - 10), -1);
            const qreal rh = qMax<qreal>(18, lines.size() * (sm + 4));
            if (paint && visible(y, rh)) {
                const QRectF tg(x0, Y(y), tw, 17);
                if (how == "TOUCH") { w.ui.frame(tg, MID, BG); w.ui.txt(tg, how, 10, INK, Qt::AlignCenter, true); }
                else if (how == "KEYS") { w.ui.g->fillRect(tg, INK); w.ui.txt(tg, how, 10, BG, Qt::AlignCenter, true); }
                else { w.ui.g->fillRect(tg, INK); w.ui.frame(tg.adjusted(2, 2, -2, -2), BG); w.ui.txt(tg, how.isEmpty() ? QString("BOTH") : how, 10, BG, Qt::AlignCenter, true); }
                for (int i = 0; i < lines.size(); ++i)
                    w.ui.txt(QRectF(x0 + tw + 10, Y(y) + i * (sm + 4) - 1, wid - tw - 10, sm + 4), lines[i], sm, INK, L, false);
            }
            y += rh + 6;
        }
        if (!c.keys.isEmpty()) {
            y += 4;
            label("KEYS");
            y += chips(c.keys, false, true, QString(), 0) + 6;
        }
        return y + 4;
    }
    // which == 2
    if (!c.tips.isEmpty()) {
        label("TIPS");
        for (const QString &t : c.tips) {
            const qreal h = para(t, sm, MID, false, x0 + 14, wid - 14);
            if (paint && visible(y, 14)) w.ui.txt(QRectF(x0, Y(y), 14, sm + 4), "+", sm, INK, L, true);
            y += h + 6;
        }
        y += 4;
    }
    QStringList titles;
    related.clear();
    for (const QString &r : c.related) {
        const HelpCard *rc = cards().resolve(r);
        if (rc) { titles << QString::fromUtf8("› ") + rc->title; related << r; }
    }
    if (!titles.isEmpty()) {
        label("RELATED");
        y += chips(titles, true, false, "tour_rel", 0) + 6;
    }
    return y + 4;
}

TourView::Fit TourView::fitFor(const HelpCard &c, bool) {
    Fit f;
    for (int px : {13, 12, 11}) {
        f.px = px;
        f.heightA = (int)regionN(0, c, px, 0, false, kRegionA, QString());
        f.heightB = (int)qMax(regionN(1, c, px, 0, false, kRegionB1, QString()), regionN(2, c, px, 0, false, kRegionB2, QString()));
        const int hb1 = (int)regionN(1, c, px, 0, false, kRegionB1, QString()), hb2 = (int)regionN(2, c, px, 0, false, kRegionB2, QString());
        f.scrollA = f.heightA > kRegionA.height();
        f.scrollB = hb1 > kRegionB1.height() || hb2 > kRegionB2.height();
        if (!f.scrollA && !f.scrollB) break;
    }
    return f;
}

TourView::Fit TourView::measure(const HelpCard &c) { return fitFor(c, false); }

HelpCard TourView::introCard(int page) const {
    HelpCard c;
    const CardPage *cp = cards().page(QString::fromLatin1(pageInfo(page).id));
    c.key = "page";
    c.kind = "page";
    c.title = cp && !cp->title.isEmpty() ? cp->title : QString::fromLatin1(pageInfo(page).label);
    c.summary = "Tap a control in the picture to read what it does.";
    c.does = cp && !cp->intro.isEmpty() ? cp->intro : QString("This is the %1 page.").arg(pageInfo(page).label);
    c.recall = cp && !cp->recall.isEmpty() ? cp->recall : QString("Tour: Help page, Tour button.");
    c.use.push_back({"Touch", "Tap a control in the picture. Tap empty space to clear."});
    c.use.push_back({"Keys", "Arrows move between controls. Tab goes in reading order. Enter asks Rook."});
    c.use.push_back({"Keys", "Comma and period change page. C opens the concepts. Esc leaves."});
    c.keys = {"Arrows", "Tab", "Enter", ", .", "C", "Esc"};
    c.related = readAlso(QString::fromLatin1(pageInfo(page).id));
    c.ask = QString("Tell me about the %1 page.").arg(pageInfo(page).label);
    return c;
}

TourView::Fit TourView::measureIntro(int page) { return fitFor(introCard(page), true); }

// ---------------------------------------------------------------------------------- painting

void TourView::paint() {
    if (!built) { // the pictures are being drawn: a moment, with a way out
        w.ui.tag(QRectF(8, 4, 64, 26), "TOUR", 14);
        w.ui.txt(QRectF(0, 300, 720, 40), "Drawing the tour pictures...", 20, INK, Qt::AlignCenter, true);
        w.ui.txt(QRectF(0, 340, 720, 24), "Nothing on them is live. Esc leaves.", 13, MID);
        w.ui.button(QRectF(554, 4, 158, 26), "tour_close", "Leave tour", false, [this] { close(); });
        return;
    }
    paintHeader();
    if (guided) paintGuideBar(); else paintChips();
    if (conceptOn) paintConceptList(); else paintPicture();
    paintCardRegions();
}

void TourView::paintHeader() {
    w.ui.tag(QRectF(8, 4, 64, 26), "TOUR", 14);
    QString title = conceptOn ? QString("Concepts") : QString::fromLatin1(pageInfo(pageIdx).label);
    const bool multi = !conceptOn && variantCount(pageIdx) > 1;
    w.ui.txt(QRectF(82, 4, multi ? 214 : 290, 26), w.ui.fit(title.toUpper(), 16, multi ? 214 : 290), 16, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    if (multi) { // this page has more than one picture
        w.ui.button(QRectF(298, 4, 26, 26), "tour_var_prev", QString::fromUtf8("‹"), false, [this] { setVariant(variant - 1); });
        w.ui.txt(QRectF(326, 4, 36, 26), QString("%1/%2").arg(variant + 1).arg(variantCount(pageIdx)), 12, MID, Qt::AlignCenter);
        w.ui.button(QRectF(364, 4, 26, 26), "tour_var_next", QString::fromUtf8("›"), false, [this] { setVariant(variant + 1); });
    }
    w.ui.button(QRectF(396, 4, 152, 26), "tour_concepts", conceptOn ? "Pictures" : "Concepts", conceptOn, [this] {
        conceptOn = !conceptOn; conceptId.clear(); scrollA = scrollB = 0;
    });
    w.ui.button(QRectF(554, 4, 158, 26), "tour_close", "Leave tour", false, [this] { close(); });
}

void TourView::paintChips() {
    const qreal cw = 704.0 / kPageCount;
    for (int i = 0; i < kPageCount; ++i) {
        const QRectF r(std::floor(8 + i * cw), 33, std::floor(8 + (i + 1) * cw) - std::floor(8 + i * cw) + 1, 24);
        w.ui.button(r, QString("tour_page_%1").arg(i), kPages[i].chip, i == pageIdx && !conceptOn, [this, i] { setPage(i); });
    }
}

void TourView::paintGuideBar() {
    const QString head = QString("START HERE  %1 of %2").arg(step + 1).arg(kPathLen);
    w.ui.tag(QRectF(8, 33, 190, 24), head, 13, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.txt(QRectF(206, 33, 190, 24), w.ui.fit(QString::fromLatin1(pageInfo(pageIdx).label), 13, 186), 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.button(QRectF(392, 33, 92, 24), "tour_back", QString::fromUtf8("‹ Back"), false, [this] { nextStep(-1); });
    w.ui.button(QRectF(490, 33, 92, 24), "tour_next", QString::fromUtf8("Next ›"), true, [this] { nextStep(1); });
    w.ui.button(QRectF(588, 33, 124, 24), "tour_free", "Explore freely", false, [this] { guided = false; });
}

void TourView::paintPicture() {
    const Pic &pic = pics[picIndex()];
    const QRectF pr = pictureRect();
    const qreal S = kScale;
    auto scr = [&](const QRectF &r) { return QRectF(pr.x() + r.x() * S, pr.y() + r.y() * S, r.width() * S, r.height() * S); };
    // touching empty space clears the selection; touching near a control selects it (fingers are bigger than 20 pixels)
    w.ui.zone(pr, "tour_picture", [this, pr](QPointF p, int ph) {
        if (ph != PH_PRESS) return;
        const int i = nearestHot(QPointF((p.x() - pr.x()) / kScale, (p.y() - pr.y()) / kScale), 16 / kScale);
        if (i >= 0) tapHot(i); else choose(-1);
    });
    w.ui.g->drawImage(pr.topLeft(), pic.small);
    const bool sel = selIdx >= 0 && selIdx < (int)pic.hot.size();
    if (sel) { // the rest of the screen steps back
        w.ui.g->fillRect(pr, QColor(4, 4, 4, 135));
        const QRectF s = scr(pic.hot[selIdx].r).intersected(pr);
        w.ui.g->drawImage(s, pic.small, QRectF(s.x() - pr.x(), s.y() - pr.y(), s.width(), s.height()));
    }
    // every control, as a faint outline; the real ones are touchable in the order of their size so the smallest on top wins
    std::vector<int> order(pic.hot.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = (int)i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return pic.hot[a].r.width() * pic.hot[a].r.height() > pic.hot[b].r.width() * pic.hot[b].r.height(); });
    for (int i : order) {
        const QRectF r = scr(pic.hot[i].r).intersected(pr);
        if (i != selIdx) {
            const QColor c(243, 243, 239, pic.hot[i].real ? 58 : 100);
            if (pic.hot[i].real) w.ui.frame(r, c);
            else { // controls that are not zones are drawn with corner ticks only
                const qreal t = 4;
                w.ui.g->fillRect(QRectF(r.left(), r.top(), t, 1), c); w.ui.g->fillRect(QRectF(r.left(), r.top(), 1, t), c);
                w.ui.g->fillRect(QRectF(r.right() - t, r.top(), t, 1), c); w.ui.g->fillRect(QRectF(r.right() - 1, r.top(), 1, t), c);
                w.ui.g->fillRect(QRectF(r.left(), r.bottom() - 1, t, 1), c); w.ui.g->fillRect(QRectF(r.left(), r.bottom() - t, 1, t), c);
                w.ui.g->fillRect(QRectF(r.right() - t, r.bottom() - 1, t, 1), c); w.ui.g->fillRect(QRectF(r.right() - 1, r.bottom() - t, 1, t), c);
            }
        }
        const QString zn = "tour_hot_" + pic.hot[i].name;
        w.ui.zone(grow(r, r.width() < 14 || r.height() < 14 ? 4 : 0), zn, [this, i](QPointF, int ph) { if (ph == PH_PRESS) tapHot(i); });
    }
    if (sel) { // the pulsing double rule, as the highlights use
        const QRectF r = scr(pic.hot[selIdx].r).intersected(pr);
        const qreal ph = 0.5 + 0.5 * std::sin(w.clock.elapsed() / 1000.0 * 2 * M_PI / 1.6);
        QColor soft = INK; soft.setAlpha((int)(70 + 185 * ph));
        w.ui.frame(grow(r, 5), soft);
        w.ui.frame(grow(r, 4), BG);
        w.ui.frame(grow(r, 3), INK);
        w.ui.frame(grow(r, 2), BG);
    }
    w.ui.frame(grow(pr, 1), LOW);
    if (w.clock.elapsed() < noticeUntil) { // the walk is over
        const QString t = "That was the tour. Tap or arrow to explore any control.";
        const QRectF r(pr.x() + 8, pr.bottom() - 30, pr.width() - 16, 22);
        w.ui.frame(grow(r, 2), INK, BG);
        w.ui.tag(r, w.ui.fit(t, 12, r.width() - 8), 12);
    }
}

void TourView::paintConceptList() {
    const QRectF pr = pictureRect();
    const auto &cs = cards().concepts;
    const int rowH = 18, rows = (int)(pr.height() / rowH);
    w.ui.frame(grow(pr, 1), LOW, BG);
    conceptScroll = qBound(0, conceptScroll, qMax(0, (int)cs.size() - rows));
    if (cs.empty()) { w.ui.txt(pr, "No concepts available.", 13, MID); return; }
    for (int i = 0; i < rows; ++i) {
        const int ci = conceptScroll + i;
        if (ci >= (int)cs.size()) break;
        const QRectF r(pr.x() + 4, pr.y() + i * rowH + 1, pr.width() - 8, rowH - 2);
        const bool on = cs[ci].key == conceptId;
        if (on) w.ui.g->fillRect(r, INK);
        w.ui.txt(r.adjusted(6, 0, -4, 0), w.ui.fit(cs[ci].title.toUpper(), 12, r.width() - 12), 12, on ? BG : INK, Qt::AlignLeft | Qt::AlignVCenter, on);
    }
    if (conceptScroll > 0) w.ui.tag(QRectF(pr.right() - 58, pr.top() + 2, 54, 14), QString::fromUtf8("▴ less"), 10);
    if (conceptScroll + rows < (int)cs.size()) w.ui.tag(QRectF(pr.right() - 62, pr.bottom() - 16, 58, 14), QString::fromUtf8("▾ more"), 10);
    w.ui.zone(pr, "tour_conceptlist", [this, pr, rowH](QPointF p, int ph) {
        static qreal startY = 0; static int startScroll = 0; static bool dragged = false;
        if (ph == PH_PRESS) { startY = p.y(); startScroll = conceptScroll; dragged = false; return; }
        if (ph == PH_MOVE) {
            if (std::fabs(p.y() - startY) > 6) dragged = true;
            if (dragged) conceptScroll = qMax(0, startScroll - (int)std::round((p.y() - startY) / rowH));
            return;
        }
        if (!dragged) {
            const int ci = conceptScroll + (int)((p.y() - pr.y()) / rowH);
            const auto &cc = cards().concepts;
            if (ci >= 0 && ci < (int)cc.size()) selectConcept(cc[ci].key);
        }
    });
}

void TourView::paintCardRegions() {
    HelpCard intro;
    const HelpCard *c = nullptr;
    QString crumb;
    if (conceptOn) {
        c = conceptId.isEmpty() ? nullptr : cards().conceptCard(conceptId);
        crumb = "concept";
        if (!c) {
            intro.key = "concepts"; intro.kind = "area"; intro.title = "Concepts";
            intro.summary = "The ideas that run through the whole instrument.";
            intro.does = "Each concept is explained once and used everywhere: scale lock, the loop pedal, bar lines, the command layer, and Recall. Pick one from the list.";
            intro.recall = "Tour, Concepts. Or Help page, Tour, Concepts.";
            intro.use.push_back({"Touch", "Tap a concept in the list."});
            intro.use.push_back({"Keys", "Up and Down move through the list. Esc goes back to the pictures."});
            intro.keys = {"Up", "Down", "Tab", "Esc"};
            intro.ask = "Which idea should I learn first?";
            c = &intro;
        }
    } else if (selIdx >= 0 && selIdx < (int)pics[picIndex()].hot.size()) {
        c = cardFor(pics[picIndex()].hot[selIdx]);
        crumb = QString::fromLatin1(pageInfo(pageIdx).label);
    } else {
        intro = introCard(pageIdx);
        c = &intro;
        crumb = "tour";
    }
    // the explanation of Recall, always one touch away: the concept for the card being shown
    const Fit f = fitFor(*c, false);
    const int px = f.px;
    w.ui.g->fillRect(QRectF(8, 447, 704, 1), GRAT);
    w.ui.g->fillRect(QRectF(432, 454, 1, 258), GRAT);
    const QRectF aA = kRegionA, aB1 = kRegionB1, aB2 = kRegionB2;
    const qreal hA = regionN(0, *c, px, 0, false, aA, crumb), hB1 = regionN(1, *c, px, 0, false, aB1, crumb), hB2 = regionN(2, *c, px, 0, false, aB2, crumb);
    scrollA = qBound(0, scrollA, qMax(0, (int)hA - (int)aA.height()));
    scrollB = qBound(0, scrollB, qMax(0, (int)qMax(hB1 - aB1.height(), hB2 - aB2.height())));
    auto scrollZone = [&](const QRectF &a, const char *name, bool isA) {
        w.ui.zone(a, name, [this, isA](QPointF p, int ph) {
            if (ph == PH_PRESS) { dragY = (int)p.y(); return; }
            if (ph == PH_MOVE) {
                const int d = dragY - (int)p.y();
                dragY = (int)p.y();
                if (isA) scrollA = qMax(0, scrollA + d); else scrollB = qMax(0, scrollB + d);
            }
        });
    };
    scrollZone(aA, "tour_scroll_a", true);
    scrollZone(aB1, "tour_scroll_b1", false);
    scrollZone(aB2, "tour_scroll_b2", false);
    auto draw = [&](int which, const QRectF &a, int scroll, qreal total) {
        w.ui.g->save();
        w.ui.g->setClipRect(a);
        regionN(which, *c, px, scroll, true, a, crumb);
        w.ui.g->restore();
        if (total > a.height()) { // never clip silently: say there is more, and which way
            if (scroll > 0) w.ui.tag(QRectF(a.right() - 70, a.top() + 2, 66, 14), QString::fromUtf8("▴ less"), 10);
            if (scroll < total - a.height() - 1) w.ui.tag(QRectF(a.right() - 70, a.bottom() - 16, 66, 14), QString::fromUtf8("▾ more"), 10);
        }
    };
    draw(0, aA, scrollA, hA);
    draw(1, aB1, scrollB, hB1);
    draw(2, aB2, scrollB, hB2);
    // what the keys do, always in the same place
    w.ui.txt(QRectF(16, 678, 410, 14), guided ? "Arrows move   Tab cycles   Enter asks   , . steps" : "Arrows move   Tab cycles   Enter asks   C concepts", 11, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.txt(QRectF(16, 694, 410, 14), guided ? "Esc backs out" : ", . page   Space scrolls   Esc backs out", 11, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    // ASK ROOK: always in the same place
    w.ui.button(kAskRect, "tour_ask", "ASK ROOK  (Enter)", true, [this] { ask(); });
}

// ---------------------------------------------------------------------------------- state for Rook and the tests

QJsonObject TourView::stateJson() const {
    QJsonObject o;
    o["open"] = opened;
    o["building"] = opened && building;
    o["page"] = pageId().isEmpty() ? QJsonValue() : QJsonValue(pageId());
    const QString s = opened ? selectedName() : QString();
    o["selected"] = s.isEmpty() ? QJsonValue() : QJsonValue(s);
    o["concept"] = (opened && conceptOn && !conceptId.isEmpty()) ? QJsonValue(conceptId) : QJsonValue();
    o["concepts_open"] = opened && conceptOn;
    o["guided"] = opened && guided;
    o["step"] = guided ? step + 1 : 0;
    return o;
}

// ---------------------------------------------------------------------------------- the first-run offer

void TourView::armFirstRunPrompt(int delayMs, int lifeMs) {
    if (w.tourSeenFlag) return;
    promptDone = false;
    promptAt = w.clock.elapsed() + delayMs;
    promptUntil = promptAt + lifeMs;
}

bool TourView::promptShowing() const {
    if (promptDone || opened || w.tourSeenFlag) return false;
    if (promptAt < 0) return false;
    const qint64 now = w.clock.elapsed();
    return now >= promptAt && now < promptUntil;
}

void TourView::answerPrompt(bool yes) {
    markSeen();
    if (yes) open("synth", QString(), true, nullptr, true);
    w.update();
}

bool TourView::promptKey(int k) {
    if (!promptShowing()) {
        // an offer nobody answered goes away quietly, once, and is not made again
        if (!promptDone && !w.tourSeenFlag && promptAt >= 0 && w.clock.elapsed() >= promptUntil) markSeen();
        return false;
    }
    if (k == Qt::Key_Return || k == Qt::Key_Enter) { answerPrompt(true); return true; }
    if (k == Qt::Key_Escape) { answerPrompt(false); return true; }
    return false;
}

void TourView::paintPrompt() {
    if (!promptShowing()) {
        if (!promptDone && !w.tourSeenFlag && promptAt >= 0 && w.clock.elapsed() >= promptUntil) markSeen(); // unanswered: it does not come back
        return;
    }
    const QRectF r(12, 6, 696, 48);
    w.ui.frame(grow(r, 2), INK, BG);
    w.ui.frame(r, LOW);
    w.ui.zone(r, "tour_prompt", [](QPointF, int) {}); // the offer takes touches meant for what is under it
    w.ui.tag(QRectF(r.x() + 6, r.y() + 6, 56, 18), "TOUR", 12);
    w.ui.txt(QRectF(r.x() + 70, r.y() + 4, 330, 22), "Take the 2-minute tour?", 15, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    w.ui.txt(QRectF(r.x() + 8, r.y() + 27, 380, 18), "Enter = yes    Esc = later    keep playing meanwhile", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    w.ui.button(QRectF(r.right() - 270, r.y() + 6, 150, 36), "tour_prompt_yes", "Take the tour", true, [this] { answerPrompt(true); });
    w.ui.button(QRectF(r.right() - 114, r.y() + 6, 106, 36), "tour_prompt_later", "Later", false, [this] { answerPrompt(false); });
}

// ---------------------------------------------------------------------------------- pictures for the documentation

// `vrequency --render DIR` also writes these: the tour on a few pages, a concept card and the first-run offer.
int renderTourShots(MainWindow &w, const QString &dir) {
    TourView &t = w.tour();
    if (qEnvironmentVariableIsSet("VREQUENCY_TOUR_FIT")) { // developer: how do the cards fit?
        int n = 0, shrunk = 0, scrolled = 0, maxA = 0, maxB = 0;
        const CardDb &db = helpCards();
        auto one = [&](const HelpCard &c, const QString &where) {
            const TourView::Fit f = t.measure(c);
            ++n; if (f.px < 13) ++shrunk; if (f.scrollA || f.scrollB) { ++scrolled; std::fprintf(stderr, "SCROLL %s %s px%d A%d B%d\n", where.toUtf8().constData(), c.key.toUtf8().constData(), f.px, f.heightA, f.heightB); }
            maxA = qMax(maxA, f.heightA); maxB = qMax(maxB, f.heightB);
        };
        for (const CardPage &p : db.pages) for (const HelpCard &c : p.cards) one(c, p.id);
        for (const HelpCard &c : db.concepts) one(c, "concepts");
        std::fprintf(stderr, "FIT cards %d shrunk %d scrolled %d maxA %d maxB %d\n", n, shrunk, scrolled, maxA, maxB);
    }
    auto save = [&](const char *name) {
        for (int i = 0; i < 6; ++i) { w.shot(); }
        w.shot().save(QString("%1/%2.png").arg(dir, name));
    };
    w.setPage(PG_PLAY);
    t.open("synth", "octave_up");           save("20-tour-synth");
    t.open("beat", "beatrec");              save("21-tour-beat");
    t.open("track-mixer", "mixer_fader1");  save("22-tour-mixer");
    t.open("synth", "concepts.recall");     save("23-tour-concept-recall");
    t.open("synth", QString(), true);       save("24-tour-start-here");
    t.open("sound-deep", "param17");        save("25-tour-parameter");
    t.close();
    w.setPage(PG_PLAY);
    t.forgetSeen(); t.armFirstRunPrompt(0);
    save("26-tour-first-run");
    t.answerPrompt(false);
    return 0;
}
