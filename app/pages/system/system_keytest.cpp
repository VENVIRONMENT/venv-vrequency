// venv-vrequency — the key test: a panel over the page that shows which keys are down, for finding
// the limits of a keyboard matrix.
#include "app/pages/system/system_page.h"
#include "app/shell/keymap.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace pal;

void SystemPage::paintKeyTest() {
    KeyState &keys = sh.keys;
    ui.zone(QRectF(0, 0, 720, 720), "keytestblock", [](QPointF, int) {}); // the panel swallows every touch but Close
    QRectF r(40, 96, 640, 520);
    ui.frame(r.adjusted(-3, -3, 3, 3), INK, BG);
    ui.frame(r, INK, BG);
    ui.tag(QRectF(r.x() + 1, r.y() + 1, 100, 22), "Key test", 14, Qt::AlignLeft | Qt::AlignVCenter);
    QStringList names;
    for (auto it = keys.held.begin(); it != keys.held.end(); ++it) names << keyName(it.key());
    names.sort();
    ui.txt(QRectF(60, 150, 600, 22), ui.fit(QString("Held now (%1): %2").arg(names.size()).arg(names.join(" ")), 14, 600), 14, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    ui.tag(QRectF(60, 176, 300, 22), QString("Most keys held at once: %1").arg(keys.maxHeld), 14, Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < keys.log.size(); ++i)
        ui.txt(QRectF(60, 210 + i * 24, 600, 22), keys.log[i], 14, keys.log[i].startsWith("DOWN") ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    ui.g->setFont(ui.font(13, false));
    ui.g->setPen(MID);
    ui.g->drawText(QRectF(60, 506, 600, 80), Qt::AlignLeft | Qt::TextWordWrap,
                "Hold one key on each row, then add more. Every key should show DOWN once and stay in Held until you let go. "
                "A key you did not press appearing here, or a pressed key missing, is a keyboard-matrix limit.");
    ui.button(QRectF(516, 100, 160, 44), "keytestclose", "Close", false, [this] { sh.keys.testOpen = false; });
}
