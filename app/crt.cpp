// Tendril — what the tube shows, and the soft-key menus printed along its bottom edge.
#include "hw.h"
#include "mainwindow.h"
#include "shared.h"
#include <QFileInfo>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

using namespace td;
using namespace ui;

namespace {
QString onOff(bool b) { return b ? "on" : "off"; }
QString noteText(float midi) {
    int n = (int)std::lround(midi);
    int cents = (int)std::lround((midi - n) * 100);
    QString s = QString("%1%2").arg(noteName(n)).arg(n / 12 - 1);
    if (std::abs(cents) >= 3) s += QString("%1%2c").arg(cents > 0 ? "+" : "").arg(cents);
    return s;
}
} // namespace

// ------------------------------------------------------------ text

void MainWindow::crtText(QRectF r, const QString &s, int px, QColor c, int align, bool bold) {
    g->setFont(hw::crtFont(px, bold));
    g->setPen(c);
    g->drawText(r, align | Qt::TextSingleLine | Qt::TextDontClip, s);
}

void MainWindow::crtInverse(QRectF r, const QString &s, int px, int align) {
    g->fillRect(r, hw::CRT_BAND);
    crtText(r.adjusted(3, 0, -3, 0), s, px, hw::CRT_DARK, align, true);
}

// ----------------------------------------------------------- menus

MainWindow::Soft MainWindow::paramSoft(int p) {
    Soft s;
    const ParamInfo &m = paramInfo(p);
    s.title = nice(m.label);
    s.value = m.integer && p != P_SEMI2 ? paramText(p, lead.p[p]).toLower() : paramText(p, lead.p[p]);
    if (m.integer) {
        auto step = [this, p](int d) {
            const ParamInfo &mi = paramInfo(p);
            int span = (int)(mi.max - mi.min) + 1;
            int v = (int)lead.p[p] - (int)mi.min + d;
            v = p == P_SEMI2 ? qBound(0, v, span - 1) : ((v % span) + span) % span;
            setParam(p, (float)(v + (int)mi.min));
        };
        s.turn = [this, step](float d) {
            stepAcc += d / 4.0f;
            while (stepAcc >= 1) { step(1); stepAcc -= 1; }
            while (stepAcc <= -1) { step(-1); stepAcc += 1; }
        };
        s.press = [step] { step(1); };
    } else {
        s.turn = [this, p](float d) { setParam(p, normToParam(p, paramToNorm(p, lead.p[p]) + d * 0.01f)); };
    }
    return s;
}

MainWindow::Soft MainWindow::settingSoft(const QString &title, const QString &value, std::function<void(int)> step, float perStep) {
    Soft s;
    s.title = title;
    s.value = value;
    s.turn = [this, step, perStep](float d) {
        stepAcc += d / perStep;
        while (stepAcc >= 1) { step(1); stepAcc -= 1; }
        while (stepAcc <= -1) { step(-1); stepAcc += 1; }
    };
    s.press = [step] { step(1); };
    return s;
}

void MainWindow::buildMenu() {
    for (Soft &s : soft) s = Soft();
    auto key = [this](int i, const QString &title, const QString &value, std::function<void()> press, bool lit = false) {
        soft[i].title = title; soft[i].value = value; soft[i].press = std::move(press); soft[i].lit = lit;
    };
    const Tuning &t = u.set.tuning;

    // ---- full-screen views bring their own keys ----
    if (vw == V_HELP) {
        key(0, "Previous", "", [this] { helpPage = (helpPage + 2) % 3; });
        key(1, "Next", QString("%1 of 3").arg(helpPage + 1), [this] { helpPage = (helpPage + 1) % 3; });
        key(5, "Close", "", [this] { setView(V_SCOPE); });
        return;
    }
    if (vw == V_STATUS) { key(5, "Close", "", [this] { setView(V_SCOPE); }); return; }
    if (vw == V_KEYTEST) {
        key(0, "Reset", "count", [this] { maxHeld = 0; keyLog.clear(); });
        key(5, "Close", "", [this] { setView(V_SCOPE); });
        return;
    }
    if (vw == V_LISTEN) {
        int ls = listenState;
        key(0, "Listen", ls == 1 ? QString("%1 s").arg(qMax<qint64>(0, 6 - (clock.elapsed() - listenStarted) / 1000)) : QString("6 s"), [this] { startListen(); }, ls == 1);
        if (ls == 2) {
            key(1, "Use key", "", [this] { applyListen(true, false); toast("Key applied"); });
            key(2, "Use tempo", "", [this] { applyListen(false, true); toast("Tempo applied"); });
            key(3, "Use both", "", [this] { applyListen(true, true); toast("Key and tempo applied"); });
            key(4, "Other key", QString("%1 of 3").arg(listenGuess % 3 + 1), [this] { listenGuess = (listenGuess + 1) % 3; });
        }
        key(5, "Close", "", [this] { setView(V_SCOPE); });
        return;
    }

    switch (mnu) {
    case M_SOUND: {
        key(0, "New sound", "random", [this] { randomise(); });
        key(1, "Mutate", "nudge", [this] { mutate(); });
        key(2, "Undo", QString("%1 back").arg(undo.size()), [this] { undoSound(); });
        key(3, "Save fav", QString("%1 kept").arg(favs.size()), [this] { saveFavourite(); });
        favIdx = favs.isEmpty() ? 0 : qBound(0, favIdx, (int)favs.size() - 1);
        soft[4] = settingSoft("Recall", favs.isEmpty() ? QString("none") : QString(favs[favIdx].patch.name).left(9),
                              [this](int d) { if (!favs.isEmpty()) favIdx = (favIdx + d + favs.size()) % favs.size(); }, 3);
        soft[4].press = [this] {
            if (favs.isEmpty()) { toast("No favourites saved yet"); return; }
            pushUndo(); lead = favs[favIdx].patch; applyLead(true);
            toast(QString("Recalled %1").arg(lead.name), 1200);
        };
        key(5, "Store", slotSave ? "press 1-0" : "to slot", [this] { slotSave = !slotSave; }, slotSave);
        break;
    }
    case M_EDIT: {
        static const int pages[8][5] = {
            {P_WAVE1, P_PW, P_UNISON, P_SPREAD, P_FM},
            {P_WAVE2, P_MIX2, P_SEMI2, P_DETUNE, P_SUB},
            {P_ATK, P_DEC, P_SUS, P_REL, P_LEVEL},
            {P_FMODE, P_CUTOFF, P_RESO, P_FENV, P_KTRK},
            {P_FATK, P_FDEC, P_FSUS, P_FREL, P_NOISE},
            {P_VIBRATE, P_VIBDEPTH, P_VIBDELAY, P_LFORATE, P_LFOCUT},
            {P_LFOAMP, P_LFOPW, P_GLIDE, P_DRIVE, P_CHORUS},
            {P_DLYMIX, P_DLYDIV, P_DLYFB, P_REVMIX, P_REVSIZE},
        };
        static const char *names[8] = {"osc A", "osc B", "amp", "filter", "filt env", "vibrato", "motion", "space"};
        int pg = menuPage[M_EDIT] = ((menuPage[M_EDIT] % 8) + 8) % 8;
        for (int i = 0; i < 5; ++i) soft[i] = paramSoft(pages[pg][i]);
        key(5, QString("More %1/8").arg(pg + 1), names[pg], [this] { menuPage[M_EDIT] = (menuPage[M_EDIT] + 1) % 8; selSoft = -1; stepAcc = 0; });
        break;
    }
    case M_KEY: {
        soft[0] = settingSoft("Root", noteName(t.root), [this](int d) { set(S_ROOT, (float)((u.set.tuning.root + d + 12) % 12)); u.set.tuning.root = (u.set.tuning.root + d + 12) % 12; });
        soft[1] = settingSoft("Scale", QString(scaleDef(t.scale).shortName).toLower(), [this](int d) { int v = (u.set.tuning.scale + d + SC_COUNT) % SC_COUNT; set(S_SCALE, (float)v); u.set.tuning.scale = v; }, 6);
        soft[2] = settingSoft("Octave", QString("%1%2").arg(t.octave > 0 ? "+" : "").arg(t.octave), [this](int d) { inst.cmd(EV_OCTAVE, d); u.set.tuning.octave += d; }, 8);
        soft[2].press = nullptr;
        soft[3] = settingSoft("Str gap", QString("%1 st").arg(t.rowInterval), [this](int d) { int v = u.set.tuning.rowInterval + d; set(S_ROWINT, (float)v); u.set.tuning.rowInterval = qBound(1, v, 12); }, 6);
        soft[3].press = nullptr;
        soft[4] = settingSoft("Bend", QString("%1 st").arg((int)u.set.bendRange), [this](int d) { float v = qBound(1.0f, u.set.bendRange + d, 12.0f); set(S_BENDRANGE, v); u.set.bendRange = v; }, 6);
        soft[4].press = nullptr;
        soft[5] = settingSoft("Glide", u.set.glideScale < 0.02f ? QString("instant") : QString("x%1").arg(u.set.glideScale, 0, 'f', 2),
                              [this](int d) { float v = qBound(0.0f, u.set.glideScale + d * 0.05f, 4.0f); set(S_GLIDESCALE, v); u.set.glideScale = v; }, 1);
        soft[5].press = nullptr;
        break;
    }
    case M_CHORD: {
        int pg = menuPage[M_CHORD] = menuPage[M_CHORD] & 1;
        if (pg == 0) {
            static const char *MN[BM_COUNT] = {"sequence", "hold", "drone"};
            key(0, "Taps", MN[u.set.backMode], [this] { perform(A_BACK_MODE); });
            key(1, "Style", QString(backStyleName(u.set.backStyle)).toLower(), [this] { perform(A_BACK_STYLE); });
            key(2, "7ths", onOff(u.set.seventh), [this] { set(S_SEVENTH, u.set.seventh ? 0 : 1); }, u.set.seventh);
            key(3, "Remove", "last", [this] { inst.cmd(EV_BACK, BC_REMOVE_LAST); });
            key(4, "Clear", "all", [this] { perform(A_BACK_CLEAR); });
        } else {
            soft[0] = settingSoft("Beats", QString("%1/chord").arg(u.set.chordBeats), [this](int d) { int v = u.set.chordBeats + d; set(S_CHORDBEATS, (float)v); u.set.chordBeats = qBound(1, v, 16); }, 6);
            soft[0].press = nullptr;
            soft[1] = settingSoft("Arp rate", QString("%1/beat").arg(u.set.arpRate), [this](int d) { int v = u.set.arpRate + d; set(S_ARPRATE, (float)v); u.set.arpRate = qBound(1, v, 8); }, 6);
            soft[1].press = nullptr;
            soft[2] = settingSoft("Volume", QString("%1%").arg((int)std::round(u.set.backVol * 100)), [this](int d) { float v = qBound(0.0f, u.set.backVol + d * 0.02f, 1.0f); set(S_BACKVOL, v); u.set.backVol = v; }, 1);
            soft[2].press = nullptr;
            soft[3] = settingSoft("Slot", u.selSlot >= 0 ? QString("%1 next").arg(u.selSlot + 1) : QString("append"), [this](int d) {
                if (u.progLen <= 0) return;
                int v = u.selSlot + d;
                if (v >= u.progLen) v = 0;
                if (v < 0) v = u.progLen - 1;
                inst.cmd(EV_BACK, BC_SELECT_SLOT, v);
                u.selSlot = v;
            }, 6);
            key(4, "Sound", "use lead", [this] { inst.cmd(EV_BACK, BC_RESYNC_SOUND); toast("Backing now plays the lead sound"); });
        }
        key(5, QString("More %1/2").arg(pg + 1), pg ? "setup" : "chords", [this] { menuPage[M_CHORD] ^= 1; selSoft = -1; stepAcc = 0; });
        break;
    }
    case M_TIME: {
        soft[0] = settingSoft("Tempo", QString("%1 bpm").arg((int)std::round(u.set.bpm)), [this](int d) { float v = qBound(30.0f, std::round(u.set.bpm) + d, 300.0f); set(S_BPM, v); u.set.bpm = v; }, 1);
        soft[0].press = nullptr;
        key(1, "Tap", "tempo", [this] { tapTempo(); });
        key(2, "Click", onOff(u.set.metro), [this] { perform(A_METRO); }, u.set.metro);
        soft[3] = settingSoft("Loop vol", QString("%1%").arg((int)std::round(u.set.loopVol * 100)), [this](int d) { float v = qBound(0.0f, u.set.loopVol + d * 0.02f, 1.0f); set(S_LOOPVOL, v); u.set.loopVol = v; }, 1);
        soft[3].press = nullptr;
        soft[4] = settingSoft("Loop fix", QString("%1 ms").arg((int)u.set.latencyMs), [this](int d) { float v = qBound(0.0f, u.set.latencyMs + d * 10.0f, 500.0f); set(S_LATENCY, v); u.set.latencyMs = v; }, 3);
        soft[4].press = nullptr;
        key(5, "Clr loop", "erase", [this] { perform(A_LOOP_CLEAR); });
        break;
    }
    case M_FILE: {
        key(0, "Save", "session", [this] {
            QString path = store::baseDir() + "/sessions/" + store::timestampName("session", "json");
            toast(saveSession(path) ? QString("Saved %1").arg(QFileInfo(path).fileName()) : QString("Could not save the session"), 2200);
        });
        sessIdx = sessions.isEmpty() ? 0 : qBound(0, sessIdx, (int)sessions.size() - 1);
        soft[1] = settingSoft("Load", sessions.isEmpty() ? QString("none") : QString("%1 of %2").arg(sessIdx + 1).arg(sessions.size()),
                              [this](int d) { if (!sessions.isEmpty()) sessIdx = (sessIdx + d + sessions.size()) % sessions.size(); }, 3);
        soft[1].press = [this] {
            if (sessions.isEmpty()) { toast("No sessions saved yet"); return; }
            QString path = sessions[sessIdx];
            toast(loadSession(path) ? QString("Loaded %1").arg(QFileInfo(path).completeBaseName()) : QString("Could not read that session"), 2400);
        };
        key(2, "Rec WAV", wav.isOpen() ? QString("%1 s").arg(wav.frames() / audio.sampleRate()) : QString("start"), [this] { toggleRecord(); }, wav.isOpen());
        delIdx = favs.isEmpty() ? 0 : qBound(0, delIdx, (int)favs.size() - 1);
        soft[3] = settingSoft("Del fav", favs.isEmpty() ? QString("none") : QString(favs[delIdx].patch.name).left(9),
                              [this](int d) { if (!favs.isEmpty()) delIdx = (delIdx + d + favs.size()) % favs.size(); }, 3);
        soft[3].press = [this] {
            if (favs.isEmpty()) return;
            QFile::remove(favs[delIdx].file);
            refreshLists();
            toast("Favourite removed");
        };
        key(4, "Factory", "sounds", [this] {
            for (int k = 0; k < 10; ++k) bank[k] = factoryPatch(k);
            toast("Sound slots 1-0 restored");
        });
        break;
    }
    case M_UTIL: {
        bool armed = clock.elapsed() < quitArmedUntil;
        key(0, "Status", "audio", [this] { setView(V_STATUS); });
        key(1, "Key test", "rollover", [this] { maxHeld = 0; setView(V_KEYTEST); });
        key(2, "Listen", "key+bpm", [this] { setView(V_LISTEN); });
        key(3, "Help", "guide", [this] { setView(V_HELP); });
        key(4, "Panic", "all off", [this] { perform(A_PANIC); });
        key(5, "Quit", armed ? "again!" : "", [this] {
            if (clock.elapsed() < quitArmedUntil) close();
            else { quitArmedUntil = clock.elapsed() + 3000; toast("Press Quit again to switch off", 2800); }
        }, armed);
        break;
    }
    }
    if (selSoft >= 0 && !soft[selSoft].turn) selSoft = -1;
}

void MainWindow::autoScale() {
    // fit about three and a half cycles of the lowest sounding note, and the trace to six divisions
    float f = measFreq;
    for (int r = 0; r < kStrings; ++r)
        if (u.str[r].gate) { float hz = midiToHz(u.str[r].note + u.bend); if (f <= 0 || hz < f) f = hz; }
    if (f <= 0) { toast("Auto-scale: play a note first"); return; }
    float want = 3.5f * 1000.0f / f / 10.0f; // ms per division
    int best = 0;
    for (int i = 1; i < TB_COUNT; ++i)
        if (std::fabs(std::log(TB_MS[i] / want)) < std::fabs(std::log(TB_MS[best] / want))) best = i;
    tbIdx = best;
    float pk = qMax(0.01f, measPk * 0.5f);
    vdIdx = VD_COUNT - 1;
    for (int i = 0; i < VD_COUNT; ++i)
        if (pk <= VD_V[i] * 3.2f) { vdIdx = i; break; }
    toast(QString("Auto-scale  %1  %2").arg(vdText(vdIdx)).arg(tbText(tbIdx)), 1400);
}

// ------------------------------------------------------------- tube

void MainWindow::drawCrt() {
    // touch: the soft-key labels are bigger targets than the keys under them
    for (int i = 0; i < 6; ++i)
        zone(QRectF(SOFTS.x() + i * SOFT_W, SOFTS.y() - 2, SOFT_W, SOFTS.height() + 6), QString("softlbl%1").arg(i), [this, i](QPointF, int ph) { if (ph == PH_PRESS) pressSoft(i); });
    if (vw == V_SCOPE)
        zone(GRAT, "xy", [this](QPointF p, int ph) {
            Event e; e.type = EV_XY; e.a = ph != PH_RELEASE;
            e.f = (float)qBound(0.0, (p.x() - GRAT.x()) / GRAT.width(), 1.0);
            e.g = (float)qBound(0.0, (GRAT.bottom() - p.y()) / GRAT.height(), 1.0);
            inst.push(e);
        });
    if (!vis(TUBE)) return;

    g->save();
    g->setClipRect(GLASS);
    g->setRenderHint(QPainter::Antialiasing, false);
    g->fillRect(GLASS, hw::CRT_BG);

    if (vw == V_SCOPE) drawScope();
    else drawTextView();

    // ---- status line ----
    const Tuning &t = u.set.tuning;
    g->fillRect(BAND, hw::CRT_BAND);
    qreal by = BAND.y(), bh = BAND.height();
    g->fillRect(QRectF(BAND.x() + 4, by + 2, 12, bh - 4), hw::CRT_DARK);
    crtText(QRectF(BAND.x() + 4, by, 12, bh), "1", 13, hw::CRT_BAND, Qt::AlignCenter, true);
    crtText(QRectF(BAND.x() + 20, by, 70, bh), vdText(vdIdx), 13, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, true);
    crtText(QRectF(BAND.x() + 92, by, 70, bh), tbText(tbIdx), 13, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, true);
    QString trg = !audio.streaming() ? "Stop" : (trigd ? "Trig'd" : "Auto");
    crtText(QRectF(BAND.x() + 166, by, 60, bh), trg, 13, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, true);
    crtText(QRectF(BAND.x() + 226, by, 110, bh), QString("%1 %2").arg(noteName(t.root)).arg(QString(scaleDef(t.scale).shortName).toLower()), 13, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, true);
    if (u.limiter < 0.85f) {
        g->fillRect(QRectF(BAND.right() - 48, by + 2, 44, bh - 4), hw::CRT_DARK);
        crtText(QRectF(BAND.right() - 48, by, 44, bh), "LIMIT", 13, hw::CRT_BAND, Qt::AlignCenter, true);
    } else {
        crtText(QRectF(BAND.right() - 70, by, 66, bh), QString("%1bpm").arg((int)std::round(u.set.bpm)), 13, hw::CRT_DARK, Qt::AlignRight | Qt::AlignVCenter, true);
    }

    // ---- measurement lines ----
    qreal my = MEAS.y();
    if (vw == V_SCOPE) {
        QString fq = measFreq > 0 ? (measFreq >= 1000 ? QString("%1kHz").arg(measFreq / 1000, 0, 'f', 3) : QString("%1Hz").arg(measFreq, 0, 'f', 1)) : QString("----");
        QString pk = measPk >= 1 ? QString("%1V").arg(measPk, 0, 'f', 2) : QString("%1mV").arg((int)std::round(measPk * 1000));
        crtText(QRectF(MEAS.x() + 4, my, 140, 15), QString("Freq(1): %1").arg(fq), 13, hw::CRT_P);
        crtText(QRectF(MEAS.x() + 152, my, 140, 15), QString("Pk-Pk(1): %1").arg(pk), 13, hw::CRT_P);
        QString nt = "--";
        for (int r = kStrings - 1; r >= 0; --r)
            if (u.str[r].gate) { nt = noteText(u.str[r].note + u.bend); break; }
        crtText(QRectF(MEAS.x() + 296, my, 106, 15), QString("Note: %1").arg(nt), 13, hw::CRT_P, Qt::AlignRight | Qt::AlignVCenter);
    }
    QString snd = curSlot >= 0 ? QString("Sound %1: %2").arg((curSlot + 1) % 10).arg(lead.name) : QString("Sound *: %1").arg(lead.name);
    crtText(QRectF(MEAS.x() + 4, my + 15, 250, 15), snd.left(30), 13, hw::CRT_P);
    QString right = QString("Oct%1%2 %3v").arg(t.octave >= 0 ? "+" : "").arg(t.octave).arg(u.voices);
    crtText(QRectF(MEAS.x() + 256, my + 15, 146, 15), right, 13, hw::CRT_DIM, Qt::AlignRight | Qt::AlignVCenter);

    // ---- soft-key labels ----
    for (int i = 0; i < 6; ++i) {
        const Soft &k = soft[i];
        if (k.title.isEmpty()) continue;
        QRectF r(SOFTS.x() + i * SOFT_W + 1, SOFTS.y() + 1, SOFT_W - 2, SOFTS.height() - 2);
        bool sel = i == selSoft, dn = downNames.contains(QString("soft%1").arg(i)) || downNames.contains(QString("softlbl%1").arg(i));
        QColor fill = dn ? hw::CRT_P : (sel || k.lit ? QColor(0x79, 0xf2, 0xae) : QColor(0x43, 0xb0, 0x7b));
        g->fillRect(r, fill);
        // raised box, as the reference draws its keys
        g->fillRect(QRectF(r.x(), r.y(), r.width(), 1), QColor(0xb9, 0xff, 0xd6));
        g->fillRect(QRectF(r.x(), r.y(), 1, r.height()), QColor(0xb9, 0xff, 0xd6));
        g->fillRect(QRectF(r.x(), r.bottom() - 1, r.width(), 1), QColor(0x1f, 0x6b, 0x47));
        g->fillRect(QRectF(r.right() - 1, r.y(), 1, r.height()), QColor(0x1f, 0x6b, 0x47));
        crtText(QRectF(r.x() + 3, r.y() + 2, r.width() - 4, 14), k.title, 12, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, true);
        crtText(QRectF(r.x() + 3, r.y() + 16, r.width() - 4, 14), k.value, 12, hw::CRT_DARK, Qt::AlignLeft | Qt::AlignVCenter, false);
        if (sel) { // this key owns the entry knob
            g->setRenderHint(QPainter::Antialiasing, true);
            g->setBrush(Qt::NoBrush);
            g->setPen(QPen(hw::CRT_DARK, 1.4));
            QPointF c(r.right() - 8, r.y() + 9);
            g->drawArc(QRectF(c.x() - 4, c.y() - 4, 8, 8), 110 * 16, -290 * 16);
            g->setPen(Qt::NoPen);
            g->setBrush(hw::CRT_DARK);
            QPainterPath h;
            h.moveTo(c + QPointF(1.5, -4.2)); h.lineTo(c + QPointF(-2.6, -6.2)); h.lineTo(c + QPointF(-2.4, -1.6)); h.closeSubpath();
            g->drawPath(h);
            g->setRenderHint(QPainter::Antialiasing, false);
        }
    }

    // ---- glass ----
    if (glassPix.isNull()) glassPix = hw::glass((int)GLASS.width(), (int)GLASS.height(), 8);
    g->drawPixmap(GLASS.topLeft(), glassPix);
    g->restore();
}

void MainWindow::drawScope() {
    const int GW = (int)GRAT.width(), GH = (int)GRAT.height();
    const qreal divX = GW / 10.0, divY = GH / 8.0;
    // ---- graticule (drawn once) ----
    if (gratPix.isNull()) {
        gratPix = QPixmap(GW + 1, GH + 1);
        gratPix.fill(Qt::transparent);
        QPainter q(&gratPix);
        q.setPen(hw::CRT_GRAT);
        for (int i = 0; i <= 10; ++i)
            for (qreal y = 0; y <= GH; y += divY / 5) q.drawPoint(QPointF(i * divX, y));
        for (int j = 0; j <= 8; ++j)
            for (qreal x = 0; x <= GW; x += divX / 5) q.drawPoint(QPointF(x, j * divY));
        // centre axes carry the fine ticks
        q.setPen(QColor(0x2b, 0x80, 0x58));
        for (qreal x = 0; x <= GW; x += divX / 5) q.drawLine(QPointF(x, GH / 2.0 - 2), QPointF(x, GH / 2.0 + 2));
        for (qreal y = 0; y <= GH; y += divY / 5) q.drawLine(QPointF(GW / 2.0 - 2, y), QPointF(GW / 2.0 + 2, y));
        q.drawRect(0, 0, GW, GH);
    }
    g->drawPixmap(GRAT.topLeft(), gratPix);

    // ---- acquire: newest samples, triggered on a rising zero crossing at centre screen ----
    static float buf[kScopeSize];
    const float sr = (float)audio.sampleRate();
    int need = qBound(64, (int)(TB_MS[tbIdx] * 10.0f * sr / 1000.0f), kScopeSize - 2048);
    int total = qMin((int)kScopeSize, need + 6000);
    inst.readScope(buf, total);
    float lo = 1e9f, hi = -1e9f;
    for (int i = total - need; i < total; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    float span = hi - lo;
    // ---- frequency: autocorrelation of the last 85 ms (decimated by four). Counting zero
    // crossings misreads any wave with more than one wiggle per cycle. ----
    measFreq = 0;
    float period = 0; // samples
    if (span > 0.004f) {
        static float d[1024];
        const float *x = buf + total - 4096;
        float mean = 0;
        for (int i = 0; i < 1024; ++i) { d[i] = (x[4 * i] + x[4 * i + 1] + x[4 * i + 2] + x[4 * i + 3]) * 0.25f; mean += d[i]; }
        mean /= 1024;
        for (int i = 0; i < 1024; ++i) d[i] -= mean;
        static float ac[402];
        float r0 = 1e-12f;
        for (int i = 0; i < 1024; ++i) r0 += d[i] * d[i];
        float best = 0;
        for (int lag = 5; lag <= 401; ++lag) {
            float a = 0;
            const int n = 1024 - lag;
            for (int i = 0; i < n; ++i) a += d[i] * d[i + lag];
            ac[lag] = a * 1024.0f / (n * r0); // unbiased, normalised
            if (lag >= 6 && lag <= 400 && ac[lag] > best) best = ac[lag];
        }
        if (best > 0.3f) {
            for (int lag = 6; lag <= 400; ++lag) // the shortest lag that is nearly as good: the true period, not a multiple
                if (ac[lag] >= 0.92f * best && ac[lag] >= ac[lag - 1] && ac[lag] >= ac[lag + 1]) {
                    float den = ac[lag - 1] - 2 * ac[lag] + ac[lag + 1];
                    float frac = std::fabs(den) > 1e-9f ? 0.5f * (ac[lag - 1] - ac[lag + 1]) / den : 0;
                    period = (lag + qBound(-0.5f, frac, 0.5f)) * 4.0f;
                    measFreq = sr / period;
                    break;
                }
        }
    }
    // ---- trigger: rising zero crossing after a deep negative swing, at most once per cycle ----
    float hyst = qMax(0.003f, -lo * 0.55f);
    int trig = -1, lastX = -100000;
    bool armed = false;
    for (int i = 1; i < total; ++i) {
        if (buf[i] < -hyst) armed = true;
        else if (armed && buf[i - 1] < 0 && buf[i] >= 0) {
            armed = false;
            if (i - lastX < period * 0.8f) continue; // hold-off
            lastX = i;
            if (i >= need / 2 && i <= total - need / 2) trig = i;
        }
    }
    trigd = trig >= 0 && span > 0.004f;
    int start = trigd ? trig - need / 2 : total - need;
    lo = 1e9f; hi = -1e9f;
    for (int i = start; i < start + need; ++i) { lo = qMin(lo, buf[i]); hi = qMax(hi, buf[i]); }
    measPk = qMax(0.0f, hi - lo);

    // ---- phosphor: the old trace fades, the new one is written on top ----
    if (phos.size() != QSize(GW, GH)) { phos = QImage(GW, GH, QImage::Format_RGB32); phos.fill(Qt::black); }
    {
        QPainter q(&phos);
        q.fillRect(phos.rect(), QColor(0, 0, 0, 105));
        q.setRenderHint(QPainter::Antialiasing, true);
        const float vdiv = VD_V[vdIdx];
        auto Y = [&](float v) { return GH / 2.0 - qBound(-4.2f, v / vdiv, 4.2f) * divY; };
        QPainterPath path;
        double per = (double)need / GW; // samples per pixel column
        if (per <= 1.5) {
            for (int x = 0; x <= GW; ++x) {
                double pos = start + x * per;
                int i0 = qBound(0, (int)pos, total - 2);
                float fr = (float)(pos - i0);
                float v = buf[i0] + (buf[i0 + 1] - buf[i0]) * fr;
                if (x == 0) path.moveTo(x, Y(v)); else path.lineTo(x, Y(v));
            }
        } else { // peak detect: each column spans from its lowest to its highest sample
            for (int x = 0; x < GW; ++x) {
                int a = start + (int)(x * per), b = qMin(total, start + (int)((x + 1) * per) + 1);
                float mn = buf[a], mx = buf[a];
                for (int i = a + 1; i < b; ++i) { mn = qMin(mn, buf[i]); mx = qMax(mx, buf[i]); }
                bool up = buf[b - 1] >= buf[a];
                if (x == 0) path.moveTo(x, Y(up ? mn : mx));
                path.lineTo(x, Y(up ? mn : mx));
                path.lineTo(x, Y(up ? mx : mn));
            }
        }
        q.setBrush(Qt::NoBrush);
        q.setPen(QPen(QColor(0x30, 0xc8, 0x78, 80), 6.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        q.drawPath(path);
        q.setPen(QPen(QColor(0xa4, 0xff, 0xcb), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        q.drawPath(path);
    }
    g->setCompositionMode(QPainter::CompositionMode_Plus);
    g->drawImage(GRAT.topLeft(), phos);
    g->setCompositionMode(QPainter::CompositionMode_SourceOver);

    // ---- markers ----
    g->setRenderHint(QPainter::Antialiasing, true);
    g->setPen(Qt::NoPen);
    g->setBrush(hw::CRT_P);
    QPainterPath tm; // trigger point, top centre
    tm.moveTo(GRAT.center().x() - 4, GRAT.y() + 1); tm.lineTo(GRAT.center().x() + 4, GRAT.y() + 1); tm.lineTo(GRAT.center().x(), GRAT.y() + 7); tm.closeSubpath();
    g->drawPath(tm);
    QPainterPath gm; // channel 1 ground, left edge
    gm.moveTo(GRAT.x() + 1, GRAT.center().y() - 5); gm.lineTo(GRAT.x() + 9, GRAT.center().y() - 5); gm.lineTo(GRAT.x() + 13, GRAT.center().y());
    gm.lineTo(GRAT.x() + 9, GRAT.center().y() + 5); gm.lineTo(GRAT.x() + 1, GRAT.center().y() + 5); gm.closeSubpath();
    g->drawPath(gm);
    g->setRenderHint(QPainter::Antialiasing, false);
    crtText(QRectF(GRAT.x() + 1, GRAT.center().y() - 6, 9, 12), "1", 10, hw::CRT_DARK, Qt::AlignCenter, true);

    // ---- pitch lanes: each string's pitch over the last two and a half seconds ----
    const Tuning &t = u.set.tuning;
    qint64 now = clock.elapsed();
    Hist hnow; hnow.t = now;
    for (int r = 0; r < kStrings; ++r) { hnow.note[r] = u.str[r].note + u.bend; hnow.gate[r] = u.str[r].gate; }
    hist.push_back(hnow);
    while (!hist.empty() && now - hist.front().t > 2600) hist.pop_front();
    const qreal laneH = 13, laneGap = 3, lanesBottom = GRAT.bottom() - 5;
    g->setRenderHint(QPainter::Antialiasing, true);
    for (int r = 0; r < kStrings; ++r) {
        qreal yb = lanesBottom - r * (laneH + laneGap);
        int cnt = km.rows[r].size();
        float nlo = t.noteFor(r, 0, false) - 1.5f, nhi = t.noteFor(r, cnt - 1, false) + 1.5f;
        auto LY = [&](float n) { return yb - qBound(0.0f, (n - nlo) / (nhi - nlo), 1.0f) * laneH; };
        auto LX = [&](qint64 tm_) { return GRAT.right() - (now - tm_) * (GRAT.width() - 26) / 2500.0; };
        g->setRenderHint(QPainter::Antialiasing, false);
        crtText(QRectF(GRAT.x() + 2, yb - laneH, 22, laneH), QString("S%1").arg(r + 1), 10, hist.back().gate[r] ? hw::CRT_P : hw::CRT_DIM);
        g->setPen(QColor(0x1c, 0x4e, 0x36));
        g->drawLine(QPointF(GRAT.x() + 24, yb), QPointF(GRAT.right(), yb));
        g->setRenderHint(QPainter::Antialiasing, true);
        g->setPen(QPen(QColor(0x6f, 0xe8, 0xa4), 1.4));
        for (size_t i = 1; i < hist.size(); ++i) {
            const Hist &a = hist[i - 1], &b = hist[i];
            if (!a.gate[r] || !b.gate[r]) continue;
            qreal xa = LX(a.t), xb = LX(b.t);
            if (xa < GRAT.x() + 24) continue;
            g->drawLine(QPointF(xa, LY(a.note[r])), QPointF(xb, LY(b.note[r])));
        }
    }

    // ---- X-Y cursors (touch the trace area) ----
    bool xyActive = u.xyTouch || u.set.xyHold;
    g->setRenderHint(QPainter::Antialiasing, false);
    if (xyActive) {
        QPointF c(GRAT.x() + GRAT.width() * u.xyX, GRAT.bottom() - GRAT.height() * u.xyY);
        g->setPen(QPen(hw::CRT_P, 1, Qt::DashLine));
        g->drawLine(QPointF(c.x(), GRAT.y()), QPointF(c.x(), GRAT.bottom()));
        g->drawLine(QPointF(GRAT.x(), c.y()), QPointF(GRAT.right(), c.y()));
        g->setPen(QPen(hw::CRT_P, 1));
        g->setBrush(Qt::NoBrush);
        g->drawRect(QRectF(c.x() - 5, c.y() - 5, 10, 10));
        crtText(QRectF(GRAT.x() + 18, GRAT.y() + 44, 300, 12),
                QString("X %1 %2%  Y %3 %4%").arg(QString(xyAxisName(u.set.xyMap, false)).toLower()).arg((int)(u.xyX * 100)).arg(QString(xyAxisName(u.set.xyMap, true)).toLower()).arg((int)(u.xyY * 100)),
                11, hw::CRT_P);
    }

    // ---- what is running: backing and loop ----
    qreal oy = GRAT.y() + 10;
    if (u.progLen > 0 || u.holdChord >= 0 || u.droneMask) {
        qreal x = GRAT.x() + 18;
        crtText(QRectF(x, oy, 40, 13), u.backPlaying ? "Back" : "Back-", 11, hw::CRT_DIM);
        x += 40;
        if (u.progLen > 0) {
            for (int i = 0; i < u.progLen; ++i) {
                QString nm = buildChord(t, u.prog[i], u.set.seventh).name;
                qreal w = nm.size() * 7 + 6;
                bool cur = u.backPlaying && i == u.progPos, sel = i == u.selSlot;
                if (cur) crtInverse(QRectF(x, oy, w, 13), nm, 11);
                else crtText(QRectF(x + 3, oy, w, 13), nm, 11, sel ? hw::CRT_P : hw::CRT_DIM);
                if (sel) g->fillRect(QRectF(x, oy + 13, w, 1), hw::CRT_P);
                x += w + 2;
            }
        } else if (u.holdChord >= 0) {
            crtInverse(QRectF(x, oy, 60, 13), QString("hold %1").arg(buildChord(t, u.holdChord, u.set.seventh).name), 11);
        } else {
            QString dn;
            for (int d = 0; d < 7; ++d) if ((u.droneMask >> d) & 1) dn += QString(buildChord(t, d, false).name).left(2).remove('m') + " ";
            crtText(QRectF(x, oy, 200, 13), "drone " + dn, 11, hw::CRT_P);
        }
        oy += 15;
    }
    if (u.loopState != L_EMPTY) {
        QString ls;
        bool hot = false;
        switch (u.loopState) {
        case L_COUNTIN: ls = QString("count %1").arg(qMax(1, u.countBeat)); hot = true; break;
        case L_REC: ls = QString("REC bar %1").arg((int)u.loopPos + 1); hot = true; break;
        case L_OVERDUB: ls = u.loopFull ? QString("DUB full") : QString("DUB"); hot = true; break;
        case L_STOPPED: ls = "stopped"; break;
        default: ls = u.loopClosing ? QString("REC closing") : QString("play"); hot = u.loopClosing; break;
        }
        qreal x = GRAT.x() + 18;
        crtText(QRectF(x, oy, 40, 13), "Loop", 11, hw::CRT_DIM);
        x += 40;
        qreal w = ls.size() * 7 + 6;
        if (hot) crtInverse(QRectF(x, oy, w, 13), ls, 11);
        else crtText(QRectF(x + 3, oy, w, 13), ls, 11, hw::CRT_P);
        x += w + 6;
        if (u.loopBars > 0) {
            crtText(QRectF(x, oy, 120, 13), QString("%1bar x%2").arg(u.loopBars).arg(u.layers), 11, hw::CRT_DIM);
            // position within the loop, like the reference's memory bar
            QRectF bar(x + 74, oy + 4, 90, 5);
            g->setPen(hw::CRT_DIM);
            g->setBrush(Qt::NoBrush);
            g->drawRect(bar);
            g->fillRect(QRectF(bar.x() + 1, bar.y() + 1, (bar.width() - 1) * qBound(0.0f, u.loopPos, 1.0f), bar.height() - 1), hw::CRT_P);
        }
        oy += 15;
    }
    if (wav.isOpen()) {
        if ((now / 400) % 2 == 0) crtInverse(QRectF(GRAT.right() - 86, GRAT.y() + 10, 80, 13), QString("REC %1s").arg(wav.frames() / audio.sampleRate()), 11);
    } else if (sinkIsDummy) {
        crtInverse(QRectF(GRAT.right() - 122, GRAT.y() + 10, 116, 13), "NO AUDIO DEVICE", 11);
    }

    drawLists();

    // ---- knob readout, messages ----
    if (now < readoutUntil) {
        QString s = QString("%1  %2").arg(readoutLabel, readoutValue);
        qreal w = s.size() * 8 + 12;
        QRectF r(GRAT.right() - w - 6, GRAT.y() + 28, w, 17);
        g->fillRect(r.adjusted(-1, -1, 1, 1), hw::CRT_BG);
        crtInverse(r, s, 13);
    }
    if (now < toastUntil && !toastText.isEmpty()) {
        QString s = toastText.left(46);
        qreal w = s.size() * 8 + 14;
        QRectF r(GRAT.center().x() - w / 2, GRAT.center().y() + 30, w, 19);
        g->fillRect(r.adjusted(-2, -2, 2, 2), hw::CRT_BG);
        g->setPen(hw::CRT_P);
        g->setBrush(Qt::NoBrush);
        g->drawRect(r.adjusted(-2, -2, 1, 1));
        crtInverse(r, s, 13);
    }
}

void MainWindow::drawLists() {
    // a soft key that picks from a list shows the list while it owns the entry knob
    QStringList items;
    QString title;
    int cur = 0;
    if (mnu == M_SOUND && selSoft == 4) { for (const auto &f : favs) items << f.patch.name; cur = favIdx; title = "Favourites: turn Entry, press Recall"; }
    else if (mnu == M_FILE && selSoft == 1) { for (const QString &s : sessions) items << QFileInfo(s).completeBaseName(); cur = sessIdx; title = "Sessions: turn Entry, press Load"; }
    else if (mnu == M_FILE && selSoft == 3) { for (const auto &f : favs) items << f.patch.name; cur = delIdx; title = "Delete which favourite?"; }
    else return;
    const int rows = 8;
    QRectF box(GRAT.x() + 30, GRAT.y() + 44, 320, 16 + rows * 15 + 4);
    g->fillRect(box, hw::CRT_BG);
    g->setPen(hw::CRT_P);
    g->setBrush(Qt::NoBrush);
    g->drawRect(box.adjusted(0, 0, -1, -1));
    crtInverse(QRectF(box.x() + 1, box.y() + 1, box.width() - 2, 15), title, 11, Qt::AlignLeft | Qt::AlignVCenter);
    if (items.isEmpty()) { crtText(QRectF(box.x() + 8, box.y() + 22, 300, 15), "(nothing saved yet)", 13, hw::CRT_DIM); return; }
    int top = qBound(0, cur - rows / 2, qMax(0, (int)items.size() - rows));
    for (int k = 0; k < rows && top + k < items.size(); ++k) {
        QRectF r(box.x() + 4, box.y() + 18 + k * 15, box.width() - 8, 15);
        QString s = QString("%1 %2").arg(top + k + 1, 2).arg(items[top + k]).left(38);
        if (top + k == cur) crtInverse(r, s, 13, Qt::AlignLeft | Qt::AlignVCenter);
        else crtText(r.adjusted(3, 0, 0, 0), s, 13, hw::CRT_P);
    }
}

void MainWindow::drawTextView() {
    QStringList L;
    QVector<int> hot; // lines drawn in inverse video
    auto K = [this](int act, bool cmd = false) { return km.keyFor(act, cmd); };
    QString title;
    if (vw == V_HELP) {
        QString cmdKey = km.keyFor(A_COMMAND, false);
        if (helpPage == 0) {
            title = "Playing";
            L << QString("%1..%2  high string     %3..%4  middle string").arg(km.rows[2].left(1), km.rows[2].right(1), km.rows[1].left(1), km.rows[1].right(1));
            L << QString("%1..%2  low string. Left to right goes up.").arg(km.rows[0].left(1), km.rows[0].right(1));
            L << "Hold a key and press another on the same row:";
            L << "  the note SLIDES. Let go and it slides back.";
            L << "Hold a shape across rows, re-finger: it glides.";
            L << QString("%1  semitones while held").arg(K(A_CHROMATIC), -10);
            L << QString("%1  sustain while held").arg(K(A_SUSTAIN), -10);
            L << QString("%1  all notes off").arg(K(A_PANIC), -10);
            L << QString("%1  loop: count in, record, play, dub").arg(K(A_LOOP), -10);
            L << "Left Right  pitch bend     Up Down  octave";
            L << "1..9 0      the ten sounds";
            L << QString("%1+%2..%3     backing chords I..VII").arg(K(A_CHORD_LAYER), km.rows[0].left(1), km.rows[0].right(1));
            L << QString("%1           bottom row plays chords").arg(K(A_CHORD_ROW));
            L << "Tab  next menu   Esc  back to Patch menu";
        } else if (helpPage == 1) {
            title = QString("Commands: hold %1 and press").arg(cmdKey);
            auto row = [&](int a, const char *ta, int b, const char *tb) {
                L << QString("%1 %2  %3 %4").arg(K(a, true), -3).arg(QString(ta), -20).arg(K(b, true), -3).arg(QString(tb));
            };
            row(A_RANDOM, "new sound", A_MUTATE, "mutate sound");
            row(A_UNDO_SOUND, "undo sound", A_FAVOURITE, "save favourite");
            row(A_KEY_UP, "key up a semitone", A_SCALE_NEXT, "next scale");
            row(A_TAP, "tap tempo", A_METRO, "metronome");
            row(A_BACK_MODE, "chord taps mode", A_BACK_STYLE, "backing style");
            row(A_BACK_PLAY, "backing start/stop", A_BACK_CLEAR, "clear backing");
            row(A_LOOP_UNDO, "undo loop layer", A_LOOP_PLAY, "loop start/stop");
            row(A_LOOP_CLEAR, "clear the loop", A_RECORD, "record a WAV");
            row(A_XYMAP, "next X-Y mapping", A_HELP, "this guide");
            L << QString("%1 remove last chord       %2 quit").arg(K(A_BACK_REMOVE, true), -9).arg(K(A_QUIT, true));
            L << "1..6  soft keys under the screen";
            L << "Left Right  turn selected knob (fine)";
            L << "Up Down     turn selected knob (coarse)";
            L << QString("with %1: key and scale step backwards").arg(K(A_CHROMATIC));
        } else {
            title = "Front panel";
            L << "Knobs: drag up or down to turn. Drag sideways";
            L << "  for fine adjustment. The dot marks the knob";
            L << "  the keyboard turns.";
            L << "Soft keys: tap a label or the key under it.";
            L << "  A key with a value takes the Entry knob;";
            L << "  tap it again to step or to carry it out.";
            L << "Touch the trace: X-Y sound control. The X-Y";
            L << "  key picks what the two axes do.";
            L << "Time/Div only changes the picture, never the";
            L << "  pitch. Auto-scale fits it to the note.";
            L << "S1..S3 traces: each string's pitch over time.";
            L << "Key deck: tap or drag along a row to slide.";
            L << "Number row: sounds. Store, then a number,";
            L << "  keeps the current sound in that slot.";
        }
    } else if (vw == V_STATUS) {
        title = "Status";
        L << QString("Audio   %1 %2Hz %3fr (%4ms)").arg(QString::fromStdString(audio.backend())).arg(audio.sampleRate()).arg(audio.quantum()).arg(1000.0 * audio.quantum() / audio.sampleRate(), 0, 'f', 1);
        L << QString("Output  %1%2").arg((sinkDescription.isEmpty() ? QString("unknown") : sinkDescription).left(30)).arg(audio.streaming() ? "" : " (idle)");
        if (sinkIsBluetooth) { L << "        Bluetooth: about 0.2 s behind you"; L << "        Use a wired output for tight timing."; }
        if (sinkIsDummy) { hot << L.size(); L << "No real audio device: connect a speaker."; }
        L << QString("Engine  load %1%  peak %2%  late %3").arg((int)(audio.load() * 100)).arg((int)(audio.peakLoad() * 100)).arg(audio.overruns());
        L << QString("Voices  %1 of %2    dropped events %3").arg(u.voices).arg(kVoices).arg(u.droppedEvents);
        L << QString("Loop    timing fix %1 ms").arg((int)u.set.latencyMs);
        L << QString("Files   %1").arg(store::baseDir()).left(50);
        L << QString("Last    %1").arg(lastRecording.isEmpty() ? QString("no recording yet") : QFileInfo(lastRecording).fileName());
        L << "Keys    edit keymap.json in that folder";
        L << QString("Saved   %1 favourites, %2 sessions").arg(favs.size()).arg(sessions.size());
    } else if (vw == V_KEYTEST) {
        title = "Key test";
        QStringList names;
        for (auto it = held.begin(); it != held.end(); ++it) names << keyName(it.key());
        names.sort();
        L << QString("Held now (%1): %2").arg(names.size()).arg(names.join(" ")).left(50);
        hot << L.size();
        L << QString("Most keys held at once: %1").arg(maxHeld);
        L << "Hold one key on each row, then add more. A key";
        L << "you did not press appearing here, or a pressed";
        L << "key missing, is a keyboard-matrix limit.";
        for (int i = 0; i < keyLog.size() && i < 9; ++i) L << keyLog[i];
    } else if (vw == V_LISTEN) {
        title = "Listen: match the key and tempo around you";
        int ls = listenState;
        if (ls == 0) {
            L << "Press Listen. Tendril hears six seconds from";
            L << "the default audio input and estimates the key";
            L << "and tempo. It needs a microphone or line input.";
        } else if (ls == 1) {
            hot << L.size();
            L << QString("Listening... %1 s").arg(qMax<qint64>(0, 6 - (clock.elapsed() - listenStarted) / 1000));
        } else if (ls == 3) {
            std::lock_guard<std::mutex> lk(listenMx);
            QString e = listenError;
            while (!e.isEmpty()) { int cut = e.size() > 48 ? e.lastIndexOf(' ', 48) : -1; if (cut <= 0) cut = qMin(48, (int)e.size()); L << e.left(cut); e = e.mid(cut).trimmed(); }
        } else {
            std::lock_guard<std::mutex> lk(listenMx);
            const ListenResult &r = listenResult;
            const KeyGuess &k = r.keys[listenGuess % 3];
            float kc = listenGuess == 0 ? r.keyConfidence : r.keyConfidence * 0.5f;
            auto q = [](float c) { return c > 0.6f ? "likely" : (c > 0.3f ? "a guess" : "very unsure"); };
            hot << L.size();
            L << QString("Key    %1 %2").arg(noteName(k.root)).arg(k.minor ? "minor" : "major");
            L << QString("       %1 (%2%)").arg(q(kc)).arg((int)(kc * 100));
            hot << L.size();
            L << (r.bpm > 0 ? QString("Tempo  %1 bpm").arg((int)std::round(r.bpm)) : QString("Tempo  no steady pulse"));
            L << QString("       %1 (%2%)").arg(q(r.bpmConfidence)).arg((int)(r.bpmConfidence * 100));
            L << "";
            L << "Detection is only an estimate. If it is wrong,";
            L << "set the key by hand in the Key menu.";
        }
    }
    g->setPen(hw::CRT_GRAT);
    g->setBrush(Qt::NoBrush);
    g->drawRect(QRectF(GRAT.x(), GRAT.y(), GRAT.width() - 1, GRAT.height() - 1));
    crtInverse(QRectF(GRAT.x() + 1, GRAT.y() + 1, GRAT.width() - 2, 16), title, 13, Qt::AlignLeft | Qt::AlignVCenter);
    for (int i = 0; i < L.size() && i < 14; ++i) {
        QRectF r(GRAT.x() + 6, GRAT.y() + 21 + i * 15, GRAT.width() - 12, 15);
        if (hot.contains(i)) crtInverse(QRectF(r.x() - 2, r.y(), L[i].size() * 8 + 8, 15), L[i], 13, Qt::AlignLeft | Qt::AlignVCenter);
        else crtText(r, L[i], 13, vw == V_KEYTEST && L[i].startsWith("up ") ? hw::CRT_DIM : hw::CRT_P);
    }
    if (clock.elapsed() < toastUntil && !toastText.isEmpty())
        crtInverse(QRectF(GRAT.x() + 20, GRAT.bottom() - 20, GRAT.width() - 40, 17), toastText.left(44), 13);
}
