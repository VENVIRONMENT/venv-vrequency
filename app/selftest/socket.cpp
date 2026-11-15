// venv-vrequency self-test — the control socket and the chat, part by part (socket_*.cpp), over one real socket.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "app/control/control.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>
#include <cstdio>

using namespace td;

namespace selftest {
void testControlSocket(Context &ctx) {
    MainWindow &w = ctx.w; Instrument &inst = ctx.inst;
    std::printf("- control socket and chat\n");
    QTemporaryDir cdir;
    const QString sock = cdir.path() + "/vrequency.sock";

    { // a leftover file from a crashed run does not block start-up
        QFile stale(sock);
        stale.open(QIODevice::WriteOnly); stale.write("not a socket"); stale.close();
        ControlServer probe(w, inst);
        check(probe.start(sock) && probe.running(), "a stale socket file is replaced at start-up");
    }
    ControlServer ctl(w, inst);
    check(ctl.start(sock) && QFile::exists(sock), "the control socket starts");
    check((QFile::permissions(sock) & (QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther | QFileDevice::ExeGroup)) == 0,
          "and is private to the user (no group or other access)");
    {
        ControlServer second(w, inst);
        check(!second.start(sock), "a second app does not take over a live socket");
    }
    Client c, c2;
    check(c.open(sock) && c2.open(sock), "two clients connect at once");
    QJsonObject r = c.call(R"({"id":"abc","cmd":"state"})");
    check(okOf(r) && r["id"].toString() == "abc", "replies carry the request id back");
    check(r["result"].toObject().contains("sound") && r["result"].toObject().contains("backing") &&
          r["result"].toObject().contains("chat_tail") && r["result"].toObject()["sound"].toObject()["params"].toObject().contains("cutoff"),
          "state holds the sound, parameters, backing and chat");

    Link link{ctl, c, c2, sock};
    socketNotes(ctx, link);
    socketEveryCommand(ctx, link);
    socketChat(ctx, link);
    socketHostileInput(ctx, link);

    c.call(R"({"cmd":"panic"})");
    ctl.stop();
    check(!QFile::exists(sock), "the socket file is removed when the server stops");
    w.setPage(PG_PLAY);
    inst.set(S_VOLUME, 0.06f);
    wait(300);
}
} // namespace selftest
