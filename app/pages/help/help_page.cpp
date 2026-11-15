// venv-vrequency — the Help page (see help_page.h).
#include "app/pages/help/help_page.h"
#include "app/chat/chat.h"
#include "app/guide/guide.h"
#include "app/ui/palette.h"
#include <QRegularExpression>
#include <QSet>
#include <cmath>

using namespace pal;

void HelpPage::paint() {
    const KeyMap &km = sh.km;
    const std::vector<HelpRow> rows = helpRows(km, sub);
    const QString title = sub == 0 ? QString("Playing") : (sub == 2 ? QString("Pulse and recording") : QString("Commands: hold %1").arg(km.keyFor(A_COMMAND, false)));
    // header: what this page of the guide is, the tour, the next page, and the way to ask
    ui.tag(QRectF(16, 62, 242, 30), ui.fit(title, 14, 230), 14, Qt::AlignLeft | Qt::AlignVCenter);
    ui.zone(QRectF(16, 62, 242, 30), "help_section", [](QPointF, int) {});
    ui.button(QRectF(264, 62, 84, 30), "help_tour", "Tour", false, [this] { sh.perform(A_TOUR); });
    ui.button(QRectF(354, 62, 178, 30), "help_next", sub == 0 ? QString::fromUtf8("Command keys ›") : (sub == 1 ? QString::fromUtf8("Pulse keys ›") : QString::fromUtf8("‹ Playing keys")), false, [this] { sub = (sub + 1) % kHelpParts; sel.clear(); });
    ui.button(QRectF(538, 62, 166, 30), "help_ask", "Ask Rook", false, [this] { ask(); });
    // every row is a zone with a stable name built from its description, so Rook can point at it and a tap asks about it
    desc.clear();
    QSet<QString> used;
    const qreal pitch = 29.5;
    for (size_t i = 0; i < rows.size(); ++i) {
        qreal y = 100 + i * pitch;
        QString slug = rows[i].d.toLower();
        slug.replace(QRegularExpression("[^a-z0-9]+"), "_");
        slug = slug.left(34); while (slug.endsWith('_')) slug.chop(1);
        QString nm = QString("help_row_%1").arg(slug), base = nm;
        for (int n = 2; used.contains(nm); ++n) nm = QString("%1_%2").arg(base).arg(n);
        used.insert(nm);
        desc[nm] = QString("%1: %2").arg(rows[i].k, rows[i].d);
        const bool on = sel == nm;
        QRectF kr(16, std::floor(y), 176, 25);
        ui.frame(kr, on ? INK : LOW, on ? INK : BG);
        ui.txt(kr, ui.fit(rows[i].k, 13, 172), 13, on ? BG : INK, Qt::AlignCenter, true);
        ui.txt(QRectF(204, std::floor(y), 500, 25), ui.fit(rows[i].d, 13, 500), 13, on || i % 2 == 0 ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter, on);
        ui.zone(QRectF(16, std::floor(y), 688, 25), nm, [this, nm](QPointF, int ph) { if (ph == PH_PRESS) { sel = nm; ask(); } });
    }
    ui.txt(QRectF(18, 586, 680, 18), "Q20: digits and arrows live on the keyboard's number / symbol layers.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 606, 680, 18), QString("Ask Rook: tap a row, or press %1+%2. Rook answers here and can point at things.").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_CHAT, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.txt(QRectF(18, 626, 680, 18), QString("Tour: tap Tour (or press %1+%2) for a picture of every page. Tap any control to read what it does.").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_TOUR, true)), 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.button(QRectF(16, 664, 688, 44), "help_back", "Back to playing", false, [this] { sh.goPage(PG_PLAY); });
}

// Ask Rook from the Help page: opens the dock with the keyboard, and starts the question with the
// row that was tapped (or the part of the guide being read).
void HelpPage::ask() {
    static const char *parts[kHelpParts] = {"the playing keys", "the command keys", "the pulse keys"};
    if (chat.draft.trimmed().isEmpty() || chat.draft == prefill) {
        prefill = sel.isEmpty() ? QString("About %1: ").arg(parts[sub]) : QString("What does \"%1\" do? ").arg(desc.value(sel));
        chat.draft = prefill.left(380);
    }
    guide.dockOpenFocus();
}
