// venv-vrequency — the Track page: its keys. Letters are commands, in the spirit of Logic.
#include "app/pages/track/track_page.h"
#include "app/pages/track/track_layout.h"
#include "app/ui/palette.h"
#include <QPainter>

using namespace td;
using namespace pal;

bool TrackPage::key(int key, int plainAct) {
    if (soundOpen && chosen() && !sh.keys.command) { // the picker has the arrows, the numbers, Space and Enter
        switch (plainAct) {
        case A_SUSTAIN: hearToggle(); return true;
        case A_LOOP: case A_HOME: soundOpen = false; return true;
        case A_PANIC: soundApply([](SoundPick &k) { k = SoundPick(); }); return true;
        case A_BEND_DOWN: soundKindStep(-1); return true;
        case A_BEND_UP: soundKindStep(1); return true;
        case A_OCT_UP: case A_OCT_DOWN: {
            const int d = plainAct == A_OCT_UP ? 1 : -1;
            if (track.clips[chosen()->clip]->kind != CK_BEAT) soundApply([d](SoundPick &k) { k.octave = qBound(-2, k.octave + d, 2); });
            return true;
        }
        default: break;
        }
        if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
            if (plainAct - A_SOUND_1 < 6) soundPickAt(plainAct - A_SOUND_1);
            return true;
        }
    }
    if (mixOpen && !sh.keys.command) { // the mixer has the arrows, 1-4, 0, M, S and Enter
        switch (key) {
        case Qt::Key_M: mixApply(mixLane, [](LaneMix &m) { m.mute = !m.mute; return true; }); return true;
        case Qt::Key_S: mixApply(mixLane, [](LaneMix &m) { m.solo = !m.solo; return true; }); return true;
        case Qt::Key_X: mixOpen = false; return true;
        }
        switch (plainAct) {
        case A_LOOP: case A_HOME: mixOpen = false; return true;
        case A_BEND_DOWN: mixLane = qMax(0, mixLane - 1); return true;
        case A_BEND_UP: mixLane = qMin(kLanes - 1, mixLane + 1); return true;
        case A_OCT_UP: case A_OCT_DOWN: {
            const bool up = plainAct == A_OCT_UP; // to the next step of 5
            mixApply(mixLane, [up](LaneMix &m) { m.level = up ? qMin(100, (m.level / 5 + 1) * 5) : qMax(0, ((m.level + 4) / 5 - 1) * 5); return true; });
            return true;
        }
        default: break;
        }
        if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
            const int k = plainAct - A_SOUND_1;
            if (k < kLanes) mixLane = k;
            else if (plainAct == A_SOUND_10) mixApply(mixLane, [](LaneMix &m) { m.level = kLaneLevelNormal; return true; });
            return true;
        }
    }
    if (sh.keys.command) {
        switch (key) {
        case Qt::Key_T:
            if (!chopOpen) trackSplit();
            else chopEdit([this](Placement &x) { // the chosen piece, cut in the middle
                if (chopPiece >= (int)x.pieces.size()) return false;
                double st = 0;
                for (int i = 0; i < chopPiece; ++i) st += x.pieces[i].len;
                const double gr = tl::chopGrid(*track.clips[x.clip]);
                const double mid = std::round((st + x.pieces[chopPiece].len / 2) / gr) * gr;
                if (!cutPiece(x, mid)) { toast("That piece is too short to cut again", 1600); return false; }
                return true;
            });
            return true;
        case Qt::Key_J:
            if (!chopOpen) trackJoin();
            else chopEdit([this](Placement &x) {
                if (!joinPieces(x, chopPiece)) { toast("It only mends with the piece that follows it in the clip", 2200); return false; }
                return true;
            });
            return true;
        case Qt::Key_R:
            if (!chopOpen) trackRepeat();
            else { chopEdit([this](Placement &x) { return repeatPiece(x, chopPiece); }); if (Placement *p = chosen()) chopPiece = qMin(chopPiece + 1, (int)p->pieces.size() - 1); }
            return true;
        case Qt::Key_X: trackCopy(true); return true;
        case Qt::Key_C: trackCopy(false); return true;
        case Qt::Key_V: trackPlaceAtLine(); return true;
        case Qt::Key_Z: if (sh.keys.chromatic) trackRedoDo(); else trackUndoDo(); return true;
        case Qt::Key_B: trackExport(); return true;
        }
        return false; // every other command key keeps its usual meaning
    }
    switch (plainAct) {
    case A_SUSTAIN: trackPlayStop(); return true;
    case A_LOOP:
        playBar = 0; viewBar = 0;
        if (inst.uiState().trackPlaying) inst.trackDo(TC_PLAY, track.cycleTo > track.cycleFrom ? track.cycleFrom : 0);
        return true;
    case A_PANIC:
        if (chosen() && !chopOpen) { trackRemove(); return true; }
        return false;
    case A_HOME:
        if (chopOpen) { chopOpen = false; return true; }
        if (handOn) { handOn = false; return true; }
        return false;
    case A_BEND_DOWN: case A_BEND_UP: {
        const int d = plainAct == A_BEND_UP ? 1 : -1;
        if (chopOpen) {
            Placement *p = chosen();
            const int to = chopPiece + d;
            if (!p || to < 0 || to >= (int)p->pieces.size()) return true;
            if (sh.keys.chord) { const int pc = chopPiece; chopEdit([pc, d](Placement &x) { return movePiece(x, pc, d); }); }
            chopPiece = to;
        } else if (sh.keys.chromatic) trackLine(d);
        else if (sh.keys.chord) trackMove(d, 0);
        else trackSelect(d, 0);
        return true;
    }
    case A_OCT_UP: case A_OCT_DOWN: {
        const int d = plainAct == A_OCT_DOWN ? 1 : -1;
        if (!chopOpen) { if (sh.keys.chord) trackMove(0, d); else trackSelect(0, d); }
        return true;
    }
    default: break;
    }
    if (plainAct >= A_SOUND_1 && plainAct <= A_SOUND_10) {
        const int k = plainAct - A_SOUND_1;
        if (k < tl::SHELF) trackTake(shelfPage * tl::SHELF + k);
        return true;
    }
    switch (key) {
    case Qt::Key_Comma: trackLine(-1); return true;
    case Qt::Key_Period: trackLine(1); return true;
    case Qt::Key_M:
        if (chopOpen) chopEdit([this](Placement &x) { if (chopPiece >= (int)x.pieces.size()) return false; x.pieces[chopPiece].mute = !x.pieces[chopPiece].mute; return true; });
        else trackMute();
        return true;
    case Qt::Key_C: trackCycle(); return true;
    case Qt::Key_E: trackChop(); return true;
    case Qt::Key_I: trackSound(); return true;
    case Qt::Key_K: trackKeep(); return true;
    case Qt::Key_X: trackMixer(); return true;
    }
    int row, col;
    return sh.km.stringKey(key, row, col); // the other letters do nothing here: no stray notes
}
