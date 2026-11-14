// venv-vrequency — the Beat page (PG_BEAT): the bar for all eight drums, the pads, the row of kept
// beats and the transport. beat_page.cpp holds the keys and the transport, beat_grid.cpp the bar and
// the pads, kept_beats.cpp the beats that are kept (and written to the state file).
#pragma once
#include "app/pages/page.h"
#include <QString>
#include <vector>

class QJsonObject;

class BeatPage : public Page {
public:
    explicit BeatPage(Shell &s) : Page(s) {}
    void paint() override;
    bool key(int qtKey, int plainAct) override;
    void tick() override { watch(); }
    void disarm() override { erase = false; }

    // what the window, the tour and the self-test reach
    bool erase = false;                      // Erase is armed: the next pad clears that drum
    void load(const QJsonObject &state, const td::SessionData &sd);
    void save(QJsonObject &state) const;
    void switchTo(int i);
    void newBeat();
    int kept() const { return (int)beats.size(); }
    int shown() const { return cur; }
    QString keptName(int i) const { return i >= 0 && i < (int)beats.size() ? beats[i].name : QString(); }

private:
    // kept beats: every beat you make has its own place; the one shown is the one the engine plays
    struct Slot { QString name; uint16_t pat[td::kDrums] = {0}; int kit = 0; };
    std::vector<Slot> beats;
    int cur = 0, row = 0, nameNext = 2;      // the beat shown, the page of the row, the next "Beat N"
    qint64 holdUntil = 0;                    // just switched: the engine has not caught up yet
    uint32_t seenHits[td::kDrums] = {0};
    qint64 padLit[td::kDrums] = {0};         // when each pad last sounded

    void hitPad(int drum);
    void watch();                            // copy what the engine plays into the beat shown
    void paintGrid();                        // the whole bar for all eight drums
    void paintPads(qint64 now);
    void paintKept(qreal y);                 // the row of kept beats
    void paintTransport();
};
