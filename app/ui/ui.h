// venv-vrequency — the small immediate-mode toolkit every page draws with. Each frame a page draws
// its controls and registers their touch zones in the same call, so drawing and hit-testing can
// never disagree. Nothing here knows about the synth: it is text, rules, zones and three controls.
#pragma once
#include <QColor>
#include <QFont>
#include <QHash>
#include <QRectF>
#include <QSet>
#include <QString>
#include <QStringList>
#include <functional>
#include <utility>
#include <vector>

class QPainter;

enum Phase { PH_PRESS, PH_MOVE, PH_RELEASE };          // what a touch is doing
enum Look { LOOK_PLAIN, LOOK_ARMED };                  // LOOK_ARMED: double frame, "waiting for a choice"

class Ui {
public:
    using Touch = std::function<void(QPointF, int)>;   // where, and the Phase
    struct Zone { QRectF r; QString name; Touch fn; };
    using Text = std::pair<QRectF, QString>;

    QPainter *g = nullptr;                             // the frame being drawn; null between frames

    // ---- a frame ----
    void beginFrame(QPainter &painter);                // forgets last frame's zones and text
    void endFrame() { g = nullptr; }
    void recordTexts(bool on) { recordText = on; }     // while on, drawn text is kept for zone labels

    // ---- text and rules ----
    QFont font(int px, bool bold) const;
    static bool pixelFont();                                  // mockup: VREQUENCY_FONT=MGS1, pixel faces by role
    static int pixelRole(int px, bool bold);                  // an old pixel size as one of five roles
    void pixelTxt(QRectF r, const QString &s, int role, QColor c, int align); // shrink, then shorten, to fit
    int chW(int px) const;                                    // character cell width at a size
    QString fit(const QString &s, int px, qreal width) const; // shorten to fit
    void txt(QRectF r, const QString &s, int px, QColor c, int align = Qt::AlignCenter, bool bold = false);
    void frame(QRectF r, QColor line, QColor fill = QColor());
    void tag(QRectF r, const QString &s, int px, int align = Qt::AlignCenter);
    void section(qreal y, const QString &s);

    // ---- controls ----
    void zone(QRectF r, const QString &name, Touch fn);
    void button(QRectF r, const QString &name, const QString &label, bool on, std::function<void()> fn,
                const QString &sub = QString(), int look = LOOK_PLAIN);
    void slider(QRectF r, const QString &name, const QString &label, float norm, const QString &value,
                std::function<void(float)> fn);
    void stepper(QRectF r, const QString &name, const QString &label, const QString &value,
                 std::function<void()> minus, std::function<void()> plus);
    bool isDown(const QString &name) const { return downNames.contains(name); } // a finger is on it

    // ---- touches ----
    // A press goes to the top zone under it, which keeps the touch until it is released. `pressed`
    // hears the zone's name before the zone acts. False: a move or release that belongs to no zone.
    bool touch(int id, QPointF c, int phase, const std::function<void(const QString &)> &pressed = nullptr);
    bool touching() const { return !captured.isEmpty(); }

    // ---- what the last frame drew ----
    const std::vector<Zone> &zones() const { return zoneList; }
    const std::vector<Text> &texts() const { return drawnText; }
    QRectF zoneRect(const QString &name) const;
    QStringList zoneNames() const;
    QString zoneAt(QPointF c) const;                   // the top zone there, or empty

private:
    std::vector<Zone> zoneList;
    QHash<int, Zone> captured;
    QSet<QString> downNames;
    std::vector<Text> drawnText;
    bool recordText = false;
    mutable QHash<quint32, QFont> fontCache;
    mutable QHash<int, int> widthCache;
};
