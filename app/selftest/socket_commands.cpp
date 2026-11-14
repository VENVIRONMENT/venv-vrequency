// venv-vrequency self-test — control socket: every command, with good arguments and with bad ones.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace td;

namespace selftest {
namespace {
// every command: a request that works, then one that must be refused
struct Case { const char *req; bool ok; int wait; };
const Case cases[] = {
    {R"({"cmd":"note","string":1,"degree":0,"dur_ms":50,"velocity":0.5})", true, 0},
    {R"({"cmd":"note","string":3,"degree":0})", false, 0},
    {R"({"cmd":"note","string":"low","degree":0})", false, 0},
    {R"({"cmd":"note","string":0})", false, 0},
    {R"({"cmd":"note","string":0,"degree":1,"midi":60})", false, 0},
    {R"({"cmd":"note","string":0,"midi":300})", false, 0},
    {R"({"cmd":"note","string":0,"degree":0,"dur_ms":99999999})", false, 0},
    {R"({"cmd":"chord","degrees":[0,2,4],"dur_ms":100})", true, 0},
    {R"({"cmd":"chord","degrees":[0,2,4,6]})", false, 0},
    {R"({"cmd":"notes","notes":[{"string":0,"degree":0},{"string":2,"midi":72}],"dur_ms":100})", true, 0},
    {R"({"cmd":"notes","notes":[{"string":9,"degree":0}]})", false, 0},
    {R"({"cmd":"notes","notes":[]})", false, 0},
    {R"({"cmd":"slide","string":0,"from":0,"to":2,"ms":200})", true, 300},
    {R"({"cmd":"slide","string":0,"from":0})", false, 0},
    {R"({"cmd":"slide","string":0,"from":"a","to":2})", false, 0},
    {R"({"cmd":"release","string":0})", true, 0},
    {R"({"cmd":"release","string":7})", false, 0},
    {R"({"cmd":"panic"})", true, 100},
    {R"({"cmd":"get_params"})", true, 0},
    {R"({"cmd":"set_param","name":"cutoff","value":99999})", true, 0},
    {R"({"cmd":"set_param","name":"nonsense","value":1})", false, 0},
    {R"({"cmd":"set_param","name":"cutoff","value":"high"})", false, 0},
    {R"({"cmd":"set_param","name":"cutoff"})", false, 0},
    {R"({"cmd":"sound","slot":3})", true, 80},
    {R"({"cmd":"sound","slot":11})", false, 0},
    {R"({"cmd":"sound","name":"mutate"})", true, 0},
    {R"({"cmd":"sound","name":"undo"})", true, 0},
    {R"({"cmd":"sound","name":"random"})", true, 0},
    {R"({"cmd":"sound","name":"Haze"})", true, 80},
    {R"({"cmd":"sound","name":"no such sound"})", false, 0},
    {R"({"cmd":"sound"})", false, 0},
    {R"({"cmd":"save_favourite","name":"Copilot test"})", true, 0},
    {R"({"cmd":"save_favourite"})", false, 0},
    {R"({"cmd":"scale","name":"major","root":"C"})", true, 80},
    {R"({"cmd":"scale","name":"minor pentatonic","root":"A"})", true, 80},
    {R"({"cmd":"scale","name":"polka"})", false, 0},
    {R"({"cmd":"scale","root":"H"})", false, 0},
    {R"({"cmd":"tempo","bpm":120})", true, 60},
    {R"({"cmd":"tempo","bpm":10})", false, 0},
    {R"({"cmd":"tempo"})", false, 0},
    {R"({"cmd":"octave","delta":1})", true, 80},
    {R"({"cmd":"octave","value":0})", true, 80},
    {R"({"cmd":"octave","value":9})", false, 0},
    {R"({"cmd":"bend","value":0.5,"ms":100})", true, 0},
    {R"({"cmd":"bend","value":3})", false, 0},
    {R"({"cmd":"sustain","on":true})", true, 0},
    {R"({"cmd":"sustain","on":false})", true, 0},
    {R"({"cmd":"sustain","on":"yes"})", false, 0},
    {R"({"cmd":"loop","action":"clear"})", true, 120},
    {R"({"cmd":"loop","action":"stop"})", false, 0},
    {R"({"cmd":"loop","action":"play"})", false, 0},
    {R"({"cmd":"loop","action":"clear"})", true, 0},
    {R"({"cmd":"loop","action":"undo"})", true, 0},
    {R"({"cmd":"loop","action":"record"})", true, 100},
    {R"({"cmd":"loop","action":"dance"})", false, 0},
    {R"({"cmd":"loop","action":"clear"})", true, 100},
    {R"({"cmd":"backing","chords":["I","vi",4,5],"style":"pulse"})", true, 150},
    {R"({"cmd":"backing","chords":[9]})", false, 0},
    {R"({"cmd":"backing","chords":"I"})", false, 0},
    {R"({"cmd":"backing","action":"stop"})", true, 100},
    {R"({"cmd":"backing","action":"play"})", true, 100},
    {R"({"cmd":"backing","action":"remove_last"})", true, 0},
    {R"({"cmd":"backing"})", false, 0},
    {R"({"cmd":"beat","kit":"crunchy"})", true, 0},
    {R"({"cmd":"beat","kit":"disco"})", false, 0},
    {R"({"cmd":"beat","set_step":{"drum":"kick","step":0,"on":true}})", true, 0},
    {R"({"cmd":"beat","set_step":[{"drum":"snare","step":4},{"drum":2,"step":2}]})", true, 100},
    {R"({"cmd":"beat","set_step":{"drum":"kick","step":16}})", false, 0},
    {R"({"cmd":"beat","set_step":{"drum":"banjo","step":1}})", false, 0},
    {R"({"cmd":"beat","hit":{"drum":"clap"}})", true, 0},
    {R"({"cmd":"beat","hit":"nope"})", false, 0},
    {R"({"cmd":"beat","action":"play"})", true, 100},
    {R"({"cmd":"beat","action":"stop"})", true, 100},
    {R"({"cmd":"beat"})", false, 0},
    {R"({"cmd":"track","action":"clear"})", true, 60},
    {R"({"cmd":"track","action":"play"})", false, 0},
    {R"({"cmd":"track","action":"export"})", false, 0},
    {R"({"cmd":"track","action":"place","clip":0,"bar":0})", false, 0},
    {R"({"cmd":"track","action":"keep"})", true, 100},
    {R"({"cmd":"track","action":"place","clip":0,"bar":0})", true, 60},
    {R"({"cmd":"track","action":"place","clip":0,"bar":0})", false, 0},
    {R"({"cmd":"track","action":"place","clip":0,"bar":99})", false, 0},
    {R"({"cmd":"track","action":"play"})", true, 150},
    {R"({"cmd":"track","action":"stop"})", true, 100},
    {R"({"cmd":"track","action":"export"})", true, 0},
    {R"({"cmd":"track","action":"rewind"})", false, 0},
    {R"({"cmd":"page","name":"beat"})", true, 0},
    {R"({"cmd":"page","name":"chat"})", true, 0},
    {R"({"cmd":"page","name":"synth"})", true, 0},
    {R"({"cmd":"page","name":"nowhere"})", false, 0},
    {R"({"cmd":"session","action":"save","name":"copilot-test"})", true, 0},
    {R"({"cmd":"session","action":"load","name":"copilot-test"})", true, 100},
    {R"({"cmd":"session","action":"load","name":"missing"})", false, 0},
    {R"({"cmd":"session","action":"save","name":"../evil"})", false, 0},
    {R"({"cmd":"screenshot"})", true, 0},
    {R"({"cmd":"chat_say","text":"hello from the test"})", true, 0},
    {R"({"cmd":"chat_say","text":""})", false, 0},
    {R"({"cmd":"chat_say","text":"x","kind":"shout"})", false, 0},
    {R"({"cmd":"chat_say"})", false, 0},
    {R"({"cmd":"chat_poll","since":0})", true, 0},
    {R"({"cmd":"chat_poll","since":"zero"})", false, 0},
    {R"({"cmd":"chat_wait","since":0,"timeout_ms":99999})", false, 0},
    {R"({"cmd":"frobnicate"})", false, 0},
    {R"({"cmd":42})", false, 0},
    {R"({"id":7})", false, 0},
};
} // namespace

// each request gets the answer it should; then what the commands left behind
void socketEveryCommand(Context &ctx, Link &link) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst; Client &c = link.c;
    int bad = 0;
    QString firstBad;
    QString shotPath, exportPath;
    for (const Case &cs : cases) {
        const QJsonObject rr = c.call(cs.req);
        const bool got = rr.contains("ok") && rr["ok"].toBool();
        const bool answered = rr.contains("ok") && (got || !rr["error"].toString().isEmpty());
        if (got != cs.ok || !answered) { ++bad; if (firstBad.isEmpty()) firstBad = QString("%1 -> %2").arg(cs.req, QString::fromUtf8(QJsonDocument(rr).toJson(QJsonDocument::Compact))); }
        if (QByteArray(cs.req).contains("screenshot")) shotPath = rr["result"].toObject()["path"].toString();
        if (QByteArray(cs.req).contains("\"export\"") && got) exportPath = rr["result"].toObject()["path"].toString();
        if (cs.wait) wait(cs.wait);
    }
    check(bad == 0, QString("%1 requests: every command works with good arguments and refuses bad ones%2").arg((int)(sizeof cases / sizeof cases[0])).arg(bad ? " (first: " + firstBad + ")" : QString()));
    c.call(R"({"cmd":"panic"})");
    c.call(R"({"cmd":"loop","action":"clear"})");
    {
        const QImage png(shotPath);
        check(!shotPath.isEmpty() && png.width() == 720 && png.height() == 720, "screenshot writes a 720x720 PNG");
        QFile::remove(shotPath);
    }
    {
        for (int i = 0; i < 100 && w.exportStatus() == 1; ++i) wait(100);
        wait(150);
        QFile ef(exportPath);
        check(!exportPath.isEmpty() && ef.exists() && ef.size() > 44 + 48000, "track export (async) writes a WAV and gives its path");
    }
    const QJsonObject r = c.call(R"({"cmd":"state"})");
    {
        const QJsonObject st = r["result"].toObject();
        check(st["track"].toObject()["clips"].toArray().size() >= 2 && st["backing"].toObject()["chords"].toArray().size() >= 3 &&
              st["beat"].toObject()["steps"].toObject()["kick"].toString().startsWith('x') && st["tempo"].toDouble() == 120 && st["page"].toString() == "synth",
              "state reflects what the commands did (track clips, backing chords, beat steps, tempo, page)");
    }
    inst.set(S_BPM, 100);
    inst.beatDo(BT_ERASE, -1);
    inst.cmd(EV_BACK, BC_CLEAR);
}
} // namespace selftest
