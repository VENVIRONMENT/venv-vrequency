// venv-vrequency self-test — control socket: hostile input is refused and harms nothing.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>
#include <cmath>

using namespace td;

namespace selftest {
// hostile input
void socketHostileInput(Context &ctx, Link &link) {
    Instrument &inst = ctx.inst; ControlServer &ctl = link.server; Client &c = link.c;
    c.send("this is not json");
    QJsonObject h = QJsonDocument::fromJson(c.line(1000)).object();
    check(!h["ok"].toBool(true) && h["error"].toString().startsWith("bad JSON"), "garbage is answered with an error");
    c.send("[1,2,3]");
    h = QJsonDocument::fromJson(c.line(1000)).object();
    check(!h["ok"].toBool(true), "a JSON array is refused");
    c.send("{\"cmd\":\"note\",\"string\":0,\"degree\":0,\"dur_ms\":" + QByteArray(40000, '9') + "}");
    h = QJsonDocument::fromJson(c.line(2000)).object();
    check(h.contains("ok") && !h["ok"].toBool(), "a number thousands of digits long is refused");
    c.send(QByteArray(R"({"cmd":"chat_say","text":")") + QByteArray(2 * 1024 * 1024, 'a') + "\"}");
    h = QJsonDocument::fromJson(c.line(3000)).object();
    check(!h["ok"].toBool(true), "a request line over a megabyte is refused");
    QJsonObject r = c.call(R"({"cmd":"state"})");
    check(okOf(r), "and the connection still works after it");
    c.send("{\"cmd\":\"chat_say\",\"text\":" + QByteArray(2000, '[') + "}");
    h = QJsonDocument::fromJson(c.line(2000)).object();
    check(h.contains("ok") && !h["ok"].toBool(), "deeply nested JSON is refused");
    c.send("{\"cmd\":\"note\",\"string\":0,\"degree\":0}\n{\"cmd\":\"state\"}\n{bad\n{\"cmd\":\"state\"}");
    int answers = 0;
    for (int i = 0; i < 4; ++i) if (!c.line(500).isEmpty()) ++answers;
    check(answers == 4, "several requests in one write are answered one by one");
    c.s.write("{\"cmd\":\"state\""); c.s.flush(); // half a line, then the client goes away
    c.s.disconnectFromServer();
    wait(100);
    check(c.open(link.path) && okOf(c.call(R"({"cmd":"state"})")), "a client that vanishes mid-line does not disturb the server");
    // every command with every argument set to the wrong kind of value
    static const char *cmds[] = {"note", "notes", "chord", "slide", "release", "set_param", "sound", "save_favourite", "scale", "tempo", "octave", "bend", "sustain",
                                 "loop", "backing", "pulse", "page", "session", "chat_say", "chat_poll", "highlight", "unhighlight", "zones"};
    static const char *argk[] = {"string", "degree", "midi", "dur_ms", "velocity", "from", "to", "ms", "name", "value", "slot", "action", "chords", "pad", "type", "tone", "length", "rate", "level", "latch", "sync", "hit",
                                 "clip", "bar", "lane", "text", "since", "notes", "degrees", "midis", "bpm", "root", "on", "delta", "style", "seventh", "beats", "kind", "limit", "zones", "label", "sub", "page"};
    static const char *argv[] = {"null", "\"x\"", "1e99", "-1e99", "[]", "{}", "true", "[[1]]", "{\"a\":1}", "0.5", "-1"};
    int refused = 0, total = 0, malformed = 0;
    for (const char *cm : cmds)
        for (const char *k : argk)
            for (const char *v : argv) {
                const QByteArray req = QByteArray("{\"cmd\":\"") + cm + "\",\"" + k + "\":" + v + "}";
                const QJsonObject rr = ctl.handle(QJsonDocument::fromJson(req).object());
                ++total;
                if (!rr.contains("ok")) ++malformed;
                else if (!rr["ok"].toBool()) ++refused;
            }
    c.call(R"({"cmd":"panic"})");
    c.call(R"({"cmd":"loop","action":"clear"})");
    wait(150);
    check(malformed == 0 && refused > total / 2, QString("%1 requests with one wrongly typed or out of range argument: every one answered, none crashed (%2 refused)").arg(total).arg(refused));
    r = c.call(R"({"cmd":"state"})");
    check(okOf(r) && inst.uiState().droppedEvents == 0, "the app is still fine, no events dropped");
    { // the output never exceeds full scale even with the abuse above
        static float x[8192];
        inst.readScope(x, 8192);
        float pk = 0; for (float v : x) pk = std::max(pk, std::fabs(v));
        check(pk <= 1.0f, "output stays inside full scale");
    }
}
} // namespace selftest
