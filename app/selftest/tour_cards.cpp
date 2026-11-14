// venv-vrequency self-test — tour: the help cards are all there, survive a damaged file, and fit their card.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/shell/mainwindow.h"
#include "app/tour/tour.h"
#include "app/ui/zones.h"
#include <QImage>
#include <cstdio>

namespace selftest {
// the cards
void tourCards(TourContext &ctx) {
    const CardDb &db = ctx.db; const auto &check = ctx.check;
    check(db.ok && db.pages.size() == 14 && db.cardCount() >= 251 && db.concepts.size() >= 23,
          QString("the help cards are built into the program: %1 pages, %2 cards, %3 concepts").arg(db.pages.size()).arg(db.cardCount()).arg(db.concepts.size()));
    check(db.conceptCard("recall") && db.conceptCard("scale-lock") && db.conceptCard("strings") && db.conceptCard("slides") && db.conceptCard("loop-pedal"),
          "the concepts include Recall, scale lock, strings, slides and the loop pedal");
    {
        int bad = 0;
        for (const CardPage &p : db.pages) for (const HelpCard &c : p.cards)
            if (c.title.isEmpty() || c.summary.isEmpty() || c.does.isEmpty() || c.recall.isEmpty() || c.use.isEmpty() || c.ask.isEmpty()) ++bad;
        check(bad == 0, "every card has a title, summary, description, use rows, a recall line and an ask text");
    }
}

// a damaged help file never crashes anything
void tourDamagedCards(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const auto &check = ctx.check;
    const char *bad[] = {"", "{", "not json at all", "[]", "{}", R"({"version":1})", R"({"pages":[1,2,3]})", R"({"pages":{"synth":5}})",
                         R"({"pages":{"synth":{"cards":{"x":"y"}}}})", R"({"pages":{"synth":{"title":7,"cards":{"octave_up":{"title":[1],"use":"no","keys":5}}}}})"};
    int ok = 0;
    for (const char *b : bad) { const CardDb d = CardDb::parse(QByteArray(b)); if (!d.ok || d.cardCount() >= 0) ++ok; }
    check(ok == (int)(sizeof(bad) / sizeof(bad[0])), "ten kinds of damaged help file are read without a crash");
    check(!CardDb::parse("{broken").ok && !CardDb::parse("{broken").error.isEmpty() && !CardDb::parse("{}").ok, "a damaged file is reported (ok false, with a reason)");
    t.setCards(CardDb::parse("{broken"));
    t.open("synth", zn::octave_up); settleTour(w);
    const TourView::Hot *hot = nullptr;
    for (const TourView::Hot &h : t.picture(TourView::pageIndex("synth")).hot) if (h.name == zn::octave_up) hot = &h;
    const HelpCard *c = hot ? t.cardFor(*hot) : nullptr;
    check(c && c->title == "Help unavailable" && !c->does.isEmpty(), "with a damaged file every control shows an error card instead of help");
    const QImage err = w.shot();
    check(!err.isNull() && t.isOpen(), "and the tour still draws");
    tourTap(w, zn::tour_ask); w.guide.dockClose();
    check(!t.isOpen(), "(Ask Rook still works from the error card)");
    t.resetCards();
    w.setPage(PG_PLAY); w.shot();
    t.open("synth", zn::octave_up); settleTour(w);
    check(t.selectedName() == zn::octave_up && t.cardFor(*hot)->title != "Help unavailable", "the real cards come back when the file is fine");
    t.close();
}

// no card overflows
void tourCardsFit(TourContext &ctx) {
    MainWindow &w = ctx.w; TourView &t = ctx.t; const CardDb &db = ctx.db; const auto &check = ctx.check;
    int n = 0, scroll = 0, small = 0, titleBad = 0; QStringList names;
    auto one = [&](const HelpCard &c, const QString &where) {
        const TourView::Fit f = t.measure(c);
        ++n;
        if (f.px < 13) ++small;
        if (f.scrollA || f.scrollB) { ++scroll; names << where + "." + c.key; }
        if (c.title.size() * 7 > 296) ++titleBad; // even at the smallest title size it must fit one line
    };
    for (const CardPage &p : db.pages) for (const HelpCard &c : p.cards) one(c, p.id);
    for (const HelpCard &c : db.concepts) one(c, "concepts");
    std::printf("  info  %d cards measured; %d need a smaller font than the default\n", n, small);
    check(scroll == 0, QString("no card needs scrolling at the default font (%1 of %2 would)").arg(scroll).arg(n) + (names.isEmpty() ? QString() : ": " + names.join(", ").left(200)));
    check(small <= 8, QString("only a few cards need the one-step-smaller font (%1)").arg(small));
    int introScroll = 0;
    for (int p = 0; p < TourView::pageCount(); ++p) { const TourView::Fit f = t.measureIntro(p); if (f.scrollA || f.scrollB) ++introScroll; }
    check(introScroll == 0 && titleBad == 0, "the page intro cards fit too, and every title fits on one line");
    // a card that cannot fit says so: more text than any card has, never clipped silently
    HelpCard huge = *db.find("synth", zn::octave_up);
    huge.does = QString("word ").repeated(200); huge.tips = QStringList{QString("tip ").repeated(60), QString("tip ").repeated(60)};
    const TourView::Fit hf = t.measure(huge);
    check(hf.scrollA && hf.px == 11, "an oversized card shrinks the font a step, then reports that it needs scrolling");
    CardDb custom = CardDb::parse(R"({"pages":{"synth":{"title":"Synth","intro":"x","cards":{"octave_up":{"title":"BIG","kind":"button","summary":"s","does":"d","use":[{"how":"Touch","text":"t"}],"keys":[],"recall":"r","tips":[],"related":[],"ask":"a"}}}},"concepts":{}})");
    custom.pages[0].cards[0].does = QString("long text here ").repeated(120);
    t.setCards(custom);
    t.open("synth", zn::octave_up); settleTour(w);
    w.shot(); const int s0 = t.scrollOfA();
    press(w, Qt::Key_PageDown); w.shot();
    check(t.scrollOfA() > s0, "a long card scrolls (Space, Page Down, or drag)");
    t.resetCards(); t.close();
}
} // namespace selftest
