// venv-vrequency self-test — Track: the track comes back from the saved state and from a session.
#include "app/selftest/areas.h"
#include "app/selftest/harness.h"
#include "core/instrument/instrument.h"
#include "app/shell/mainwindow.h"
#include "app/store/store.h"
#include "app/ui/zones.h"
#include <QJsonObject>

using namespace td;

namespace selftest {
// the saved state and a session both bring the same track back
void trackSaveAndLoad(Context &ctx, TrackState &tr) {
    MainWindow &w = ctx.w;
    const TrackData &T = w.trackPage.trackData();
    w.saveState();
    QJsonObject tj; store::readJson(store::baseDir() + "/state.json", tj);
    TrackData rt; store::trackFromJson(tj["track"].toObject(), rt);
    bool rsame = rt.clips.size() == T.clips.size() && rt.bars == T.bars;
    for (int ln = 0; rsame && ln < kLanes; ++ln) {
        rsame = rt.lane[ln].size() == T.lane[ln].size();
        for (size_t i = 0; rsame && i < rt.lane[ln].size(); ++i)
            rsame = rt.lane[ln][i].bar == T.lane[ln][i].bar && rt.lane[ln][i].bars == T.lane[ln][i].bars && rt.lane[ln][i].clip == T.lane[ln][i].clip
                    && samePick(rt.lane[ln][i].pick, T.lane[ln][i].pick);
    }
    for (size_t i = 0; rsame && i < rt.clips.size(); ++i) rsame = sameClip(*rt.clips[i], *T.clips[i]) && samePick(rt.clips[i]->pick, T.clips[i]->pick);
    for (int ln = 0; rsame && ln < kLanes; ++ln) rsame = sameMix(rt.mix[ln], T.mix[ln]);
    check(rsame, "the track read back from the saved state is the same track, picked sounds and the mix included");
    check(rt.lane[LN_LOOPA][0].pick.own && QString(rt.lane[LN_LOOPA][0].pick.patch.name) == "Pluck" && QString(rt.clips[tr.loopClip]->pick.patch.name) == "Sub"
          && rt.clips[tr.beatClip]->pick.kit == tr.otherKit, "a placement's own sound, a clip's sound and a beat's kit all come back");
    QString sp2 = store::baseDir() + "/sessions/selftest-track.json";
    check(w.saveSession(sp2), "a session saves the track with it");
    const int bars0 = T.bars;
    tap(w, zn::plus(zn::track_length)); wait(40);
    check(T.bars == bars0 + 1, "the Length stepper makes the track longer");
    check(w.loadSession(sp2) && T.bars == bars0 && T.clips.size() == 3, "loading the session brings the track back");
}
} // namespace selftest
