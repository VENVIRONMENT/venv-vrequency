// venv-vrequency — the Sound page's favourites: six to a page, recall or (when armed) delete.
#include "app/pages/sound/sound_page.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <QFile>
#include <QPainter>

using namespace td;
using namespace pal;

void SoundPage::paintFavourites() {
    const int per = 6, pages = qMax(1, ((int)sounds.favs.size() + per - 1) / per);
    favPage = qBound(0, favPage, pages - 1);
    ui.section(534, QString("Favourites  %1").arg(sounds.favs.size()));
    qreal cw = (688 - 110 - 3 * 8) / 3.0;
    for (int k = 0; k < per; ++k) {
        int idx = favPage * per + k;
        QRectF r(std::floor(16 + (k % 3) * (cw + 8)), 560 + (k / 3) * 74, std::floor(cw), 66);
        if (idx >= sounds.favs.size()) { ui.frame(r, GRAT); continue; }
        ui.button(r, QString("favitem%1").arg(k), sounds.favs[idx].patch.name, false, [this, idx] {
            if (idx >= sounds.favs.size()) return;
            if (favDelete) { QFile::remove(sounds.favs[idx].file); sh.refreshLists(); favDelete = false; sh.toast("Favourite removed"); return; }
            sounds.recall(sounds.favs[idx].patch);
        }, favDelete ? "tap to delete" : "recall", favDelete ? LOOK_ARMED : LOOK_PLAIN);
    }
    qreal bx = 704 - 102;
    { // the arrows say which way: the left half goes back a page, the right half forward, both wrapping
        const QRectF r(bx, 560, 102, 30);
        const bool inv = ui.isDown("favprev");
        ui.frame(r, inv ? INK : LOW, inv ? INK : BG);
        ui.txt(r, QString::fromUtf8("‹ %1/%2 ›").arg(favPage + 1).arg(pages), 14, inv ? BG : INK, Qt::AlignCenter, true);
        ui.zone(r, "favprev", [this, r, pages](QPointF p, int ph) {
            if (ph == PH_PRESS) favPage = (favPage + (p.x() < r.center().x() ? pages - 1 : 1)) % pages;
        });
    }
    ui.button(QRectF(bx, 598, 102, 102), "favdel", "Delete", favDelete, [this] { favDelete = !favDelete; }, favDelete ? "pick one" : "a favourite");
}
