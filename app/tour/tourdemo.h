// venv-vrequency — private to the tour's picture-drawing: the scratch-window driver and the steps of the demo.
#pragma once
#include "app/tour/tour.h"
#include "app/shell/mainwindow.h"
#include "core/instrument/instrument.h"
#include <QString>
#include <array>
#include <initializer_list>
#include <vector>

// The engine has no audio thread here: this runs it by hand, faster than real time.
inline void pump(td::Instrument &inst, int ms) {
    static float l[256], r[256];
    for (int n = ms * 48 / 256 + 1; n > 0; --n) inst.process(l, r, 256);
}

struct Rig {
    td::Instrument &inst;
    MainWindow &w;
    void key(int k, bool down) { w.handleKey(k, down); }
    void press(int k) { w.shot(); key(k, true); key(k, false); }  // the window is drawn first, as it would be on a screen: its zones and readouts are current
    void ctrl(int k) { key(Qt::Key_Control, true); press(k); key(Qt::Key_Control, false); }
    void tap(const QString &zone) {
        w.shot();
        const QRectF r = w.zoneRect(zone);
        w.pointer(7, r.center(), PH_PRESS);
        w.pointer(7, r.center(), PH_RELEASE);
    }
    void touch(QPointF a) { w.shot(); w.pointer(3, a, PH_PRESS); w.pointer(3, a, PH_RELEASE); }
    void settle(int frames, int ms = 40) { for (int i = 0; i < frames; ++i) { pump(inst, ms); w.shot(); } }
};

// The part of the picture-drawing that needs the window's private state (a friend of MainWindow).
struct TourDemo {
    static void capture(Rig &rig, int page, TourView::Pic &pic);       // the controls of whatever the scratch window shows now (page: a tour page)
    static td::LayerData melody(const td::Patch &sound, std::initializer_list<std::array<double, 3>> notes);
    static void extras(int page, TourView::Pic &pic);                  // named controls that are not zones
    static void playPages(Rig &rig, std::vector<TourView::Pic> &pics); // synth, sound, pulse, freq, backing, tune, system, help, chat
    static void build(std::vector<TourView::Pic> &pics, SinkProbe *sink);
};
