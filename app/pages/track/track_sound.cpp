// venv-vrequency — the Track page: the sound picker for a placed clip, and hearing it alone.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/sounds/sounds.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

void TrackPage::trackSound() {
    if (soundOpen) { soundOpen = false; return; }
    if (!chosen()) { toast("Choose a placed clip first", 1400); return; }
    soundOpen = true; chopOpen = false; mixOpen = false;
    soundSetup();
}

int TrackPage::soundCount(int kind) const {
    if (kind == PK_DRUMS) return kKits;
    if (kind == PK_SYNTH) return 10 + (int)sh.soundShelf().favs.size();
    return kLibSounds;
}


Patch TrackPage::soundPatch(int kind, int i) const {
    if (kind == PK_SYNTH) return i < 10 ? sh.soundShelf().bank[qBound(0, i, 9)] : sh.soundShelf().favs[qBound(0, i - 10, (int)sh.soundShelf().favs.size() - 1)].patch;
    return librarySound(tl::libKind(kind), i);
}

// Open on the kind, and the page, that holds what the clip plays now.
void TrackPage::soundSetup() {
    Placement *p = chosen();
    if (!p) return;
    soundForLane = selLane; soundForIdx = selIdx;
    const Clip &c = *track.clips[p->clip];
    soundPage = 0;
    if (c.kind == CK_BEAT) { soundKind = PK_DRUMS; return; }
    const SoundPick &k = pickFor(c, *p);
    const QString now = QString::fromUtf8(soundName(c, k));
    const int lk = k.on ? libraryFind(k.patch.name) : -1;
    soundKind = lk == SK_KEYS ? PK_KEYS : (lk == SK_GUITAR ? PK_GUITAR : (lk == SK_BASS ? PK_BASS : PK_SYNTH));
    if (soundKind == PK_SYNTH) {
        const int n = soundCount(PK_SYNTH), per = tl::cellsPerPage(n);
        for (int i = 0; i < n; ++i)
            if (now == QString::fromUtf8(soundPatch(PK_SYNTH, i).name)) { soundPage = i / per; break; }
    }
}

// Changes what the chosen placement plays. `fn` edits a copy of the pick in force; it then
// goes to the clip (every placement) or stays with this placement, whichever is set.
void TrackPage::soundApply(const std::function<void(SoundPick &)> &fn) {
    Placement *p = chosen();
    if (!p) return;
    const int ln = selLane, i = selIdx, ci = p->clip;
    const bool only = p->pick.own || p->pick.any();
    trackApply([&](TrackData &t) {
        Placement &pl = t.lane[ln][i];
        SoundPick k = pickFor(*t.clips[ci], pl);
        fn(k);
        if (only) {
            k.own = true;
            if (samePick(k, pl.pick)) return false;
            pl.pick = k;
            return true;
        }
        k.own = false;
        if (samePick(k, t.clips[ci]->pick)) return false;
        Clip c = *t.clips[ci];
        c.pick = k;
        t.clips[ci] = std::make_shared<const Clip>(std::move(c));
        return true;
    });
    dashCache.clear(); // the clip was copied: its drawing is cached by address
}

void TrackPage::soundPickAt(int cell) {
    Placement *p = chosen();
    if (!p || cell < 0 || cell > 5) return;
    const Clip &c = *track.clips[p->clip];
    const int kind = c.kind == CK_BEAT ? PK_DRUMS : (soundKind == PK_DRUMS ? PK_SYNTH : soundKind);
    const int n = soundCount(kind), per = tl::cellsPerPage(n), pages = qMax(1, (n + per - 1) / per);
    if (n > 6 && cell == 5) { soundPage = (soundPage + 1) % pages; return; }
    const int i = qBound(0, soundPage, pages - 1) * per + cell;
    if (i >= n) return;
    if (kind == PK_DRUMS) {
        const int own = c.kit;
        soundApply([i, own](SoundPick &k) { k.kit = i == own ? -1 : i; });
    } else {
        const Patch pt = soundPatch(kind, i);
        const int oct = kind == PK_SYNTH ? 0 : libraryOctave(tl::libKind(kind));
        soundApply([&pt, oct](SoundPick &k) { k.on = true; k.patch = pt; k.octave = oct; });
    }
    if (!hearOn && !inst.uiState().trackPlaying) hearStart(); // nothing is playing: let the pick be heard
}

void TrackPage::soundKindStep(int d) {
    Placement *p = chosen();
    if (!p || track.clips[p->clip]->kind == CK_BEAT) return;
    soundKind = qBound((int)PK_KEYS, (soundKind == PK_DRUMS ? (int)PK_SYNTH : soundKind) + d, (int)PK_BASS);
    soundPage = 0;
}

// Every placement of the clip, or only this one. Either way this placement keeps its sound.
void TrackPage::soundScope() {
    Placement *p = chosen();
    if (!p) return;
    const int ln = selLane, i = selIdx, ci = p->clip;
    const bool only = p->pick.own || p->pick.any();
    const QString name = QString::fromUtf8(track.clips[ci]->name);
    trackApply([&](TrackData &t) {
        SoundPick k = pickFor(*t.clips[ci], t.lane[ln][i]);
        if (!only) { k.own = true; t.lane[ln][i].pick = k; return true; }
        k.own = false;
        for (int l = 0; l < kLanes; ++l)
            for (Placement &q : t.lane[l]) if (q.clip == ci) q.pick = SoundPick();
        Clip c = *t.clips[ci];
        c.pick = k;
        t.clips[ci] = std::make_shared<const Clip>(std::move(c));
        return true;
    });
    dashCache.clear();
    toast(only ? QString("Every %1 now plays this sound").arg(name) : QString("From here on only this %1 changes").arg(name), 1800);
}

TrackData TrackPage::hearTrack() {
    TrackData t = track;
    Placement *p = chosen();
    if (!p) return t;
    Placement one = *p;
    one.mute = false;
    for (int ln = 0; ln < kLanes; ++ln) { t.lane[ln].clear(); t.mix[ln] = LaneMix(); } // heard alone, whatever the mixer says
    t.lane[selLane].push_back(one);
    t.cycleFrom = one.bar; t.cycleTo = one.bar + one.bars;
    t.bars = qMax(t.bars, t.cycleTo);
    return t;
}

void TrackPage::hearStart() {
    Placement *p = chosen();
    if (!p) return;
    hearOn = true; hearSeen = false; hearAt = sh.clock.elapsed();
    hearLane = selLane; hearIdx = selIdx;
    inst.setTrack(std::make_shared<const TrackData>(hearTrack()));
    inst.trackDo(TC_PLAY, p->bar);
}

void TrackPage::hearEnd(bool stop) {
    if (!hearOn) return;
    hearOn = false;
    if (stop) inst.trackDo(TC_STOP);
    inst.setTrack(std::make_shared<const TrackData>(track));
}

void TrackPage::hearToggle() {
    if (hearOn) { hearEnd(true); return; }
    hearStart();
}

// Called every frame: hearing alone ends with the picker, and follows the chosen clip.
void TrackPage::hearWatch() {
    if (!hearOn) return;
    if (u.trackPlaying) hearSeen = true;
    if (sh.page() != PG_TRACK || !soundOpen || !chosen()) { hearEnd(true); return; }
    if (hearSeen ? !u.trackPlaying : sh.clock.elapsed() - hearAt > 1500) { hearEnd(false); return; } // stopped some other way
    if (selLane != hearLane || selIdx != hearIdx) hearStart();
}
