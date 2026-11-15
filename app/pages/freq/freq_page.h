// venv-vrequency — the Freq page (PG_FREQ): up to four steady tones at exact frequencies. From the
// top: the Freq sound's last 1.5 s as an envelope (beating shows as swells), two cycles of tone 1
// and the spectrum, the four tone rows, the bend-and-warp cells, and HOLD / GATE / KEYS / SNAP.
// freq_page.cpp holds the keys, the actions and the soft keys; freq_views.cpp the three views,
// drawn only from the Freq tap (Instrument::readFreqScope); freq_rows.cpp the tone rows and the
// warp cells. The tones' settings live in the engine (and its sessions); the page keeps only which
// tone is selected and which letter keys it is holding.
#pragma once
#include "app/pages/page.h"
#include "core/freq/freq.h"
#include <QSet>
#include <vector>

class QJsonObject;

class FreqPage : public Page {
public:
    explicit FreqPage(Shell &s) : Page(s) {}
    void paint() override;
    bool key(int qtKey, int plainAct) override;
    void tick() override;

    // what the window, the tour and the self-test reach
    void select(int tone);                   // 0..3
    int selected() const { return sel; }
    void tapRow(int tone);                   // selects it; an off tone is switched on
    void cycleWave(int tone);
    void setField(int tone, int field, float v);   // one setting (tone -1: the page's own)
    void toggle(int field);                  // FF_HOLD, FF_GATE or FF_KEYS
    void stepRate(int dir);                  // GATE's rate, one step up or down
    void snap() { setField(sel, td::FF_SNAP, 0); }
    void load(const QJsonObject &state);
    void save(QJsonObject &state) const;

private:
    int sel = 0;
    QSet<int> heldKeys;                      // KEYS: the letter keys this page took, still down
    std::vector<float> buf;                  // the newest Freq output (the tap)
    std::vector<float> db;                   // its spectrum, dB
    static constexpr int kSpecW = 330;
    float specLine[kSpecW] = {};
    float scale = 0.05f, peak = 0;           // the views' full height (from the 1.5 s envelope), its peak
    float modDragX = -1, modDragAmt = 0;     // "2 > 1": a drag sets the amount, a tap cycles the mode
    bool modDragged = false;

    void paintScope(QRectF r);               // freq_views.cpp
    void paintCycles(QRectF r);
    void paintSpectrum(QRectF r);
    void paintRow(int i, QRectF r);          // freq_rows.cpp
    void paintWarp(qreal y);
    void cell(QRectF r, const char *zone, const QString &label, const QString &value, float norm, int field,
              float lo, float hi, bool logScale);
    void paintSoftKeys(QRectF r);
    QString sectionLine() const;
};

QString freqHz(float hz);                    // "220.00 Hz", "1.20 kHz" (freq_rows.cpp)
