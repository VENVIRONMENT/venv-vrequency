// Tendril — the instrument face: chassis, control groups, key deck.
#include "hw.h"
#include "mainwindow.h"
#include "shared.h"
#include <QPainter>
#include <QPainterPath>
#include <cmath>

using namespace td;
using namespace ui;

namespace {
// control positions (canvas pixels)
const QRectF SEC_TIME(478, 32, 232, 92), SEC_FILTER(478, 226, 156, 176), SEC_SPACE(638, 226, 72, 176), SEC_RUN(478, 406, 232, 58);
const QPointF K_TIME(520, 80), K_GLIDE(660, 80), K_ENTRY(505, 180);
const QPointF K_CUT(520, 275), K_RES(594, 275), K_ENV(520, 367), K_DRIVE(594, 367), K_DELAY(674, 274), K_REVERB(674, 350), K_VOL(38, 439);
const qreal R_BIG = 25, R_TIME = 24, R_MID = 18, R_ENTRY = 19, R_SMALL = 15, R_SPACE = 17, R_VOL = 15;
const qreal BX[4] = {538, 582, 626, 670};
const qreal BY[2] = {148, 192};
const QRectF BAY(116, 422, 222, 36);
const QPointF POWER(444, 441), OUT_LAMP(356, 433), REC_LAMP(222, 448), BEAT_LAMP(700, 419);

qreal keyX(int col) { return DECK_X0 + 34 + col * DECK_DX; }
qreal rowY(int rowFromBottom) { return ROW_Y0 + (2 - rowFromBottom) * ROW_H; }
int rowShift(int rowFromBottom) { return rowFromBottom == 0 ? 1 : 0; } // the Q20's bottom letter row starts one key in

void entryIcon(QPainter &p, QPointF c, qreal r, QColor col, qreal w) {
    // the "turn the entry knob" symbol: an open circle with an arrow head
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(col, w, Qt::SolidLine, Qt::FlatCap));
    p.drawArc(QRectF(c.x() - r, c.y() - r, 2 * r, 2 * r), 120 * 16, -300 * 16);
    qreal a = 120 * M_PI / 180;
    QPointF tip(c.x() + r * std::cos(a), c.y() - r * std::sin(a));
    QPainterPath h;
    h.moveTo(tip + QPointF(4.2, 0.6));
    h.lineTo(tip + QPointF(-0.6, -3.6));
    h.lineTo(tip + QPointF(-1.0, 2.8));
    h.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    p.drawPath(h);
}
} // namespace

// ------------------------------------------------------------- frame

void MainWindow::drawAll(QPainter &painter, qreal W, qreal H) {
    u = inst.uiState();
    if (sinkGeneration != seenSinkGeneration) { // output device changed: pick a sensible loop timing offset
        seenSinkGeneration = sinkGeneration;
        set(S_LATENCY, sinkIsBluetooth ? 200.0f : 0.0f);
    }
    g = &painter;
    qreal s = qMin(W, H) / CW;
    if (std::fabs(W - CW * s) > 0.5 || std::fabs(H - CW * s) > 0.5) painter.fillRect(QRectF(0, 0, W, H), QColor(0x14, 0x13, 0x12));
    painter.translate((W - CW * s) / 2, (H - CW * s) / 2);
    painter.scale(s, s);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, s != 1.0);
    zones.clear();
    knobTurn.clear();
    if (chassisPix.isNull()) buildChassis();
    buildMenu();

    if (allDirty) painter.drawPixmap(0, 0, chassisPix);
    else for (const QRect &r : dirty) painter.drawPixmap(r, chassisPix, r);
    drawPanel();
    drawUnderTube();
    drawDeck();
    drawCrt();
    g = nullptr;
}

quint64 MainWindow::panelSignature() const {
    qint64 now = clock.elapsed();
    bool running = (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) || (u.loopState != L_EMPTY && u.loopState != L_STOPPED);
    double frac = u.beat - std::floor(u.beat);
    quint64 h = 1469598103934665603ull;
    auto mixin = [&h](quint64 v) { h = (h ^ v) * 1099511628211ull; };
    mixin(u.loopState); mixin(u.countBeat); mixin(u.loopClosing); mixin(u.layers);
    mixin((u.loopState == L_OVERDUB || u.loopState == L_REC || u.loopClosing) ? (now / 300) & 1 : 0);
    mixin(u.backPlaying); mixin(running && frac < 0.22);
    mixin(wav.isOpen() ? 2 + ((now / 400) & 1) : 0);
    mixin(sinkIsDummy * 2 + sinkIsBluetooth); mixin(audio.streaming());
    mixin(selSoft >= 0); mixin(mnu); mixin(vw); mixin(u.set.xyHold); mixin(u.set.xyMap);
    mixin((quint64)(u.set.volume * 500)); mixin((quint64)(u.set.glideScale * 100)); mixin(u.progLen + 16 * (u.holdChord + 1) + 512 * u.droneMask);
    return h;
}

quint64 MainWindow::deckSignature() const {
    quint64 h = 1469598103934665603ull;
    auto mixin = [&h](quint64 v) { h = (h ^ v) * 1099511628211ull; };
    for (int r = 0; r < kStrings; ++r) {
        mixin(u.str[r].gate); mixin(u.str[r].topCol + 1); mixin(u.str[r].held);
        mixin((quint64)((u.str[r].note + u.bend + 64) * 12));
    }
    mixin(u.curChord + 1); mixin(u.droneMask); mixin(u.backPlaying); mixin(curSlot + 1); mixin(chordRow); mixin(modChord); mixin(slotSave); mixin(u.sustain);
    mixin(slotSave ? (clock.elapsed() / 300) & 1 : 0);
    return h;
}

// ----------------------------------------------------------- chassis

void MainWindow::buildChassis() {
    QImage img(720, 720, QImage::Format_RGB32);
    img.fill(QColor(0x14, 0x13, 0x12));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // the moulded case
    {
        QPainterPath body;
        body.addRoundedRect(QRectF(1.5, 1.5, 717, 717), 9, 9);
        p.save();
        p.setClipPath(body);
        p.drawImage(0, 0, hw::plastic(720, 720, hw::CHASSIS, 3, 0x7e3d11u));
        p.restore();
        hw::plate(p, QRectF(1.5, 1.5, 717, 717), 9, QColor(), 1);
    }

    // ---- maker's label ----
    {
        QRectF lab(14, 7, 450, 19);
        hw::plate(p, lab, 5, QColor(0xdf, 0xdc, 0xd1), -1);
        p.setFont(hw::wordmarkFont(15));
        p.setPen(hw::INK);
        p.drawText(QRectF(24, 7, 80, 19), Qt::AlignLeft | Qt::AlignVCenter, "tendril");
        hw::legend(p, QRectF(86, 7, 70, 10), "OSC-720", 10, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
        hw::legend(p, QRectF(86, 16, 150, 9), "STRING SYNTHESIZER", 8, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
        // trace emblem
        QPainterPath em;
        for (int i = 0; i <= 40; ++i) {
            qreal x = 268 + i * 1.6, y = 16.5 - 5.5 * std::sin(i * 0.38) * std::exp(-i * 0.035);
            if (i == 0) em.moveTo(x, y); else em.lineTo(x, y);
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(hw::BAR_FILTER, 1.4));
        p.drawPath(em);
        hw::legend(p, QRectF(360, 7, 96, 10), "48 kSa/s", 10, Qt::AlignRight | Qt::AlignVCenter, hw::INK, true);
        hw::legend(p, QRectF(360, 16, 96, 9), "3 strings  28 voices", 8, Qt::AlignRight | Qt::AlignVCenter, hw::INK_SOFT);
        // right of the label: type plate and two case screws
        hw::screw(p, QPointF(486, 16.5), 4.2, 28);
        hw::screw(p, QPointF(702, 16.5), 4.2, 104);
        hw::legend(p, QRectF(500, 7, 190, 10), "Type 720S   Class 3 string instrument", 9, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
        hw::legend(p, QRectF(500, 16, 190, 9), "No user-serviceable notes inside", 8, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
    }

    // ---- display assembly ----
    {
        hw::plate(p, MOULD, 17, QColor(0xdc, 0xd9, 0xce), 2);
        // the opening slopes in to the tube: upper and left walls in shadow, lower and right lit
        QRectF open(16, 34, 446, 349);
        QLinearGradient slope(open.topLeft(), open.bottomRight());
        slope.setColorAt(0, QColor(0x6c, 0x69, 0x60));
        slope.setColorAt(0.40, QColor(0xa6, 0xa3, 0x98));
        slope.setColorAt(0.62, QColor(0xcb, 0xc8, 0xbd));
        slope.setColorAt(1, QColor(0xf6, 0xf4, 0xec));
        p.setPen(QPen(QColor(0x6e, 0x6b, 0x63, 170), 1));
        p.setBrush(slope);
        p.drawRoundedRect(open, 20, 20);
        // the wall meets the tube: a soft dark line where the two surfaces turn
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 60), 3));
        p.drawRoundedRect(TUBE.adjusted(-2, -2, 2, 2), 13, 13);
        // tube surround
        p.setPen(QPen(QColor(0, 0, 0, 200), 1.2));
        p.setBrush(QColor(0x0a, 0x0c, 0x0b));
        p.drawRoundedRect(TUBE, 11, 11);
        p.setPen(Qt::NoPen);
        p.setBrush(hw::CRT_BG);
        p.drawRoundedRect(GLASS, 8, 8);
        // tick marks tying each soft key to its label
        p.setPen(QPen(hw::INK_SOFT, 1));
        for (int i = 0; i < 6; ++i) {
            qreal x = GLASS.x() + (i + 0.5) * SOFT_W;
            p.drawLine(QPointF(x, 381.5), QPointF(x, 385));
        }
    }

    // ---- strip under the display ----
    {
        hw::knobSeat(p, K_VOL, R_VOL, 11, false);
        hw::legend(p, QRectF(60, 446, 50, 12), "Volume", 11, Qt::AlignLeft | Qt::AlignVCenter);
        // little loudness wedge
        QPainterPath wedge;
        wedge.moveTo(61, 441); wedge.lineTo(92, 441); wedge.lineTo(92, 433); wedge.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(hw::INK_SOFT);
        p.drawPath(wedge);
        // session drive: a slot, a tab and a lamp
        hw::plate(p, BAY, 4, QColor(0xdd, 0xda, 0xcf), 1);
        QRectF slit(BAY.x() + 10, BAY.y() + 8, BAY.width() - 20, 9);
        QLinearGradient sg(slit.topLeft(), slit.bottomLeft());
        sg.setColorAt(0, QColor(0x05, 0x05, 0x05));
        sg.setColorAt(1, QColor(0x3a, 0x38, 0x34));
        p.setPen(QPen(QColor(0x55, 0x52, 0x4b), 1));
        p.setBrush(sg);
        p.drawRoundedRect(slit, 2, 2);
        hw::plate(p, QRectF(BAY.x() + 14, BAY.y() + 21, 52, 10), 2, QColor(0xcb, 0xc8, 0xbc), 1);
        hw::plate(p, QRectF(BAY.right() - 66, BAY.y() + 21, 52, 10), 2, QColor(0xcb, 0xc8, 0xbc), 1);
        hw::legend(p, QRectF(REC_LAMP.x() + 6, REC_LAMP.y() - 6, 30, 12), "Rec", 10, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
        hw::legend(p, QRectF(BAY.x(), BAY.y() - 10, BAY.width(), 10), "Sessions and recordings", 9, Qt::AlignCenter, hw::INK_SOFT);
        hw::legend(p, QRectF(OUT_LAMP.x() + 7, OUT_LAMP.y() - 6, 50, 12), "Output", 11, Qt::AlignLeft | Qt::AlignVCenter);
        hw::legend(p, QRectF(POWER.x() - 30, 413, 60, 11), "Power", 10, Qt::AlignCenter);
        hw::legend(p, QRectF(346, 444, 76, 10), "wired", 9, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
        hw::legend(p, QRectF(346, 453, 76, 10), "amber: Bluetooth", 9, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);
    }

    // ---- control groups ----
    {
        hw::plate(p, PANEL.adjusted(-1, -1, 1, 1), 9, QColor(0xdb, 0xd8, 0xcd, 150), -1);
        hw::groove(p, SEC_TIME, 7);
        hw::sectionTitle(p, SEC_TIME, "Horizontal", hw::BAR_TIME);
        hw::knobSeat(p, K_TIME, R_TIME, TB_COUNT, false);
        hw::knobSeat(p, K_GLIDE, R_MID, 9, false);
        hw::legend(p, QRectF(K_TIME.x() - 40, K_TIME.y() + R_TIME + 6, 80, 12), "Time/Div", 11);
        hw::legend(p, QRectF(K_GLIDE.x() - 40, K_GLIDE.y() + R_MID + 10, 80, 12), "Glide", 11);
        hw::legend(p, QRectF(556, 46, 60, 10), "Touch X-Y", 9, Qt::AlignCenter, hw::INK_SOFT);
        // slow / fast marks at the ends of the time scale
        auto wave = [&p](QPointF o, qreal step) {
            QPainterPath w;
            for (int i = 0; i <= 12; ++i) {
                QPointF q(o.x() + i, o.y() - 3 * std::sin(i * step));
                if (i == 0) w.moveTo(q); else w.lineTo(q);
            }
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(hw::INK_SOFT, 1));
            p.drawPath(w);
        };
        wave(QPointF(K_TIME.x() - 41, K_TIME.y() + 30), 2.1);
        wave(QPointF(K_TIME.x() + 29, K_TIME.y() + 30), 0.55);

        // menu keys and the entry knob
        hw::knobSeat(p, K_ENTRY, R_ENTRY, 0, true);
        hw::legend(p, QRectF(K_ENTRY.x() - 26, K_ENTRY.y() + R_ENTRY + 10, 52, 12), "Entry", 11);
        struct Grp { qreal x0, x1, y; const char *name; };
        const Grp grp[3] = {{BX[0] - 3, BX[1] + 41, BY[0] - 14, "Sound"}, {BX[2] - 3, BX[3] + 41, BY[0] - 14, "Play"}, {BX[1] - 3, BX[3] + 41, BY[1] - 14, "Setup"}};
        for (const Grp &gr : grp) {
            QRectF f(gr.x0, gr.y, gr.x1 - gr.x0, 42);
            hw::groove(p, f, 5);
            hw::sectionTitle(p, f, gr.name, hw::BAR_MENU);
        }

        hw::groove(p, SEC_FILTER, 7);
        hw::sectionTitle(p, SEC_FILTER, "Filter", hw::BAR_FILTER);
        hw::knobSeat(p, K_CUT, R_BIG, 11, false);
        hw::knobSeat(p, K_RES, R_BIG, 11, false);
        hw::knobSeat(p, K_ENV, R_SMALL, 9, false);
        hw::knobSeat(p, K_DRIVE, R_SMALL, 9, false);
        hw::legend(p, QRectF(K_CUT.x() - 40, K_CUT.y() + R_BIG + 7, 80, 12), "Cutoff", 11);
        hw::legend(p, QRectF(K_RES.x() - 40, K_RES.y() + R_BIG + 7, 80, 12), "Resonance", 11);
        hw::legend(p, QRectF(K_ENV.x() - 40, K_ENV.y() + R_SMALL + 7, 80, 12), "Envelope", 11);
        hw::legend(p, QRectF(K_DRIVE.x() - 40, K_DRIVE.y() + R_SMALL + 7, 80, 12), "Drive", 11);
        hw::legend(p, QRectF(K_CUT.x() - R_BIG - 16, K_CUT.y() + R_BIG - 2, 14, 10), "40", 8, Qt::AlignRight | Qt::AlignVCenter, hw::INK_SOFT);
        hw::legend(p, QRectF(K_CUT.x() + R_BIG + 2, K_CUT.y() + R_BIG - 2, 16, 10), "16k", 8, Qt::AlignLeft | Qt::AlignVCenter, hw::INK_SOFT);

        hw::groove(p, SEC_SPACE, 7);
        hw::sectionTitle(p, SEC_SPACE, "Space", hw::BAR_SPACE);
        hw::knobSeat(p, K_DELAY, R_SPACE, 9, false);
        hw::knobSeat(p, K_REVERB, R_SPACE, 9, false);
        hw::legend(p, QRectF(K_DELAY.x() - 34, K_DELAY.y() + R_SPACE + 8, 68, 12), "Delay", 11);
        hw::legend(p, QRectF(K_REVERB.x() - 34, K_REVERB.y() + R_SPACE + 8, 68, 12), "Reverb", 11);

        hw::groove(p, SEC_RUN, 7);
        hw::sectionTitle(p, QRectF(SEC_RUN.x(), SEC_RUN.y(), SEC_RUN.width() - 40, SEC_RUN.height()), "Run control", hw::BAR_RUN);
        hw::legend(p, QRectF(BEAT_LAMP.x() - 30, BEAT_LAMP.y() - 5, 24, 10), "beat", 9, Qt::AlignRight | Qt::AlignVCenter, hw::INK_SOFT);
    }

    // ---- key deck ----
    {
        QPainterPath tray;
        tray.addRoundedRect(DECK, 8, 8);
        hw::plate(p, DECK, 8, QColor(), -2);
        p.save();
        p.setClipPath(tray);
        p.drawImage(0, 0, hw::plastic(720, 720, hw::DECK, 2, 0x51c0ffeeu, 5));
        p.restore();
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 0, 0, 70), 1));
        p.drawRoundedRect(DECK.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        auto strip = [&p](QRectF r) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 40));
            p.drawRoundedRect(r.translated(0.8, 1.2), 4, 4);
            p.setBrush(hw::STRIP);
            p.drawRoundedRect(r, 4, 4);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(QColor(0xb7, 0xb8, 0xb3), 1));
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        };
        strip(QRectF(DECK.x() + 6, NUM_Y + 2, DECK.width() - 12, 18));
        for (int row = 0; row < 3; ++row) {
            qreal y = rowY(row);
            strip(QRectF(DECK.x() + 6, y + 1, DECK.width() - 12, 22));
            // glide light pipe under the row's keys
            int n = km.rows[row].size(), sh = rowShift(row);
            QRectF pipe(keyX(sh) - 30, y + 56, keyX(sh + n - 1) - keyX(sh) + 60, 3.4);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0x5b, 0x60, 0x5c));
            p.drawRoundedRect(pipe, 1.6, 1.6);
            p.setBrush(QColor(0x2b, 0x2f, 0x2c));
            for (qreal x = pipe.x() + 1; x < pipe.right() - 3; x += 5) p.drawRect(QRectF(x, pipe.y() + 0.7, 4, 2));
        }
    }
    p.end();
    chassisPix = QPixmap::fromImage(img);
}

// ------------------------------------------------------ live controls

void MainWindow::knobCtl(QPointF c, qreal r, const QString &name, qreal angleDeg, std::function<void(float)> turn) {
    knobTurn[name] = turn;
    QRectF hit(c.x() - r - 9, c.y() - r - 9, 2 * r + 18, 2 * r + 18);
    zone(hit, name, [this, name, turn](QPointF p, int ph) {
        if (ph == PH_PRESS) { dragLast[name] = p; selKnob = name; return; }
        if (ph == PH_RELEASE) { dragLast.remove(name); return; }
        QPointF d = p - dragLast.value(name, p);
        dragLast[name] = p;
        // up/down turns the knob at full rate, sideways turns it five times finer
        float amt = (float)(-d.y() / 190.0 + d.x() / 950.0);
        if (amt != 0 && turn) turn(amt);
    });
    if (!vis(hit)) return;
    hw::knob(*g, c, r, angleDeg, downNames.contains(name));
    if (selKnob == name) { // which knob the keyboard nudges: a small green dot beside it
        g->setPen(Qt::NoPen);
        g->setBrush(QColor(0x2e, 0xa4, 0x4f));
        g->drawEllipse(QPointF(c.x() - r - 4.5, c.y() - r - 3.5), 1.9, 1.9);
    }
}

void MainWindow::paramKnob(QPointF c, qreal r, const QString &name, int param) {
    float n = paramToNorm(param, lead.p[param]);
    knobCtl(c, r, name, -135.0 + 270.0 * qBound(0.0f, n, 1.0f), [this, param](float amt) {
        float v = normToParam(param, paramToNorm(param, lead.p[param]) + amt);
        setParam(param, v);
        readout(nice(paramInfo(param).label), paramText(param, v));
    });
}

void MainWindow::capCtl(QRectF r, const QString &name, const QString &label, QColor lit, int style, std::function<void()> fn, qreal grow) {
    zone(r.adjusted(-grow, -grow, grow, grow), name, [fn](QPointF, int ph) { if (ph == PH_PRESS && fn) fn(); });
    if (!vis(r)) return;
    hw::cap(*g, r, label, downNames.contains(name), lit, style);
}

void MainWindow::drawPanel() {
    // ---- horizontal ----
    knobCtl(K_TIME, R_TIME, "timediv", -135.0 + 270.0 * tbIdx / (TB_COUNT - 1), [this](float amt) {
        float &acc = dragAcc["timediv"];
        acc += amt * (TB_COUNT - 1) * 1.6f;
        int steps = (int)acc;
        if (!steps) return;
        acc -= steps;
        tbIdx = qBound(0, tbIdx + steps, TB_COUNT - 1);
        readout("Time/Div", tbText(tbIdx).chopped(1));
    });
    knobCtl(K_GLIDE, R_MID, "glide", -135.0 + 270.0 * qBound(0.0f, u.set.glideScale / 4.0f, 1.0f), [this](float amt) {
        float v = qBound(0.0f, u.set.glideScale + amt * 4.0f, 4.0f);
        u.set.glideScale = v;
        set(S_GLIDESCALE, v);
        readout("Glide", v < 0.02f ? QString("instant") : QString("x%1").arg(v, 0, 'f', 2));
    });
    capCtl(QRectF(564, 58, 44, 20), "xymap", nice(xyMapName(u.set.xyMap)), QColor(), hw::CAP_GREY, [this] { perform(A_XYMAP); });
    capCtl(QRectF(564, 88, 44, 20), "xyhold", "Hold", u.set.xyHold ? hw::LAMP_GREEN : QColor(), hw::CAP_GREY, [this] { set(S_XYHOLD, u.set.xyHold ? 0 : 1); });

    // ---- entry knob and menu keys ----
    knobCtl(K_ENTRY, R_ENTRY, "entry", entryAngle, [this](float amt) { turnEntry(amt * 40.0f); });
    if (vis(QRectF(480, 128, 50, 30)))
        entryIcon(*g, QPointF(K_ENTRY.x() - 14, K_ENTRY.y() - R_ENTRY - 20), 6.5, selSoft >= 0 ? QColor(0x1f, 0xb0, 0x4c) : QColor(0x8f, 0xa8, 0x90), selSoft >= 0 ? 2.2 : 1.6);
    static const char *MN[M_COUNT] = {"Patch", "Edit", "Key", "Chords", "Time", "File", "Utility"};
    const QPointF pos[M_COUNT] = {{BX[0], BY[0]}, {BX[1], BY[0]}, {BX[2], BY[0]}, {BX[3], BY[0]}, {BX[1], BY[1]}, {BX[2], BY[1]}, {BX[3], BY[1]}};
    for (int m = 0; m < M_COUNT; ++m) {
        bool on = mnu == m && vw == V_SCOPE;
        capCtl(QRectF(pos[m].x(), pos[m].y(), 38, 24), QString("menu%1").arg(m), MN[m], on ? hw::LAMP_GREEN : QColor(), hw::CAP_GREY,
               [this, m] { setMenu(m, mnu == m && vw == V_SCOPE ? 0 : menuPage[m]); });
    }
    capCtl(QRectF(BX[0], BY[1], 38, 24), "autoscale", "Auto\nscale", QColor(), hw::CAP_WHITE, [this] { autoScale(); });

    // ---- filter ----
    paramKnob(K_CUT, R_BIG, "cutoff", P_CUTOFF);
    paramKnob(K_RES, R_BIG, "reso", P_RESO);
    paramKnob(K_ENV, R_SMALL, "fenv", P_FENV);
    paramKnob(K_DRIVE, R_SMALL, "drive", P_DRIVE);
    static const char *FM[3] = {"LP", "HP", "BP"};
    for (int i = 0; i < 3; ++i)
        capCtl(QRectF(488 + i * 48, 319, 40, 18), QString("fmode%1").arg(i), FM[i], (int)lead.p[P_FMODE] == i ? hw::LAMP_GREEN : QColor(), hw::CAP_OVAL,
               [this, i] { setParam(P_FMODE, (float)i); readout("Filter", QString(filterName(i)).toLower()); });

    // ---- space ----
    paramKnob(K_DELAY, R_SPACE, "delay", P_DLYMIX);
    paramKnob(K_REVERB, R_SPACE, "reverb", P_REVMIX);

    // ---- run control ----
    bool blink = (clock.elapsed() / 300) % 2 == 0;
    QString ll = "Loop";
    QColor lc;
    switch (u.loopState) {
    case L_COUNTIN: ll = QString::number(qMax(1, u.countBeat)); lc = hw::LAMP_AMBER; break;
    case L_REC: ll = "Rec"; lc = hw::LAMP_RED; break;
    case L_PLAY: if (u.loopClosing) { ll = "Rec"; lc = hw::LAMP_RED; } else { ll = u.layers > 1 ? QString("Loop x%1").arg(u.layers) : QString("Loop"); lc = hw::LAMP_GREEN; } break;
    case L_OVERDUB: ll = "Dub"; lc = blink ? hw::LAMP_RED : QColor(0xc8, 0x6a, 0x5c); break;
    default: break;
    }
    capCtl(QRectF(486, 426, 58, 28), "loop", ll, lc, hw::CAP_RUN, [this] { inst.cmd(EV_LOOP, LC_PEDAL); });
    bool lp = u.loopState == L_PLAY || u.loopState == L_OVERDUB;
    capCtl(QRectF(552, 426, 54, 28), "loopstop", "Run/Stop", lp ? hw::LAMP_GREEN : (u.loopState == L_STOPPED ? hw::LAMP_RED : QColor()), hw::CAP_RUN,
           [this] { inst.cmd(EV_LOOP, LC_PLAYSTOP); });
    capCtl(QRectF(614, 430, 38, 22), "loopundo", "Undo", QColor(), hw::CAP_GREY, [this] { inst.cmd(EV_LOOP, LC_UNDO); });
    bool hasBack = u.progLen || u.holdChord >= 0 || u.droneMask;
    capCtl(QRectF(660, 426, 44, 28), "backplay", "Backing", u.backPlaying && hasBack ? hw::LAMP_GREEN : QColor(), hw::CAP_RUN, [this] { inst.cmd(EV_BACK, BC_PLAYSTOP); });
    if (vis(QRectF(BEAT_LAMP.x() - 8, BEAT_LAMP.y() - 8, 16, 16))) {
        double frac = u.beat - std::floor(u.beat);
        bool running = (u.backPlaying && (u.progLen || u.holdChord >= 0 || u.droneMask)) || (u.loopState != L_EMPTY && u.loopState != L_STOPPED);
        hw::lamp(*g, BEAT_LAMP, 2.6, hw::LAMP_AMBER, running && frac < 0.22);
    }
}

void MainWindow::drawUnderTube() {
    // the six soft keys
    for (int i = 0; i < 6; ++i) {
        qreal x = GLASS.x() + (i + 0.5) * SOFT_W;
        capCtl(QRectF(x - 22, 387, 44, 16), QString("soft%1").arg(i), QString(), QColor(), hw::CAP_SOFT, [this, i] { pressSoft(i); }, 6);
    }
    knobCtl(K_VOL, R_VOL, "volume", -135.0 + 270.0 * qBound(0.0f, u.set.volume, 1.0f), [this](float amt) {
        float v = qBound(0.0f, u.set.volume + amt, 1.0f);
        u.set.volume = v;
        set(S_VOLUME, v);
        readout("Volume", QString("%1%").arg((int)std::round(v * 100)));
    });
    zone(BAY, "drive", [this](QPointF, int ph) { if (ph == PH_PRESS) setMenu(M_FILE); });
    if (vis(BAY)) {
        bool rec = wav.isOpen();
        hw::lamp(*g, REC_LAMP, 2.8, hw::LAMP_RED, rec && (clock.elapsed() / 400) % 2 == 0);
    }
    if (vis(QRectF(OUT_LAMP.x() - 10, OUT_LAMP.y() - 10, 20, 20)))
        hw::lamp(*g, OUT_LAMP, 3.0, sinkIsDummy ? hw::LAMP_RED : (sinkIsBluetooth ? hw::LAMP_AMBER : hw::LAMP_GREEN), audio.streaming() || sinkIsDummy);
    capCtl(QRectF(POWER.x() - 12, POWER.y() - 12, 24, 24), "power", QString(), QColor(), hw::CAP_OVAL, [this] {
        setMenu(M_UTIL);
        toast("Press Quit to switch off", 2000);
    }, 6);
    // the cap is CAP_OVAL (a grey disc); give it the white power-button face
    if (vis(QRectF(POWER.x() - 14, POWER.y() - 14, 28, 28))) {
        bool dn = downNames.contains("power");
        QRadialGradient gr(POWER + QPointF(-3, -4), 15);
        gr.setColorAt(0, QColor(0xff, 0xff, 0xfb));
        gr.setColorAt(1, dn ? QColor(0xc4, 0xc1, 0xb6) : QColor(0xdc, 0xd9, 0xce));
        g->setPen(Qt::NoPen);
        g->setBrush(gr);
        g->drawEllipse(POWER + QPointF(0, dn ? 1 : 0), 10.5, 10.5);
        g->setBrush(Qt::NoBrush);
        g->setPen(QPen(hw::INK_SOFT, 1.3, Qt::SolidLine, Qt::RoundCap));
        g->drawArc(QRectF(POWER.x() - 4, POWER.y() - 4 + (dn ? 1 : 0), 8, 8), 130 * 16, 280 * 16);
        g->drawLine(POWER + QPointF(0, -5 + (dn ? 1 : 0)), POWER + QPointF(0, -1 + (dn ? 1 : 0)));
    }
}

// ----------------------------------------------------------- key deck

void MainWindow::buildDeckLegends() {
    const Tuning &t = u.set.tuning;
    QString sig = QString("%1/%2/%3/%4/%5/%6/%7").arg(t.root).arg(t.scale).arg(t.octave).arg(t.rowInterval).arg(u.set.seventh).arg(chordRow || modChord).arg(curSlot);
    for (const Patch &b : bank) sig += QString("/") + b.name;
    if (sig == deckSig && !deckPix.isNull()) return;
    deckSig = sig;
    QImage img(720, 720, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    // number row: sound slots
    for (int i = 0; i < 10; ++i) {
        qreal cx = keyX(i);
        hw::legend(p, QRectF(cx - 31, NUM_Y + 2, 12, 18), QString::number((i + 1) % 10), 13, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
        QFont f = hw::labelFont(10, curSlot == i);
        QString nm = QFontMetrics(f).elidedText(bank[i].name, Qt::ElideRight, 48);
        hw::legend(p, QRectF(cx - 19, NUM_Y + 2, 50, 18), nm, 10, Qt::AlignLeft | Qt::AlignVCenter, hw::LEG_ORANGE, curSlot == i);
    }
    bool chords = chordRow || modChord;
    for (int row = 0; row < 3; ++row) {
        qreal y = rowY(row);
        int n = km.rows[row].size(), sh = rowShift(row);
        for (int c = 0; c < n; ++c) {
            qreal cx = keyX(c + sh);
            int note = t.noteFor(row, c, false);
            bool isRoot = ((note - t.root) % 12 + 12) % 12 == 0;
            hw::legend(p, QRectF(cx - 31, y + 1, 14, 22), QString(km.rows[row][c]), 14, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
            QString nn = QString("%1%2").arg(noteName(note)).arg(note / 12 - 1);
            bool dimNote = row == 0 && chords;
            hw::legend(p, QRectF(cx - 17, y + 1, 26, 22), nn, 11, Qt::AlignLeft | Qt::AlignVCenter, dimNote ? QColor(0x9d, 0xb4, 0xd6) : (isRoot ? hw::LEG_RED : hw::LEG_BLUE), isRoot && !dimNote);
            if (row == 0 && c < 7) {
                Chord ch = buildChord(t, c, u.set.seventh);
                QFont f = hw::labelFont(11, chords);
                QString cn = QFontMetrics(f).elidedText(ch.name, Qt::ElideRight, 26);
                hw::legend(p, QRectF(cx + 1, y + 1, 27, 22), cn, 11, Qt::AlignLeft | Qt::AlignVCenter, chords ? hw::LEG_RED : QColor(0xcf, 0x7d, 0x73), chords);
            }
        }
    }
    // function keys in the spare positions
    hw::legend(p, QRectF(keyX(9) - 31, rowY(1) + 1, 62, 22), "sus", 13, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
    hw::legend(p, QRectF(keyX(9) - 31, rowY(1) + 1, 62, 22), "sustain", 10, Qt::AlignRight | Qt::AlignVCenter, hw::KEY_GREEN);
    hw::legend(p, QRectF(keyX(0) - 31, rowY(0) + 1, 62, 22), "alt", 13, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
    hw::legend(p, QRectF(keyX(0) - 31, rowY(0) + 1, 62, 22), "chords", 10, Qt::AlignRight | Qt::AlignVCenter, hw::LEG_RED);
    hw::legend(p, QRectF(keyX(8) - 31, rowY(0) + 1, 62, 22), "oct", 13, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
    hw::legend(p, QRectF(keyX(8) - 31, rowY(0) + 1, 62, 22), "down", 10, Qt::AlignRight | Qt::AlignVCenter, hw::LEG_BLUE);
    hw::legend(p, QRectF(keyX(9) - 31, rowY(0) + 1, 62, 22), "oct", 13, Qt::AlignLeft | Qt::AlignVCenter, hw::INK, true);
    hw::legend(p, QRectF(keyX(9) - 31, rowY(0) + 1, 62, 22), "up", 10, Qt::AlignRight | Qt::AlignVCenter, hw::LEG_BLUE);
    p.end();
    deckPix = QPixmap::fromImage(img);
}

void MainWindow::drawDeck() {
    const Tuning &t = u.set.tuning;
    buildDeckLegends();
    bool show = vis(DECK);
    if (show) {
        if (allDirty) g->drawPixmap(DECK.toRect(), deckPix, DECK.toRect());
        else for (const QRect &r : dirty) { QRect d = r.intersected(DECK.toRect()); if (!d.isEmpty()) g->drawPixmap(d, deckPix, d); }
    }
    const QColor rubber(0x1c, 0x1d, 0x1f), backlight(0x6d, 0xff, 0x9c);

    // ---- number row: the ten sounds ----
    bool blink = (clock.elapsed() / 300) % 2 == 0;
    for (int i = 0; i < 10; ++i) {
        qreal cx = keyX(i);
        QString name = QString("slot%1").arg(i);
        zone(QRectF(cx - 34, NUM_Y, 68, NUM_H), name, [this, i](QPointF, int ph) { if (ph == PH_PRESS) selectSlot(i); });
        if (!show) continue;
        bool dn = downNames.contains(name) || held.contains(i == 9 ? Qt::Key_0 : Qt::Key_1 + i);
        hw::rubberKey(*g, QRectF(cx - 20, NUM_Y + 24, 42, 17), dn, QColor(), rubber);
        hw::lamp(*g, QPointF(cx + 29, NUM_Y + 32.5), 2.2, slotSave ? hw::LAMP_AMBER : hw::LAMP_GREEN, slotSave ? blink : curSlot == i);
    }

    // ---- three strings ----
    for (int row = 0; row < 3; ++row) {
        qreal y = rowY(row);
        int count = km.rows[row].size(), sh = rowShift(row);
        const StringUi &su = u.str[row];
        bool chords = row == 0 && (chordRow || modChord);
        for (int c = 0; c < count; ++c) {
            qreal cx = keyX(c + sh);
            QString cname = QString("chord%1").arg(c);
            if (chords && c < 7) zone(QRectF(cx - 34, y, 68, ROW_H), cname, [this, c](QPointF, int ph) { if (ph == PH_PRESS) inst.cmd(EV_CHORD, c); });
            if (!show) continue;
            bool phys = held.contains(km.rows[row][c].unicode());
            bool dn = phys || touchCol[row] == c || (chords && downNames.contains(cname));
            bool sounding = !chords && su.gate && su.topCol == c;
            bool chordOn = chords && c < 7 && (u.curChord == c || ((u.droneMask >> c) & 1));
            QColor glow = sounding ? backlight : (chordOn ? QColor(0xff, 0x8a, 0x70) : QColor());
            hw::rubberKey(*g, QRectF(cx - 22, y + 28, 46, 23), dn, glow, rubber);
        }
        if (!chords) {
            const qreal x0 = keyX(sh), dx = DECK_DX;
            zone(QRectF(x0 - 34, y, (count - 1) * dx + 68, ROW_H), QString("string%1").arg(row), [this, row, count, x0, dx](QPointF p, int ph) {
                int col = qBound(0, (int)std::floor((p.x() - x0) / dx + 0.5), count - 1);
                int &cur = touchCol[row];
                if (ph == PH_RELEASE) { if (cur >= 0) inst.key(row, cur | 0x100, false); cur = -1; return; }
                if (col == cur) return;
                inst.key(row, col | 0x100, true, modChromatic); // press the new key first: on a held string that is a slide
                if (cur >= 0) inst.key(row, cur | 0x100, false);
                cur = col;
            });
        }
        // glide light pipe: one lit segment follows the sounding pitch between the keys
        if (show && su.gate) {
            float pbase = su.note;
            qreal px;
            int prev = t.noteFor(row, 0, false);
            if (pbase <= prev) px = keyX(sh) - qMin(0.4, (double)(prev - pbase) / 6.0) * DECK_DX;
            else {
                px = keyX(sh + count - 1) + qMin(0.4, (double)(pbase - t.noteFor(row, count - 1, false)) / 6.0) * DECK_DX;
                for (int c = 1; c < count; ++c) {
                    int n = t.noteFor(row, c, false);
                    if (pbase <= n) { px = keyX(sh + c - 1) + DECK_DX * (pbase - prev) / qMax(1, n - prev); break; }
                    prev = n;
                }
            }
            px += u.bend * 6.0;
            qreal left = keyX(sh) - 29;
            int seg = (int)std::floor((px - left) / 5.0);
            g->setPen(Qt::NoPen);
            for (int k = -2; k <= 2; ++k) {
                qreal x = left + (seg + k) * 5.0;
                if (x < left - 1 || x > keyX(sh + count - 1) + 26) continue;
                int a = k == 0 ? 255 : (std::abs(k) == 1 ? 150 : 60);
                g->setBrush(QColor(0x7d, 0xff, 0xa6, a));
                g->drawRect(QRectF(x, y + 56.7, 4, 2));
                if (k == 0) { g->setBrush(QColor(0x7d, 0xff, 0xa6, 60)); g->drawRect(QRectF(x - 1.5, y + 55.2, 7, 5)); }
            }
        }
    }

    // ---- function keys in the spare positions ----
    struct Fn { int row, col; const char *name; QColor body; bool on; std::function<void()> fn; };
    const Fn fns[4] = {
        {1, 9, "sus", hw::KEY_GREEN.darker(135), u.sustain, [this] { sustainLatch(); }},
        {0, 0, "chordrow", rubber, chordRow, [this] { perform(A_CHORD_ROW); }},
        {0, 8, "octdn", rubber, false, [this] { inst.cmd(EV_OCTAVE, -1); }},
        {0, 9, "octup", rubber, false, [this] { inst.cmd(EV_OCTAVE, 1); }},
    };
    for (const Fn &f : fns) {
        qreal cx = keyX(f.col), y = rowY(f.row);
        auto fn = f.fn;
        zone(QRectF(cx - 34, y, 68, ROW_H), f.name, [fn](QPointF, int ph) { if (ph == PH_PRESS) fn(); });
        if (!show) continue;
        bool dn = downNames.contains(f.name) || (f.row == 1 && susKey);
        hw::rubberKey(*g, QRectF(cx - 22, y + 28, 46, 23), dn || f.on, f.on ? (f.row == 1 ? QColor(0x6d, 0xff, 0x9c) : QColor(0xff, 0x8a, 0x70)) : QColor(), f.body);
    }
}
