// venv-vrequency self-test — control socket: notes, slides and panic, measured at the output.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include <QJsonObject>
#include <cmath>

using namespace td;

namespace selftest {
// notes, slides and panic: what sounds is measured at the output
void socketNotes(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; Client &c = link.c;
    // a plain sine, no effects, so the pitch can be measured
    for (const char *p : {R"({"cmd":"set_param","name":"wave1","value":0})", R"({"cmd":"set_param","name":"mix2","value":0})",
                          R"({"cmd":"set_param","name":"unison","value":0})", R"({"cmd":"set_param","name":"sub","value":0})",
                          R"({"cmd":"set_param","name":"fm","value":0})", R"({"cmd":"set_param","name":"vibdepth","value":0})",
                          R"({"cmd":"set_param","name":"delaymix","value":0})", R"({"cmd":"set_param","name":"reverbmix","value":0})",
                          R"({"cmd":"set_param","name":"chorus","value":0})", R"({"cmd":"set_param","name":"cutoff","value":16000})",
                          R"({"cmd":"set_param","name":"fenv","value":0})", R"({"cmd":"set_param","name":"sustain","value":1})"})
        c.call(p);
    wait(100);
    inst.set(S_VOLUME, 0.3f);
    QJsonObject r = c.call(R"({"cmd":"note","string":0,"degree":0,"dur_ms":1500})");
    wait(450);
    float hz = scopeHz(inst);
    check(okOf(r) && std::fabs(hz - 110.0f) < 2.0f && inst.uiState().str[0].gate, QString("a note over the socket: string 0 degree 0 is A2 (measured %1 Hz)").arg(hz, 0, 'f', 1));
    key(w, Qt::Key_X, true); wait(60); // the player's own key on the same string slides it: both interleave
    check(inst.uiState().str[0].target == 48, "the player's keys still play over a socket note");
    key(w, Qt::Key_X, false);
    c.call(R"({"cmd":"release"})"); wait(300);
    r = c.call(R"({"cmd":"note","string":1,"degree":3,"dur_ms":1500})");
    wait(450);
    hz = scopeHz(inst);
    const float want = (float)midiToHz(inst.uiState().set.tuning.noteFor(1, 3, false));
    check(okOf(r) && std::fabs(hz - want) < want * 0.02f, QString("degree 3 of the middle string follows the scale (%1 Hz, wanted %2)").arg(hz, 0, 'f', 1).arg(want, 0, 'f', 1));
    c.call(R"({"cmd":"panic"})"); wait(250);
    r = c.call(R"({"cmd":"note","string":2,"midi":81,"dur_ms":1500})");
    wait(450);
    hz = scopeHz(inst);
    check(okOf(r) && std::fabs(hz - 880.0f) < 12.0f, QString("an exact MIDI note: 81 is 880 Hz (measured %1 Hz)").arg(hz, 0, 'f', 1));
    r = c.call(R"({"cmd":"note","string":2,"degree":-2,"dur_ms":200})");
    check(okOf(r), "a negative scale degree is accepted");
    c.call(R"({"cmd":"panic"})"); wait(250);
    check(inst.uiState().voices == 0, "panic over the socket silences everything");
    r = c.call(R"({"cmd":"note","string":0,"degree":0,"dur_ms":150})");
    wait(120);
    check(inst.uiState().str[0].gate, "a note sounds for its duration...");
    wait(500);
    check(!inst.uiState().str[0].gate, "...and lets go by itself");
    c.call(R"({"cmd":"slide","string":0,"from":0,"to":3,"ms":900})");
    wait(200);
    const float fromNote = inst.uiState().str[0].note;
    wait(500);
    check(inst.uiState().str[0].note > fromNote + 0.5f || std::fabs(inst.uiState().str[0].target - fromNote) > 0.5f, "a slide moves the string's pitch");
    wait(700);
    c.call(R"({"cmd":"panic"})");
}
} // namespace selftest
