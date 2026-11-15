// venv-vrequency — `--render`: pictures of the tour for the documentation.
#include "app/tour/tour.h"
#include "app/shell/mainwindow.h"
#include <cstdio>

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
    t.open("pulse", "pulselatch");          save("21-tour-pulse");
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
