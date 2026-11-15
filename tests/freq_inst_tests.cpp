// venv-vrequency — engine tests: the Freq tones inside the instrument: tap, delay, panic, KEYS, SNAP, sessions.
#include "tests/freq_support.h"

void testFreqInstrument() {
    SECTION("freq: in the instrument (tap, delay, panic, KEYS, SNAP, sessions)");
    {
        Rig r;
        r.inst.freqSet(-1, FF_HOLD, 1);
        r.run(2.5);
        std::vector<float> tap(kFreqScope);
        r.inst.readFreqScope(tap.data(), kFreqScope);
        CHECK(near(envelopeHz(tap, 24000), 1.5f, 0.05f) && rms(tap.data(), kFreqScope) > 0.1f, "the Freq tap holds the tones' own output (beat %.3f Hz)", envelopeHz(tap, 24000));
        CHECK(r.inst.uiState().freq.hold && r.inst.uiState().freqSounding, "HOLD shows in the published state");
        r.inst.cmd(EV_PANIC);
        r.run(0.03); r.clear(); r.run(0.1);
        CHECK(peak(r.L) < 1e-4f && !r.inst.uiState().freq.hold, "panic silences Freq and lets HOLD go (peak %.6f)", peak(r.L));
        r.inst.readFreqScope(tap.data(), 2400);
        CHECK(peak(std::vector<float>(tap.begin(), tap.begin() + 2400)) == 0.0f, "silence reaches the tap too");
        r.inst.freqSet(-1, FF_DELAY, 1); r.inst.freqSet(-1, FF_HOLD, 1); r.run(0.3);
        r.inst.freqSet(-1, FF_HOLD, 0); r.run(0.05); r.clear(); r.run(0.3);
        CHECK(rms(r.L.data(), (int)r.L.size()) > 0.005f, "DELAY sends the tones into the master delay: echoes after they stop");
    }
    {
        FreqState s; s.tone[2].on = true;
        freqMoveRoot(s, 330);
        CHECK(std::fabs(s.tone[1].hz - 331.5f) < 1e-3f && std::fabs(s.tone[2].hz - 495) < 0.01f,
              "KEYS: a close tone keeps its Hz offset (%.2f), a far one its ratio (%.2f)", s.tone[1].hz, s.tone[2].hz);
        Rig r;
        r.inst.freqSet(-1, FF_KEYS, 1); r.inst.freqSet(-1, FF_HOLD, 0); r.inst.freqSet(1, FF_ON, 0);
        r.run(0.1);
        CHECK(peak(r.L) == 0.0f, "KEYS without HOLD: silent until a key is held");
        r.inst.freqSet(-1, FF_ROOT, 440); r.inst.freqSet(-1, FF_KEYHELD, 1); r.clear(); r.run(0.3);
        CHECK(near(measureHz(r.L.data() + 4800, 9600), 440, 0.005f), "a held key plays tone 1 at its note (%.1f Hz)", measureHz(r.L.data() + 4800, 9600));
        r.inst.freqSet(-1, FF_KEYHELD, 0); r.run(0.05); r.clear(); r.run(0.05);
        CHECK(peak(r.L) < 1e-4f, "and stops when it is let go");
        int p, q;
        CHECK(freqSnapRatio(331, 220) == 330 && freqRatio(330, 220, p, q) == 2 && p == 3 && q == 2, "SNAP: 331 Hz over 220 Hz goes to 3:2");
        CHECK(std::fabs(freqSnapNote(225) - 220) < 0.01f && freqRatio(221.5f, 220, p, q) == 1 && p == 1, "tone 1 snaps to a note; 221.5 is ~1:1");
        CHECK(freqRatio(220 * 1.37f, 220, p, q) == 0, "no simple ratio near 1.37: '-'");
    }
    {
        Rig r;
        r.inst.freqSet(0, FF_HZ, 123.25f); r.inst.freqSet(2, FF_WAVE, FW_NOISE); r.inst.freqSet(-1, FF_MOD, FM_RING);
        r.inst.freqSet(-1, FF_CRUSH, 0.4f); r.inst.freqSet(-1, FF_RATE, 6); r.inst.freqSet(-1, FF_HOLD, 1);
        r.run(0.02);
        SessionData sd; r.inst.snapshot(sd);
        Rig r2; r2.inst.restore(sd); r2.run(0.02);
        const UiState u = r2.inst.uiState();
        CHECK(sameFreq(u.freq, sd.freq) && u.freq.tone[0].hz == 123.25f && u.freq.mod == FM_RING, "a session keeps the Freq settings");
        CHECK(!u.freq.hold && !u.freq.gate && peak(r2.L) == 0.0f, "but nothing sounds when it is opened");
    }
}
