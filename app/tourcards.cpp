// venv-vrequency — the tour's help cards: docs/helpcards.json, built into the program as a Qt resource.
// A damaged file never crashes anything: the tour then shows an error card instead of help.
#include "tour.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
QString text(const QJsonValue &v, int max = 1200) { return v.isString() ? v.toString().left(max) : QString(); }

QStringList list(const QJsonValue &v, int maxItems, int maxLen) {
    QStringList out;
    if (!v.isArray()) return out;
    for (const QJsonValue &x : v.toArray()) {
        if (x.isString() && out.size() < maxItems) out << x.toString().left(maxLen);
    }
    return out;
}

HelpCard readCard(const QString &key, const QJsonObject &o) {
    HelpCard c;
    c.key = key;
    c.title = text(o["title"], 80);
    c.kind = text(o["kind"], 20);
    c.summary = text(o["summary"], 200);
    c.does = text(o["does"], 500);
    c.recall = text(o["recall"], 300);
    c.ask = text(o["ask"], 300);
    for (const QJsonValue &v : o["use"].toArray()) {
        const QJsonObject u = v.toObject();
        if (c.use.size() < 6) c.use.push_back({text(u["how"], 10), text(u["text"], 300)});
    }
    c.keys = list(o["keys"], 12, 24);
    c.tips = list(o["tips"], 4, 300);
    c.related = list(o["related"], 8, 80);
    c.match = list(o["match"], 80, 48);
    if (c.title.isEmpty()) c.title = key;
    return c;
}
} // namespace

CardDb CardDb::parse(const QByteArray &json) {
    CardDb db;
    QJsonParseError pe;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull()) { db.error = QString("the help file is damaged (%1)").arg(pe.errorString()); return db; }
    if (!doc.isObject()) { db.error = "the help file is not an object"; return db; }
    const QJsonObject root = doc.object();
    if (!root["pages"].isObject() || root["pages"].toObject().isEmpty()) { db.error = "the help file has no pages"; return db; }
    const QJsonObject pages = root["pages"].toObject();
    for (auto it = pages.begin(); it != pages.end(); ++it) {
        if (!it.value().isObject()) continue;
        const QJsonObject po = it.value().toObject();
        CardPage p;
        p.id = it.key();
        p.title = text(po["title"], 80);
        p.intro = text(po["intro"], 400);
        p.recall = text(po["recall"], 300);
        const QJsonObject cards = po["cards"].toObject();
        for (auto ci = cards.begin(); ci != cards.end(); ++ci) {
            if (!ci.value().isObject()) continue;
            p.cards.push_back(readCard(ci.key(), ci.value().toObject()));
        }
        for (int i = 0; i < (int)p.cards.size(); ++i) {
            p.byZone.insert(p.cards[i].key, i);
            for (const QString &m : p.cards[i].match) {
                if (m.endsWith('*') && m.size() > 1) p.byPrefix.push_back({m.left(m.size() - 1), i});
                else p.byZone.insert(m, i);
            }
        }
        db.pages.push_back(std::move(p));
    }
    const QJsonObject con = root["concepts"].toObject();
    for (auto it = con.begin(); it != con.end(); ++it)
        if (it.value().isObject()) db.concepts.push_back(readCard(it.key(), it.value().toObject()));
    db.ok = !db.pages.empty();
    if (!db.ok) db.error = "the help file has no usable pages";
    return db;
}

const CardPage *CardDb::page(const QString &id) const {
    for (const CardPage &p : pages) if (p.id == id) return &p;
    return nullptr;
}

const HelpCard *CardDb::find(const QString &pageId, const QString &zone) const {
    const CardPage *p = page(pageId);
    if (!p) return nullptr;
    auto it = p->byZone.constFind(zone);
    if (it != p->byZone.constEnd()) return &p->cards[it.value()];
    for (const auto &pr : p->byPrefix) if (zone.startsWith(pr.first)) return &p->cards[pr.second];
    return nullptr;
}

const HelpCard *CardDb::conceptCard(const QString &id) const {
    for (const HelpCard &c : concepts) if (c.key == id) return &c;
    return nullptr;
}

const HelpCard *CardDb::resolve(const QString &ref, QString *pageOut, QString *keyOut) const {
    const int dot = ref.indexOf('.');
    if (dot <= 0) return nullptr;
    const QString pg = ref.left(dot), z = ref.mid(dot + 1);
    const HelpCard *c = pg == "concepts" ? conceptCard(z) : find(pg, z);
    if (c) { if (pageOut) *pageOut = pg; if (keyOut) *keyOut = c->key; }
    return c;
}

int CardDb::cardCount() const {
    int n = 0;
    for (const CardPage &p : pages) n += (int)p.cards.size();
    return n;
}

const CardDb &helpCards() {
    static const CardDb db = [] {
        QFile f(":/helpcards.json");
        if (!f.open(QIODevice::ReadOnly)) { CardDb d; d.error = "the help file is not built into this program"; return d; }
        return CardDb::parse(f.readAll());
    }();
    return db;
}
