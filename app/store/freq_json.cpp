// venv-vrequency — the Freq tones as JSON, for the state file and sessions (see store.h).
//
// {"tones": [{"on": true, "wave": "sine", "hz": 220, "level": 0.8}, ...], "mod": "fm", "modAmount": 0.5,
//  "fold": 0, "cutoff": 18000, "drive": 0, "crush": 0, "delay": 0, "rate": 4, "keys": false}
// HOLD and GATE are not written: nothing sounds when a file is opened. A file without "freq" (or
// without one of these) keeps the defaults.
#include "app/store/store.h"

namespace store {

QJsonObject freqToJson(const td::FreqState &s) {
    QJsonObject o;
    QJsonArray tones;
    for (const td::FreqTone &t : s.tone) {
        QJsonObject x;
        x["on"] = t.on; x["wave"] = td::freqWaveKey(t.wave); x["hz"] = (double)t.hz; x["level"] = (double)t.level;
        tones.append(x);
    }
    o["tones"] = tones;
    o["mod"] = td::freqModKey(s.mod); o["modAmount"] = (double)s.modAmt;
    o["fold"] = (double)s.warp.fold; o["cutoff"] = (double)s.warp.cutoff;
    o["drive"] = (double)s.warp.drive; o["crush"] = (double)s.warp.crush;
    o["delay"] = (double)s.delay; o["rate"] = (double)s.rate; o["keys"] = s.keys;
    return o;
}

void freqFromJson(const QJsonObject &o, td::FreqState &s) {
    s = td::FreqState();
    const QJsonArray tones = o["tones"].toArray();
    for (int i = 0; i < td::kFreqTones && i < tones.size(); ++i) {
        const QJsonObject x = tones[i].toObject();
        td::FreqTone &t = s.tone[i];
        t.on = x["on"].toBool(t.on);
        const int w = td::freqWaveFromKey(x["wave"].toString().toUtf8().constData());
        if (w >= 0) t.wave = w;
        t.hz = (float)x["hz"].toDouble(t.hz);
        t.level = (float)x["level"].toDouble(t.level);
    }
    const int m = td::freqModFromKey(o["mod"].toString().toUtf8().constData());
    if (m >= 0) s.mod = m;
    s.modAmt = (float)o["modAmount"].toDouble(s.modAmt);
    s.warp.fold = (float)o["fold"].toDouble(s.warp.fold);
    s.warp.cutoff = (float)o["cutoff"].toDouble(s.warp.cutoff);
    s.warp.drive = (float)o["drive"].toDouble(s.warp.drive);
    s.warp.crush = (float)o["crush"].toDouble(s.warp.crush);
    s.delay = (float)o["delay"].toDouble(s.delay);
    s.rate = (float)o["rate"].toDouble(s.rate);
    s.keys = o["keys"].toBool(false);
    s.hold = s.gate = false;
    td::freqClamp(s);
}

} // namespace store
