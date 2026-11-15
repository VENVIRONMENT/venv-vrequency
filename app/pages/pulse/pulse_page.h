// venv-vrequency — the Pulse page (PG_PULSE): eight pulse pads, the selected pad's picture and its
// four sliders, the last four seconds of hits, and LOOP / LATCH / SYNC / TYPE.
// pulse_page.cpp holds the keys, the actions and the soft keys; pulse_pads.cpp the pads and the
// selected pad; pulse_strip.cpp the record of hits. The pads' settings live in the engine (and its
// sessions); the page keeps only which pad is selected.
#pragma once
#include "app/pages/page.h"
#include "app/pages/pulse/pulse_picture.h"
#include "core/pulse/pulse_bank.h"
#include <deque>

class QJsonObject;

class PulsePage : public Page {
public:
    explicit PulsePage(Shell &s) : Page(s) {}
    void paint() override;
    bool key(int qtKey, int plainAct) override;
    void tick() override { drainTicks(); }

    // what the window, the control socket, the tour and the self-test reach
    void hitPad(int pad);                    // a tap: plays it and selects it (a latched pad: lets it go)
    void setLatch(int pad, bool on);
    void select(int pad);
    void cycleType();                        // TYPE: the selected pad's next kind
    int selected() const { return sel; }
    void load(const QJsonObject &state);
    void save(QJsonObject &state) const;
    size_t ticksKept() const { return hist.size(); }

private:
    int sel = 0;
    uint32_t seenHits[td::kPulsePads] = {0};
    qint64 padLit[td::kPulsePads] = {0};     // when each pad last sounded
    PulsePicture small[td::kPulsePads], big;
    std::deque<td::PulseTick> hist;          // the strip's hits, oldest first
    uint32_t tickCursor = 0;
    int typePad = -1, typeSent = 0;          // TYPE's last request, until the engine shows it
    qint64 typeAt = 0;

    float rateNow(int pad) const;            // RATE with SYNC applied
    void setField(int field, float v);
    void drainTicks();
    void paintPads(qint64 now);
    void paintSelected(qreal y);
    void paintSliders(QRectF r);
    void paintStrip(QRectF r);
    void paintSoftKeys(QRectF r);
};
