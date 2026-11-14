// venv-vrequency — guided help. Three things that let the copilot (Rook) teach on the real
// screen: a highlight overlay that rings the controls being talked about, a chat dock that lets
// the player answer on whatever page they are on, and a catalogue of every named touch zone.
#include "mainwindow.h"
#include "palette.h"
#include <QPainter>
#include <QRegularExpression>
#include <QSet>
#include <cmath>

using namespace pal;

QStringList chatWrap(const QString &s, int cols, int first); // chatpage.cpp

namespace {
const char *kPages[] = {"track", "synth", "sound", "beat", "backing", "tune", "system", "help", "chat"};
qreal dockDragY = 0;

// Names that stand for several zones. Only used when no zone has the name itself.
QStringList aliasZones(const QString &n) {
    if (n == "octave") return {"octave_down", "octave_up"};
    if (n == "strings") return {"string0", "string1", "string2"};
    if (n == "tabs") return {"tab_*"};
    if (n == "chords") return {"chord*"};
    if (n == "pads") return {"pad*"};
    return {};
}

bool hidden(const QString &n) { return n.startsWith("dock") || n == "ticker" || n == "keytestblock"; }

QRectF inflated(const QRectF &r, qreal d) { return r.adjusted(-d, -d, d, d); }
} // namespace

const char *MainWindow::pageName(int p) { return p >= 0 && p < PG_COUNT ? kPages[p] : "synth"; }

int MainWindow::pageFromName(const QString &n0) {
    const QString n = n0.toLower() == "play" ? QString("synth") : n0.toLower();
    for (int i = 0; i < PG_COUNT; ++i) if (n == kPages[i]) return i;
    return -1;
}

// ------------------------------------------------------------------ zone catalogue

QString MainWindow::zoneLabel(const QString &name) const {
    static const QHash<QString, QString> fixed = {
        {"xy", "X-Y pad (scope)"}, {"hdr_sound", "Sound name"}, {"hdr_tune", "Key, tempo and octave"},
        {"key_readout", "Key"}, {"tempo_readout", "Tempo"}, {"octave_readout", "Octave readout"},
        {"octave_down", "Octave down"}, {"octave_up", "Octave up"}, {"loop_pedal", "Loop pedal"},
        {"loop_undo", "Undo loop layer"}, {"loop_playstop", "Loop play / stop"}, {"sustain", "Sustain"},
        {"new_sound", "New sound"}, {"menu", "Menu"}, {"string0", "Low string"}, {"string1", "Middle string"},
        {"string2", "High string"}, {"track_lanes", "Track lanes"}, {"track_ruler", "Track ruler (bars)"},
        {"track_keep", "Keep"}, {"chop_strip", "Chop strip"}, {"chat_history", "Chat history"},
        {"chat_input", "Chat input"}, {"chat_send", "Send"}, {"help_section", "Help section title"},
        {"help_next", "Next help page"}, {"help_ask", "Ask Rook"}, {"help_back", "Back to playing"},
        {"dock", "Chat dock"}, {"dock_history", "Dock messages"}, {"dock_input", "Dock input"},
        {"dock_send", "Dock Send"}, {"dock_full", "Open the Chat page"}, {"dock_close", "Hide the dock"},
        {"ticker", "Message ticker"}, {"backmode", "Chord tap mode"}, {"backplay", "Backing play / stop"},
        {"sound_octave-", "Sound octave down"}, {"sound_octave+", "Sound octave up"},
    };
    if (fixed.contains(name)) return fixed[name];
    static const QRegularExpression lane("^mixer_(lane|fader|mute|solo)(\\d)$");
    const auto m = lane.match(name);
    if (m.hasMatch()) {
        const QString k = m.captured(1), ln = QString("Lane %1").arg(m.captured(2).toInt() + 1);
        return k == "lane" ? ln : (k == "fader" ? ln + " fader" : (k == "mute" ? ln + " mute" : ln + " solo"));
    }
    if (name.startsWith("tab_")) return QString("%1 tab").arg(name.mid(4));
    if (name.startsWith("help_row_")) { const QString d = helpDesc.value(name); if (!d.isEmpty()) return d.left(60); }
    for (const Zone &z : zones) {
        if (z.name != name) continue;
        if (z.r.width() * z.r.height() > 45000) break;       // a big area has no single caption
        QStringList parts;
        for (const auto &t : drawnText)
            if (z.r.contains(t.first.center())) parts << t.second.trimmed();
        parts.removeAll(QString());
        if (parts.isEmpty()) break;
        if (parts.size() > 3) parts = parts.mid(0, 3);
        QString s = parts.join(" ");
        if (s.size() > 40) s = s.left(39) + QString::fromUtf8("…");
        return s;
    }
    QString h = name;
    h.replace('_', ' ');
    return h;
}

QVector<MainWindow::ZoneInfo> MainWindow::zoneCatalogue(int page, int helpSub) {
    // draw the page off-screen, read its zones, and put everything back the way it was
    const int sPg = pg, sHelp = helpPage, sTopicPage = topicPage, sTapPage = lastTappedPage;
    const bool sDeep = deepEdit;
    const Hl sHl = hl;
    const QString sTopic = topicLabel, sTap = lastTapped, sSel = helpSel;
    const QStringList sTopicZ = topicZones;
    const bool sTourSusp = tourSuspended;
    tourSuspended = true;                                   // the page itself is wanted, not the tour's picture of it
    if (page != pg) { pg = page; deepEdit = false; }
    if (page == PG_HELP && helpSub >= 0) helpPage = qBound(0, helpSub, 3);
    hl.page = pg; hl.zones = sHl.page == page ? sHl.zones : QStringList(); hl.until = sHl.until;
    topicPage = pg;
    shot();
    QVector<ZoneInfo> out;
    const QRectF D = dockRect();
    for (const Zone &z : zones) {
        ZoneInfo zi;
        zi.name = z.name; zi.r = z.r; zi.label = zoneLabel(z.name);
        zi.covered = !D.isNull() && !hidden(z.name) && D.intersects(z.r);
        out.push_back(zi);
    }
    pg = sPg; helpPage = sHelp; deepEdit = sDeep; hl = sHl;
    tourSuspended = sTourSusp;
    topicPage = sTopicPage; topicLabel = sTopic; topicZones = sTopicZ; lastTapped = sTap; lastTappedPage = sTapPage; helpSel = sSel;
    shot();
    return out;
}

// ------------------------------------------------------------------ highlight

QStringList MainWindow::highlightedZones() {
    if (hl.zones.isEmpty()) return {};
    if (hl.page != pg || clock.elapsed() >= hl.until) { hl.zones.clear(); hl.label.clear(); return {}; }
    return hl.zones;
}

void MainWindow::clearHighlight() {
    hl.zones.clear(); hl.label.clear(); hl.until = 0; hl.page = -1;
    topicLabel.clear(); topicZones.clear();
    hlCaption = QRectF();
    update();
}

QStringList MainWindow::expandZoneNames(const QStringList &names, QStringList &unknown) const {
    QStringList have;
    for (const Zone &z : zones) if (!hidden(z.name)) have << z.name;
    QStringList out;
    auto add = [&](const QString &n) { if (!out.contains(n)) out << n; };
    std::function<bool(const QString &, int)> resolve = [&](const QString &n, int depth) -> bool {
        if (have.contains(n)) { add(n); return true; }
        bool any = false;
        if (n.endsWith('*') && n.size() > 1) {
            for (const QString &h : have) if (h.startsWith(n.left(n.size() - 1))) { add(h); any = true; }
            return any;
        }
        if (depth < 2)
            for (const QString &a : aliasZones(n)) any = resolve(a, depth + 1) || any;
        return any;
    };
    for (const QString &raw : names) {
        const QString n = raw.trimmed();
        if (n.isEmpty()) { unknown << raw.left(48); continue; }
        if (!resolve(n, 0)) unknown << n.left(48);
    }
    return out;
}

QStringList MainWindow::setHighlight(const QStringList &names, const QString &label, int ms, QStringList &unknown) {
    const QStringList known = expandZoneNames(names, unknown);
    if (known.isEmpty()) return known;
    hl.zones = known;
    hl.label = label;
    hl.page = pg;
    hl.issued = clock.elapsed();
    hl.until = hl.issued + qBound(100, ms, 30000);
    // what the dock says is being discussed: the label, else the name that was asked for
    QString what = label;
    if (what.isEmpty()) {
        const QString first = names.value(0).trimmed();
        what = (first.endsWith('*') || !aliasZones(first).isEmpty() || known.size() > 1) && !known.contains(first)
                   ? QString(first).remove('*').replace('_', ' ') : zoneLabel(known[0]);
    }
    topicLabel = what.left(28);
    topicZones = known;
    topicPage = pg;
    update();
    return known;
}

// What a message sent now carries as `focus`: the highlighted zones, else the ones the last agent
// message was about, else the control the player touched last on this page.
QStringList MainWindow::guideFocus() {
    QStringList f = highlightedZones();
    if (f.isEmpty() && topicPage == pg) f = topicZones;
    if (f.isEmpty() && !lastTapped.isEmpty() && lastTappedPage == pg) f << lastTapped;
    return f;
}

void MainWindow::paintHighlights() {
    hlCaption = QRectF();
    const QStringList names = highlightedZones();
    if (names.isEmpty()) return;
    const qreal t = clock.elapsed() / 1000.0;
    const qreal ph = 0.5 + 0.5 * std::sin(t * 2 * M_PI / 1.6);  // a gentle pulse
    QColor soft = INK;
    soft.setAlpha((int)(70 + 185 * ph));
    QVector<QRectF> rects;
    for (const QString &n : names) {
        const QRectF r = zoneRect(n);
        if (r.isNull()) continue;
        rects << r;
        frame(inflated(r, 6), BG);        // a dark gap keeps the rules readable over bright controls
        frame(inflated(r, 5), soft);      // outer rule: pulses
        frame(inflated(r, 4), BG);
        frame(inflated(r, 3), INK);       // inner rule: steady (2 px of padding inside it)
    }
    if (rects.isEmpty() || hl.label.isEmpty()) return;

    // the caption: a small tag by the first zone that covers neither a highlight nor the dock
    const QString text = fit(hl.label, 13, 40 * chW(13));
    const qreal w = text.size() * chW(13) + 12, h = 18;
    QVector<QRectF> avoid;
    for (const QRectF &r : rects) avoid << inflated(r, 8);
    if (!dockR.isNull()) avoid << inflated(dockR, 4);
    if (pg != PG_CHAT && !dockOpen && clock.elapsed() < tickerUntil) avoid << QRectF(12, 2, 696, 52) << QRectF(12, 664, 696, 52);
    const QRectF z = rects[0], screen(2, 2, 716, 716);
    auto ok = [&](const QRectF &c) {
        if (!screen.contains(c)) return false;
        for (const QRectF &a : avoid) if (a.intersects(c)) return false;
        return true;
    };
    const qreal x0 = qBound(2.0, z.x(), 718 - w), x1 = qBound(2.0, z.right() - w, 718 - w);
    QVector<QRectF> cand = {
        QRectF(x0, z.top() - 9 - h, w, h), QRectF(x0, z.bottom() + 9, w, h),
        QRectF(x1, z.top() - 9 - h, w, h), QRectF(x1, z.bottom() + 9, w, h),
        QRectF(z.right() + 9, z.center().y() - h / 2, w, h), QRectF(z.left() - 9 - w, z.center().y() - h / 2, w, h)};
    QRectF pick;
    for (const QRectF &c : cand) if (ok(c)) { pick = c; break; }
    if (pick.isNull()) { // anywhere free, nearest to the zone
        qreal best = 1e9;
        for (qreal y = 2; y + h <= 718; y += 6)
            for (qreal x = 2; x + w <= 718; x += 12) {
                const QRectF c(x, y, w, h);
                if (!ok(c)) continue;
                const qreal d = std::hypot(c.center().x() - z.center().x(), c.center().y() - z.center().y());
                if (d < best) { best = d; pick = c; }
            }
    }
    if (pick.isNull()) return;
    hlCaption = pick;
    frame(inflated(pick, 2), INK, BG);
    tag(pick, text, 13);
}

// ------------------------------------------------------------------ chat dock

void MainWindow::dockOpenFocus() {
    dockOpen = true;
    dockFocus = true;
    chatUnread = 0;
    tickerUntil = 0;
    update();
}

void MainWindow::dockClose() {
    askFocus.clear(); askPage.clear(); askVia.clear();
    dockOpen = false;
    dockFocus = false;
    dockR = QRectF();
    update();
}

// Where the dock goes: the bottom of the screen, as tall as is comfortable, unless that would
// cover something highlighted. Then it gets shorter, then it moves to the top, then shorter there.
void MainWindow::layoutDock() {
    dockR = QRectF();
    dockLines = 0;
    if (!dockOpen || pg == PG_CHAT || keyTest) return;
    QVector<QRectF> hot;
    for (const QString &n : highlightedZones()) { const QRectF r = zoneRect(n); if (!r.isNull()) hot << inflated(r, 8); }
    struct Opt { bool top; int lines; };
    static const Opt order[] = {{false, 4}, {false, 2}, {true, 4}, {true, 2}, {false, 1}, {true, 1}, {false, 0}, {true, 0}};
    QRectF fallback;
    for (const Opt &o : order) {
        const qreal h = 74 + 16 * o.lines;
        const QRectF r(6, o.top ? 4 : 720 - 4 - h, 708, h);
        if (fallback.isNull()) fallback = r;
        bool clash = false;
        for (const QRectF &x : hot) clash = clash || x.intersects(r.adjusted(-3, -3, 3, 3));
        if (!clash) { dockR = r; dockLines = o.lines; return; }
    }
    dockR = QRectF(6, 720 - 4 - 74, 708, 74); // nothing clear: the smallest, at the bottom
    dockLines = 0;
}

void MainWindow::paintDock() {
    if (dockR.isNull()) return;
    chatUnread = 0;
    const QRectF D = dockR;
    frame(D.adjusted(-2, -2, 2, 2), INK, BG);
    frame(D, dockFocus ? INK : LOW);
    zone(D, "dock", [](QPointF, int) {}); // swallows touches so nothing under the dock is pressed by accident

    // header: what is being discussed, who is listening, and the two ways out
    const bool listening = copilotWaiters > 0 || clock.elapsed() - copilotSeenAt < 45000;
    qreal hx = D.x() + 6;
    if (!topicLabel.isEmpty()) {
        const QString t = QString::fromUtf8("▸ ") + fit(topicLabel.toUpper(), 13, 260);
        const qreal tw = t.size() * chW(13) + 14;
        tag(QRectF(hx, D.y() + 4, tw, 24), t, 13, Qt::AlignLeft | Qt::AlignVCenter);
        hx += tw + 8;
    }
    txt(QRectF(hx, D.y() + 4, D.right() - 150 - hx, 24), listening ? "Rook is listening" : "Rook is not connected", 12, listening ? MID : LOW, Qt::AlignLeft | Qt::AlignVCenter);
    button(QRectF(D.right() - 140, D.y() + 4, 64, 24), "dock_full", "Full", false, [this] { chatToggle(); });
    button(QRectF(D.right() - 70, D.y() + 4, 64, 24), "dock_close", "Hide", false, [this] { dockClose(); });

    // the last few lines
    const int cw = chW(13), lh = 16;
    const QRectF hist(D.x() + 6, D.y() + 32, D.width() - 12, dockLines * lh + 2);
    if (dockLines > 0) {
        struct Ln { int msg; QString text; bool head; };
        const int cols = (int)((hist.width() - 10) / cw);
        std::vector<Ln> lines;
        const size_t n0 = chat.size() > 8 ? chat.size() - 8 : 0;
        for (size_t i = n0; i < chat.size(); ++i) {
            const int nameW = chat[i].from.size() + 3;
            const QStringList rows = chatWrap(chat[i].text, cols, cols - nameW);
            for (int k = 0; k < rows.size(); ++k) lines.push_back({(int)i, rows[k], k == 0});
        }
        const int total = (int)lines.size();
        chatScroll = qBound(0, chatScroll, qMax(0, total - dockLines));
        const int end = total - chatScroll, start = qMax(0, end - dockLines);
        if (chat.empty()) txt(QRectF(hist.x() + 4, hist.y(), hist.width(), lh), "No messages yet. Type below and press Enter.", 13, MID, Qt::AlignLeft | Qt::AlignVCenter);
        for (int i = start; i < end; ++i) {
            const Ln &ln = lines[i];
            const ChatMsg &m = chat[ln.msg];
            const qreal y = hist.y() + (i - start) * lh;
            qreal x = hist.x() + 4;
            if (ln.head) {
                QString name = m.from.toUpper();
                if (m.kind == "ask") name += "?";
                const qreal w = (name.size() + 1) * cw + 4;
                if (m.user) { frame(QRectF(x, y, w, lh - 1), MID); txt(QRectF(x, y, w, lh - 1), name, 12, INK, Qt::AlignCenter, true); }
                else tag(QRectF(x, y, w, lh - 1), name, 12);
                x += w + cw;
            }
            txt(QRectF(x, y, hist.right() - x, lh), ln.text, 13, m.user ? MID : INK, Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (total > dockLines && chatScroll < total - dockLines) txt(QRectF(hist.right() - 14, hist.y(), 12, 12), QString::fromUtf8("▴"), 11, MID);
        zone(hist, "dock_history", [this](QPointF p, int ph) { // drag to scroll
            if (ph == PH_PRESS) { dockDragY = p.y(); return; }
            const int d = (int)std::round((p.y() - dockDragY) / 16.0);
            if (d) { chatScroll += d; dockDragY = p.y(); }
        });
    }

    // the input line and Send
    const QRectF box(D.x() + 6, D.bottom() - 42, D.width() - 12 - 102, 36);
    frame(box, dockFocus ? INK : LOW, BG);
    if (dockFocus) frame(box.adjusted(2, 2, -2, -2), GRAT);
    const int bc = (int)((box.width() - 20) / chW(15));
    QString shown = chatDraft.size() > bc - 1 ? QString::fromUtf8("…") + chatDraft.right(bc - 2) : chatDraft;
    if (chatDraft.isEmpty()) {
        const QString hint = dockFocus ? QString("type to ask  (Esc: back to the keys)") : QString("%1+%2 or touch to type").arg(km.keyFor(A_COMMAND, false)).arg(km.keyFor(A_CHAT, true));
        txt(box.adjusted(10 + (dockFocus ? chW(15) : 0), 0, -10, 0), fit(hint, 14, box.width() - 20), 14, LOW, Qt::AlignLeft | Qt::AlignVCenter);
    } else txt(box.adjusted(10, 0, -10, 0), shown, 15, dockFocus ? INK : MID, Qt::AlignLeft | Qt::AlignVCenter, dockFocus);
    if (dockFocus && (clock.elapsed() / 500) % 2 == 0) {
        const qreal cx = box.x() + 10 + (chatDraft.isEmpty() ? 0 : shown.size() * chW(15));
        g->fillRect(QRectF(cx, box.y() + 8, chW(15) - 2, 20), INK);
    }
    zone(box, "dock_input", [this](QPointF, int ph) { if (ph == PH_PRESS) dockFocus = true; });
    button(QRectF(D.right() - 102, D.bottom() - 42, 96, 36), "dock_send", "Send", false, [this] { chatSend(); });
}
