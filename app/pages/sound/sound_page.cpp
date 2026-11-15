// venv-vrequency — the Sound page (see sound_page.h): the ten slots, the sound buttons and the name.
#include "app/pages/sound/sound_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include "app/ui/zones.h"
#include <QPainter>

using namespace td;
using namespace pal;

SoundPage::SoundPage(Shell &s) : Page(s), sounds(s.soundShelf()), pics(s.ui) {}

void SoundPage::disarm() { deepEdit = false; warpView = false; favDelete = false; sounds.slotSave = false; }

void SoundPage::paintSlots() {
    qreal w = (688 - 4 * 8) / 5.0;
    for (int i = 0; i < 10; ++i) {
        QRectF r(std::floor(16 + (i % 5) * (w + 8)), 66 + (i / 5) * 56, std::floor(w), 50);
        QString name = QString("slot%1").arg(i);
        bool on = sounds.curSlot == i && !sounds.slotSave;
        bool inv = on || ui.isDown(name);
        ui.frame(r, inv || sounds.slotSave ? INK : LOW, inv ? INK : BG);
        if (sounds.slotSave && !inv) ui.frame(r.adjusted(3, 3, -3, -3), INK);
        // slot number in a corner tab, name beneath: laid out like a soft-key label
        ui.g->fillRect(QRectF(r.x() + 1, r.y() + 1, 18, 16), inv ? BG : LOW);
        ui.txt(QRectF(r.x() + 1, r.y() + 1, 18, 16), QString::number((i + 1) % 10), 12, inv ? INK : BG, Qt::AlignCenter, true);
        ui.txt(QRectF(r.x() + 4, r.y() + 20, r.width() - 8, 24), ui.fit(sounds.bank[i].name, 13, r.width() - 8), 13, inv ? BG : INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.zone(r, name, [this, i](QPointF, int ph) { if (ph == PH_PRESS) sounds.selectSlot(i); });
    }
    auto col = [w](int i) { return QRectF(std::floor(16 + i * (w + 8)), 182, std::floor(w), 54); };
    ui.button(col(0), "newsound", "New sound", false, [this] { sounds.randomise(); }, "randomise");
    ui.button(col(1), "mutate", "Mutate", false, [this] { sounds.mutate(); }, "nudge it");
    ui.button(col(2), "undosound", "Undo", false, [this] { sounds.undoSound(); }, QString("%1 step%2").arg(sounds.undo.size()).arg(sounds.undo.size() == 1 ? "" : "s"));
    ui.button(col(3), "slotsave", "Store", sounds.slotSave, [this] { sounds.slotSave = !sounds.slotSave; }, sounds.slotSave ? "pick a slot" : "to a slot 1-0");
    ui.button(col(4), "fav", QString::fromUtf8("♥ Save"), false, [this] { sounds.saveFavourite(); }, "favourite");
}

void SoundPage::paint() {
    paintSlots();
    ui.txt(QRectF(18, 246, 300, 40), ui.fit(sounds.lead.name, 24, 300), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
    // WARP swaps the quick sliders for the Warp view; either button leaves the other's view
    ui.button(QRectF(704 - 220 - 8 - 150, 248, 150, 38), zn::warp, "Warp", warpView && !deepEdit, [this] {
        warpView = deepEdit || !warpView; deepEdit = false;
    });
    ui.button(QRectF(704 - 220, 248, 220, 38), zn::deep, deepEdit ? QString::fromUtf8("‹ Quick controls") : QString::fromUtf8("All parameters ›"), deepEdit, [this] { deepEdit = !deepEdit; if (deepEdit) warpView = false; });
    if (deepEdit) { paintDeep(); return; }
    if (warpView) paintWarp(296);
    else paintQuick();
    paintFavourites();
}
