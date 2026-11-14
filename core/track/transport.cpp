// venv-vrequency — the track's transport: taking a new track, starting, stopping, and following the beat clock.
#include "core/instrument/instrument.h"
#include "core/track/track.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>

namespace td {

void Instrument::setTrack(std::shared_ptr<const TrackData> t) {
    {
        std::lock_guard<std::mutex> lk(mx);
        track.swap(t);
        trackDirty = true;
    }
    // the previous track, now in `t`, is released here on the caller's thread
}

void Instrument::trackStart(int fromBar, bool once) {
    if (!track) return;
    // the track takes over the transport: stop what was playing live
    beatPlaying = beatRec = false;
    if (loopState == L_COUNTIN || loopState == L_REC || closing) loopClear();
    else if (loopState == L_PLAY || loopState == L_OVERDUB) loopPlayStop();
    if (!liveSaved) { // the chords lane borrows the backing; remember what was there
        for (int i = 0; i < kProgMax; ++i) live.prog[i] = prog[i];
        live.progLen = progLen; live.holdChord = holdChord; live.droneMask = droneMask;
        live.backStyle = st.backStyle; live.chordBeats = st.chordBeats; live.arpRate = st.arpRate; live.seventh = st.seventh;
        live.patch = parts[1].patch;
        liveSaved = true;
    }
    backPlaying = false; selSlot = -1;
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    int bars = std::clamp(track->bars, 1, kTrackMaxBars);
    trackPos = std::clamp(fromBar, 0, bars - 1) * 4.0;
    beat = trackPos; progOrigin = 0;
    lastDrumStep = (long)std::floor(beat * 4.0) - 1;
    lastPulseStep = lastArpStep = -1;
    trackPlaying = true; trackOnce = once; trackDirty = false;
}

void Instrument::trackStop() {
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    trackPlaying = trackOnce = false;
    trackKit = -1;
    if (liveSaved) {
        for (int i = 0; i < kProgMax; ++i) prog[i] = live.prog[i];
        progLen = live.progLen; holdChord = live.holdChord; droneMask = live.droneMask;
        st.backStyle = live.backStyle; st.chordBeats = live.chordBeats; st.arpRate = live.arpRate; st.seventh = live.seventh;
        parts[1].patch = live.patch;
        liveSaved = false;
    }
    backPlaying = false;
    partShift[1] = 0;
}

void Instrument::runLanes(const TrackData &t, double a, double b, double g) {
    if (b <= a) return;
    for (int ln = 0; ln < kLanes; ++ln) runLane(t, ln, a, b, g);
}

void Instrument::tickTrack(double b0, double b1) {
    if (!trackPlaying) return;
    const TrackData *t = track.get();
    if (!t) { trackStop(); return; }
    const double total = std::clamp(t->bars, 1, kTrackMaxBars) * 4.0;
    if (trackDirty) { // the arrangement changed under us: let go and pick up where we are
        for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
        trackDirty = false;
        if (trackPos >= total) trackPos = 0;
    }
    const bool cycle = !trackOnce && t->cycleTo > t->cycleFrom && t->cycleFrom >= 0 && t->cycleTo * 4.0 <= total;
    double end = total, start = 0;
    if (cycle) {
        start = t->cycleFrom * 4.0;
        if (trackPos < t->cycleTo * 4.0) end = t->cycleTo * 4.0;
    }
    const double a = trackPos, b = a + (b1 - b0);
    if (b < end) {
        runLanes(*t, a, b, b0);
        trackPos = b;
        return;
    }
    runLanes(*t, a, end, b0);
    if (trackOnce) { ++trackPasses; trackStop(); return; }
    for (int ln = 0; ln < kLanes; ++ln) laneOff(ln);
    const double rest = b - end;
    runLanes(*t, start, start + rest, b0 + (end - a));
    trackPos = start + rest;
}

} // namespace td
