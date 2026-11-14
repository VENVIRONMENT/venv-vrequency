// venv-vrequency — the interactive tour. A faithful, inert picture of each page (the real page drawn
// off-screen with a demo state) where every control is a hotspot: touch it, or move to it with the arrow
// keys, and a readout card says what it does, how to use it by touch and by keys, and how to get back to
// it later (RECALL). The cards come from docs/helpcards.json, embedded in the binary.
//
// Nothing in the tour is live: it draws pictures and reads cards. It never sends a key, a touch or a
// setting to the synth, and it never changes the real page.
#pragma once
#include <QHash>
#include <QImage>
#include <QJsonObject>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVector>
#include <vector>

class MainWindow;

// ---------------------------------------------------------------------------------- the cards

struct HelpCard {
    QString key, title, kind, summary, does, recall, ask;
    struct Use { QString how, text; };
    QVector<Use> use;
    QStringList keys, tips, related, match;
};

struct CardPage {
    QString id, title, intro, recall;
    std::vector<HelpCard> cards;
    QHash<QString, int> byZone;                // zone name (a card's own key or any of its `match`) -> card index
    std::vector<std::pair<QString, int>> byPrefix; // a `match` entry ending in * -> card index
};

class CardDb {
public:
    bool ok = false;
    QString error;                          // why the file could not be used
    std::vector<CardPage> pages;
    std::vector<HelpCard> concepts;

    // Never throws or crashes: a damaged file gives ok == false and an error message.
    static CardDb parse(const QByteArray &json);
    const CardPage *page(const QString &id) const;
    // a card by zone name (its own key or any name in `match`), or null
    const HelpCard *find(const QString &page, const QString &zone) const;
    const HelpCard *conceptCard(const QString &id) const;
    // "page.zone" or "concepts.id"
    const HelpCard *resolve(const QString &ref, QString *page = nullptr, QString *key = nullptr) const;
    int cardCount() const;
};

// The cards built into the program (docs/helpcards.json), read once.
const CardDb &helpCards();

// ---------------------------------------------------------------------------------- the tour

class TourView {
public:
    explicit TourView(MainWindow &w);
    ~TourView();

    // the pages of the tour, in order: id (also the name the control socket uses), label, chip text
    struct PageInfo { const char *id, *label, *chip; int realPage; };
    static int pageCount();
    static const PageInfo &pageInfo(int i);
    static int pageIndex(const QString &name);
    static int picCount();                 // pages plus the extra pictures of pages that need more than one
    static int picPage(int pic);           // the page a picture belongs to       // -1 when unknown; accepts play / back / sys too

    // One hotspot: a control on the picture. `ref` is the card it shows.
    struct Hot { QString name; QRectF r; QString ref; bool real = true; };
    struct Pic { QImage img; QImage small; std::vector<Hot> hot; };

    // ---- opening and closing ----
    // page: tour page name (empty: the last one looked at); element: a control name, card key or concept id.
    // Returns false (and says why in err) for an unknown page. An unknown element opens the page with nothing selected.
    bool open(const QString &page = QString(), const QString &element = QString(), bool guided = false, QString *err = nullptr, bool defer = false);
    bool isBuilding() const { return opened && building; }
    void close();
    bool isOpen() const { return opened; }
    void paint();
    void key(int qtKey, const QString &text, bool shift);

    // ---- the first-run offer ----
    void armFirstRunPrompt(int delayMs, int lifeMs = 30000);   // shows after delayMs; if nobody answers within lifeMs it goes away for good
    bool promptShowing() const;
    bool promptKey(int qtKey);          // Enter = yes, Esc = later; true when the key was used
    void paintPrompt();
    void answerPrompt(bool yes);
    bool seen() const;
    void forgetSeen();                  // test / developer hook: behave as a first run again

    // ---- for the control socket and the self-test ----
    QJsonObject stateJson() const;
    QString pageId() const { return pageIdx >= 0 ? QString::fromLatin1(pageInfo(pageIdx).id) : QString(); }
    QString selectedName() const;
    QString selectedRef() const;
    QString conceptShown() const { return conceptOn ? conceptId : QString(); }
    bool guidedMode() const { return guided; }
    int guidedStep() const { return step; }
    bool conceptsOpen() const { return conceptOn; }
    const Pic &picture(int page);
    bool select(const QString &name);                     // choose a hotspot by control name or card key
    void selectConcept(const QString &id);
    void setCards(const CardDb &db);                      // test hook: use these cards instead of the built-in ones
    void resetCards();
    const CardDb &cards() const;
    // card layout facts (no drawing): the font size used, and whether any part needs scrolling
    struct Fit { int px = 13; bool scrollA = false, scrollB = false; int heightA = 0, heightB = 0; };
    Fit measure(const HelpCard &c);
    Fit measureIntro(int page);
    // the card a hotspot shows (never null: an error or "missing" card stands in)
    const HelpCard *cardFor(const Hot &h) const;
    QRectF pictureRect() const { return QRectF(8, 62, 380, 380); }
    static QString asciiTitle(const HelpCard &c) { return c.title; }
    int scrollOfA() const { return scrollA; }
    int variantShown() const { return variant; }
    int variantsOnPage() const { return variantCount(pageIdx); }

private:
    MainWindow &w;
    bool opened = false;
    int pageIdx = 4;                 // Synth
    int selIdx = -1;                 // hotspot index on the page, or -1
    int variant = 0;                 // which picture of a page that has several
    bool conceptOn = false;          // the Concepts view
    QString conceptId;               // the concept being read there
    int conceptScroll = 0;
    bool guided = false;
    int step = 0;
    int scrollA = 0, scrollB = 0;
    int prevPage = -1, prevSel = -1;
    bool built = false, building = false;
    int pendingPage = 0; QString pendingElement; bool pendingGuided = false;
    std::vector<Pic> pics;
    CardDb override_;
    bool useOverride = false;
    mutable HelpCard missing;
    // first-run offer
    qint64 promptAt = -1, promptUntil = 0;
    bool promptOn = false, promptDone = false;
    int dragY = 0;
    qint64 noticeUntil = 0;
    QStringList related;             // the related refs of the card last laid out, for its chips

    static int variantCount(int page);
    static int variantBase(int page);
    static int variantPic(int page, int v);
    int picIndex() const;
    void setVariant(int v);
    void tapHot(int idx);
    void finishOpen(int idx, const QString &element, bool guide);
    void ensureBuilt();
    void build();
    void markSeen();
    void setPage(int i, bool keepSelection = false);
    void choose(int idx);
    void move(int dx, int dy);
    void cycle(int d);
    void paintHeader();
    void paintPicture();
    void paintConceptList();
    void paintCardRegions();
    void paintGuideBar();
    void paintChips();
    void ask();
    void goRef(const QString &ref);
    void nextStep(int d);
    int nearestHot(QPointF p, qreal tol) const;
    HelpCard introCard(int page) const;
    Fit fitFor(const HelpCard &c, bool intro);
    qreal regionN(int which, const HelpCard &c, int px, int scroll, bool paint, const QRectF &area, const QString &crumb);
};

// ---------------------------------------------------------------------------------- the pictures

// Draws every tour page off-screen in a scratch copy of the app with a demo state (a filled
// progression, a beat, a loop, clips on the track, a lit scope). Touches no file and no live audio.
class SinkProbe;
void tourBuildPictures(std::vector<TourView::Pic> &pics, SinkProbe *sink = nullptr);   // sink: the real window's output device, shown on the System page
// `vrequency --dump-zones`: prints one "page zone" line per real zone of every tour page.
int tourDumpZones(const QString &pictureDir = QString());

// `vrequency --render DIR` also calls this: pictures of the tour for the documentation.
int renderTourShots(MainWindow &w, const QString &dir);
