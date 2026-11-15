// venv-vrequency — the oscilloscope on the Synth page. It shows the instrument's real output:
// triggered so the wave stands still, with its own time and voltage scales (they follow the signal
// automatically and never affect the pitch), measured frequency and level, in one of six views
// that the 3D button cycles: the 3D stack of the latest cycles, the spectrum, the waterfall of
// spectra, the phase figure, the level history, or the fading white-phosphor line. Touching it
// is the X-Y sound control. This file lays the screen out and switches the views; the others
// draw the views (see scope.h).
#include "app/pages/synth/scope.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include "audio/audio.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

using namespace td;
using namespace pal;

namespace {
const int L = Qt::AlignLeft | Qt::AlignVCenter;
QString tbText(int i) {
    const float ms = Scope::kMsPerDiv[i];
    return ms < 1 ? QString("%1us/").arg((int)(ms * 1000)) : QString("%1ms/").arg(ms, 0, 'f', 2);
}
QString vdText(int i) {
    const float v = Scope::kVoltsPerDiv[i];
    return v < 1 ? QString("%1mV/").arg((int)std::round(v * 1000)) : QString("%1V/").arg(v, 0, 'f', 2);
}
QString noteText(float midi) {
    int n = (int)std::lround(midi);
    return QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
}
} // namespace

const char *Scope::modeShort(Mode m) {
    static const char *names[M_COUNT] = {"3D", "SPEC", "FALL", "PHASE", "LEVEL", "LINE"};
    return names[m];
}

const char *Scope::cycleMode() {
    static const char *longNames[M_COUNT] = {"3D stack", "spectrum", "waterfall", "phase", "level", "line"};
    mode = (Mode)((mode + 1) % M_COUNT);
    layerCount = 0; figCount = 0; lvlClock.invalidate();
    return longNames[mode];
}

// The graticule is drawn once: a dotted grid, ticks on the centre axes.
void Scope::paintGraticule(QRectF gr, qreal div) {
    const int GW = kTraceW, GH = kTraceH;
    if (gratPix.isNull()) {
        gratPix = QPixmap(GW + 1, GH + 1);
        gratPix.fill(Qt::transparent);
        QPainter q(&gratPix);
        q.setPen(QColor(0x58, 0x58, 0x56));
        for (int i = 0; i <= kDivsX; ++i)
            for (qreal y = 0; y <= GH; y += div / 5) q.drawPoint(QPointF(i * div, std::floor(y)));
        for (int j = 0; j <= kDivsY; ++j)
            for (qreal x = 0; x <= GW; x += div / 5) q.drawPoint(QPointF(std::floor(x), j * div));
        q.setPen(QColor(0x80, 0x80, 0x7d));
        for (qreal x = 0; x <= GW; x += div / 5) q.drawLine(QPointF(std::floor(x), GH / 2 - 2), QPointF(std::floor(x), GH / 2 + 2));
        for (qreal y = 0; y <= GH; y += div / 5) q.drawLine(QPointF(GW / 2 - 2, std::floor(y)), QPointF(GW / 2 + 2, std::floor(y)));
        q.setPen(QColor(0x46, 0x46, 0x44));
        q.drawRect(0, 0, GW, GH);
    }
    ui.g->drawPixmap(gr.topLeft(), gratPix);
}

// The trigger point (top centre), the channel ground (left margin) and the X-Y cursors.
void Scope::paintMarkers(QRectF xy, QRectF gr) {
    ui.g->setRenderHint(QPainter::Antialiasing, true);
    ui.g->setPen(Qt::NoPen);
    ui.g->setBrush(INK);
    qreal gy = gr.center().y();
    if (mode == M_LINE) { // trigger and ground markers belong to the moving line
        QPainterPath tm;
        tm.moveTo(gr.center().x() - 5, gr.y() + 1); tm.lineTo(gr.center().x() + 5, gr.y() + 1); tm.lineTo(gr.center().x(), gr.y() + 8); tm.closeSubpath();
        ui.g->drawPath(tm);
        QPainterPath gm;
        gm.moveTo(xy.x() + 4, gy - 7); gm.lineTo(xy.x() + 15, gy - 7); gm.lineTo(xy.x() + 21, gy); gm.lineTo(xy.x() + 15, gy + 7); gm.lineTo(xy.x() + 4, gy + 7); gm.closeSubpath();
        ui.g->drawPath(gm);
        ui.g->setRenderHint(QPainter::Antialiasing, false);
        ui.txt(QRectF(xy.x() + 4, gy - 7, 12, 14), "1", 11, BG, Qt::AlignCenter, true);
    }
    ui.g->setRenderHint(QPainter::Antialiasing, false);

    // X-Y cursors (touch anywhere on the scope)
    const QString xn = nice(xyAxisName(u.set.xyMap, false)), yn = nice(xyAxisName(u.set.xyMap, true));
    if (u.xyTouch || u.set.xyHold) {
        QPointF c(std::floor(xy.x() + xy.width() * u.xyX), std::floor(xy.bottom() - xy.height() * u.xyY));
        c.setX(qBound(gr.x(), c.x(), gr.right()));
        c.setY(qBound(gr.y(), c.y(), gr.bottom()));
        ui.g->setPen(QPen(u.xyTouch ? INK : MID, 1, Qt::DashLine));
        ui.g->drawLine(QPointF(c.x(), gr.y()), QPointF(c.x(), gr.bottom()));
        ui.g->drawLine(QPointF(gr.x(), c.y()), QPointF(gr.right(), c.y()));
        ui.frame(QRectF(c.x() - 7, c.y() - 7, 15, 15), INK, u.xyTouch ? INK : BG);
    }
    // axis legends sit on the graticule like the scope's own annotations
    ui.g->fillRect(QRectF(gr.x() + 6, gr.y() + 12, (yn.size() + 3) * ui.chW(12) + 4, 15), BG);
    ui.txt(QRectF(gr.x() + 8, gr.y() + 12, 300, 15), QString::fromUtf8("Y↑ %1").arg(yn), 12, MID, L);
    ui.g->fillRect(QRectF(gr.x() + 6, gr.bottom() - 19, (xn.size() + 3) * ui.chW(12) + 4, 15), BG);
    ui.txt(QRectF(gr.x() + 8, gr.bottom() - 19, 300, 15), QString::fromUtf8("X→ %1").arg(xn), 12, MID, L);
}

// The status line (inverse video, left of the two keys) and the measurement line below the trace.
void Scope::paintReadouts(QRectF xy, QRectF gr) {
    QRectF band(xy.x() + 1, xy.y() + 1, xy.width() - 276, 18);
    ui.g->fillRect(band, INK);
    ui.g->fillRect(QRectF(band.x() + 3, band.y() + 2, 14, 14), BG);
    ui.txt(QRectF(band.x() + 3, band.y() + 2, 14, 14), "1", 12, INK, Qt::AlignCenter, true);
    const QString vd = mode == M_SPECTRUM || mode == M_WATERFALL ? QString("90dB") : mode == M_LEVEL ? QString("54dB")
                     : mode == M_PHASE ? QString("Auto") : vdText(vdIdx);
    ui.txt(QRectF(band.x() + 24, band.y(), 80, 18), vd, 13, BG, L, true);
    const QString tb = mode == M_LINE ? tbText(tbIdx) : mode == M_SPECTRUM ? QString("Spectrum") : mode == M_WATERFALL ? QString("Waterfall")
                     : mode == M_PHASE ? QString("T/4") : mode == M_LEVEL ? QString("4 s")
                     : (period > 8 ? QString("1 cycle") : QString("20ms"));
    ui.txt(QRectF(band.x() + 104, band.y(), 90, 18), tb, 13, BG, L, true);
    QString trg = !sh.audio.streaming() ? "Stop" : (trigd ? "Trig'd" : "Auto");
    ui.txt(QRectF(band.x() + 200, band.y(), 70, 18), trg, 13, BG, L, true);
    ui.txt(QRectF(band.x() + 270, band.y(), band.width() - 276, 18), QString("%1 kSa/s").arg(sh.audio.sampleRate() / 1000), 13, BG, Qt::AlignRight | Qt::AlignVCenter, true);

    QRectF ml(xy.x() + 8, gr.bottom() + 2, xy.width() - 16, xy.bottom() - gr.bottom() - 3);
    QString fq = measFreq > 0 ? (measFreq >= 1000 ? QString("%1kHz").arg(measFreq / 1000, 0, 'f', 3) : QString("%1Hz").arg(measFreq, 0, 'f', 1)) : QString("----");
    QString pk = measPk >= 1 ? QString("%1V").arg(measPk, 0, 'f', 2) : QString("%1mV").arg((int)std::round(measPk * 1000));
    QString nt = "--";
    for (int r = kStrings - 1; r >= 0; --r)
        if (u.str[r].gate) { nt = noteText(u.str[r].note + u.bend); break; }
    if (mode == M_SPECTRUM || mode == M_WATERFALL || mode == M_PHASE || mode == M_LEVEL) {
        paintViewReadouts(ml, fq, nt);
        return;
    }
    ui.txt(ml, QString("Freq(1): %1").arg(fq), 13, INK, L);
    ui.txt(ml.adjusted(170, 0, 0, 0), QString("Pk-Pk(1): %1").arg(pk), 13, INK, L);
    ui.txt(ml.adjusted(340, 0, 0, 0), QString("Note: %1").arg(nt), 13, INK, L);
    if (u.xyTouch || u.set.xyHold)
        ui.txt(ml, QString("X %1%  Y %2%").arg((int)(u.xyX * 100)).arg((int)(u.xyY * 100)), 13, INK, Qt::AlignRight | Qt::AlignVCenter);
}

// The measurement line of the spectrum, waterfall, phase and level views.
void Scope::paintViewReadouts(QRectF ml, const QString &freq, const QString &note) {
    QString b, c;
    if (mode == M_PHASE) {
        b = QString("Delay: %1ms").arg(figDelayMs, 0, 'f', 2);
        c = ratioText();
    } else if (mode == M_LEVEL) {
        const float pk = lvl[Scope::kLvl - 1];
        b = pk > 1e-4f ? QString("Peak: %1dB").arg(20.0f * std::log10(pk), 0, 'f', 1) : QString("Peak: --");
        c = QString("Note: %1").arg(note);
    } else {
        b = peakHz > 0 ? QString("Peak: %1").arg(hzText(peakHz)) : QString("Peak: ----");
        c = QString("Cutoff: %1").arg(hzText(sh.soundShelf().lead.p[P_CUTOFF]));
    }
    ui.txt(ml, QString("Freq(1): %1").arg(freq), 13, INK, L);
    ui.txt(ml.adjusted(170, 0, 0, 0), b, 13, INK, L);
    ui.txt(ml.adjusted(340, 0, 0, 0), c, 13, INK, L);
}

void Scope::draw(QRectF xy) {
    const qreal div = 46;
    const QRectF gr(xy.x() + 22, xy.y() + 21, kTraceW, kTraceH);
    ui.frame(xy, ui.isDown("xy") ? INK : LOW, BG);
    if (mode == M_LINE) paintGraticule(gr, div);
    if (mode != M_LEVEL) lvlClock.invalidate();
    const View v = acquire();
    switch (mode) {
    case M_STACK: captureCycle(v); paintStack(gr, div); break;
    case M_SPECTRUM: computeSpectrum(v); paintSpectrum(gr); break;
    case M_WATERFALL: computeSpectrum(v); captureSpectrumRow(); paintStack(gr, div, true); paintFallLabels(gr); break;
    case M_PHASE: capturePhase(v); paintPhase(gr, div); break;
    case M_LEVEL: updateLevel(); paintLevel(gr); break;
    default: paintTrace(v, gr, div); break;
    }
    paintMarkers(xy, gr);
    paintReadouts(xy, gr);
}
