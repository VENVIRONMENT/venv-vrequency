// venv-vrequency — the Tune page's Listen panel: the button, the answer and the ways to use it.
#include "app/pages/tune/tune_page.h"
#include "app/ui/palette.h"
#include <QPainter>
#include <cmath>

using namespace td;
using namespace pal;

void TunePage::paintListen() {
    QRectF lp(16, 466, 688, 238);
    ui.frame(lp, LOW, BG);
    ui.tag(QRectF(lp.x() + 1, lp.y() + 1, 62, 18), "Listen", 12);
    ui.txt(QRectF(88, 466, 600, 20), "match the key and tempo of music around you", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    int ls = listener.state;
    ui.button(QRectF(32, 506, 200, 60), "listen", ls == 1 ? QString::fromUtf8("Listening…") : QString("Listen"), ls == 1, [this] { listener.start(); }, ls == 1 ? QString("%1 s").arg(qMax<qint64>(0, 6 - (sh.clock.elapsed() - listener.started) / 1000)) : "6 seconds, needs a mic");
    if (ls == 0) {
        ui.txt(QRectF(248, 506, 440, 60), "Uses the system's default audio input, if any.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
    } else if (ls == 3) {
        std::lock_guard<std::mutex> lk(listener.mx);
        ui.g->setFont(ui.font(13, false));
        ui.g->setPen(INK);
        ui.g->drawText(QRectF(248, 506, 440, 60), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, listener.error);
    } else if (ls == 2) {
        std::lock_guard<std::mutex> lk(listener.mx);
        const ListenResult &r = listener.result;
        const KeyGuess &k = r.keys[listener.guess % 3];
        float kc = listener.guess == 0 ? r.keyConfidence : r.keyConfidence * 0.5f;
        QString kq = kc > 0.6f ? "likely" : (kc > 0.3f ? "a guess" : "very unsure");
        ui.txt(QRectF(248, 500, 250, 40), QString("%1 %2").arg(noteName(k.root)).arg(k.minor ? "minor" : "major"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(248, 540, 250, 22), QString("key: %1 (%2%)").arg(kq).arg((int)(kc * 100)), 13, kc > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
        QString tq = r.bpmConfidence > 0.6f ? "likely" : (r.bpmConfidence > 0.3f ? "a guess" : "very unsure");
        ui.txt(QRectF(500, 500, 190, 40), r.bpm > 0 ? QString("%1 bpm").arg((int)std::round(r.bpm)) : QString("no pulse"), 24, INK, Qt::AlignLeft | Qt::AlignVCenter, true);
        ui.txt(QRectF(500, 540, 190, 22), QString("tempo: %1 (%2%)").arg(tq).arg((int)(r.bpmConfidence * 100)), 13, r.bpmConfidence > 0.6f ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
    if (ls == 2) {
        qreal bw = (656 - 3 * 8) / 4.0;
        ui.button(QRectF(32, 580, bw, 56), "lkey", "Use key", false, [this] { listener.apply(true, false); sh.toast("Key applied"); });
        ui.button(QRectF(32 + bw + 8, 580, bw, 56), "ltempo", "Use tempo", false, [this] { listener.apply(false, true); sh.toast("Tempo applied"); });
        ui.button(QRectF(32 + 2 * (bw + 8), 580, bw, 56), "lboth", "Use both", false, [this] { listener.apply(true, true); sh.toast("Key and tempo applied"); });
        ui.button(QRectF(32 + 3 * (bw + 8), 580, bw, 56), "lnext", "Other key", false, [this] { listener.guess = (listener.guess + 1) % 3; }, QString("guess %1 of 3").arg(listener.guess % 3 + 1));
        ui.txt(QRectF(32, 646, 656, 44), "Wrong? Pick the key and scale by hand above: detection is an estimate.", 12, MID, Qt::AlignLeft | Qt::AlignVCenter);
    }
}
