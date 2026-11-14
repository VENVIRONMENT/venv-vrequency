// venv-vrequency — the instrument: string keyboard, scale lock, backing sequencer,
// event looper, voice pool and master effects.
//
// Threading model: one real-time thread calls process(). One other thread (the app's GUI
// thread: the queues take a single writer) talks to it through the lock-free queues
// (push / setPatch) and reads state through uiState()/readScope().
// The exception is the mutex `mx`: process() holds it for the whole of every block, and
// snapshot(), restore() and setTrack() take it on the caller's thread. While one of those
// runs, the audio thread waits. How long that is, and why it was left as it is, is in
// docs/ARCHITECTURE.md ("Audio thread and the engine lock").
#pragma once
#include "core/drums/drum_kit.h"
#include "core/fx/chorus.h"
#include "core/fx/crush.h"
#include "core/fx/delay.h"
#include "core/fx/limiter.h"
#include "core/fx/reverb.h"
#include "core/instrument/engine_types.h"
#include "core/instrument/event_port.h"
#include "core/instrument/limits.h"
#include "core/instrument/settings.h"
#include "core/instrument/ui_state.h"
#include "core/synth/params.h"
#include "core/synth/part.h"
#include "core/synth/voice.h"
#include "core/track/lanes.h"
#include <atomic>
#include <memory>
#include <mutex>

namespace td {

struct LayerData;
struct SessionData;
struct TrackData;

class Instrument : public EventPort {
public:
    explicit Instrument(float sampleRate = 48000.0f);
    ~Instrument();

    // ---- the control thread (one writer; see the top of this file) ----
    // push, key, set, cmd, note, hit, trackDo and beatDo come from EventPort (event_port.h)
    void setPatch(int part, const Patch &p); // lock-free hand-off
    void setTrack(std::shared_ptr<const TrackData> t); // takes mx; the old track is freed on the caller's thread
    UiState uiState() const;
    void readScope(float *dst, int n) const; // most recent n mono samples

    // master output tap for WAV recording (interleaved stereo float)
    void setRecording(bool on) { recOn.store(on); }
    int readRecorded(float *dst, int maxFrames);
    uint32_t recOverruns() const { return recDrops.load(); }

    // bulk state (both take mx: the audio thread waits while they copy)
    void snapshot(SessionData &out);
    void restore(const SessionData &in);

    // ---- audio thread ----
    void process(float *outL, float *outR, int frames);
    float sampleRate() const { return sr; }

    // exposed for tests
    const Voice &voice(int i) const { return voices[i]; }
    const Part &part(int i) const { return parts[i]; }
    const Crush &crusher() const { return crush; }
    const DrumKit &drumKit() const { return drums; }

private:
    float sr;
    Settings st;
    Part parts[kParts];
    float bendTarget[kParts] = {0};
    float partShift[kParts] = {0};     // semitones added to every note of a part (a track clip's octave)
    Voice voices[kVoices];
    int slotVoice[kParts][kSlots];
    float slotNote[kParts][kSlots];
    uint64_t serial = 1;

    StringState strings[kStrings];
    bool sustain = false;
    float xyX = 0.5f, xyY = 0.5f;
    bool xyTouch = false;

    // transport
    double beat = 0;
    // backing
    bool backPlaying = false;
    int prog[kProgMax] = {0};
    int progLen = 0, selSlot = -1, holdChord = -1, droneMask = 0;
    double progOrigin = 0;
    int curChord = -1, progPos = -1;
    int backDesired[kSlots];
    long lastPulseStep = -1, lastArpStep = -1;
    int arpIdx = 0, arpDir = 1;
    double pulseOffBeat = -1, arpOffBeat = -1;
    // looper
    int loopState = L_EMPTY;
    double loopLen = 0, loopStart = 0, recStartBeat = 0;
    Layer layers[kLayers];
    int nLayers = 0, recLayer = -1;
    bool closing = false, loopFull = false;
    uint32_t evSeq = 0;
    int countBeat = 0;
    // beat
    DrumKit drums;
    uint16_t beatPat[kDrums] = {0};
    bool beatPlaying = false, beatRec = false;
    long lastDrumStep = -1;
    uint16_t beatNext[kDrums] = {0};   // the beat to switch to
    int beatNextKit = 0;
    bool beatSwitching = false;
    void beatSwitchNow();
    long skipStep[kDrums];             // a step just recorded by hand: do not sound it twice
    uint32_t drumHits[kDrums] = {0};
    // track
    std::shared_ptr<const TrackData> track;
    bool trackPlaying = false, trackOnce = false, trackDirty = false, liveSaved = false;
    double trackPos = 0;
    uint32_t trackPasses = 0;
    int trackKit = -1;                 // the kit of the beat clip that is playing, -1 = none
    LaneState laneSt[kLanes];
    LiveBacking live;
    // click
    float clickEnv = 0, clickPh = 0, clickFreq = 1500;

    // fx
    Chorus chorus; Delay delay; Reverb reverb; Crush crush; Limiter limiter;
    int glitchOwner = -1; // the part whose pad (live or looped) switched the crusher on
    float volSm = 0.7f;
    float sendSm[kParts][3];

    // queues & shared state
    std::mutex mx;
    mutable std::atomic<uint32_t> uiSeq{0};
    UiState ui;
    float scope[kScopeSize];
    std::atomic<uint32_t> scopeW{0};
    std::vector<float> recBuf;
    std::atomic<uint32_t> recW{0}, recR{0}, recDrops{0};
    std::atomic<bool> recOn{false};
    float peakHold = 0;
    float laneHold[kLanes] = {0};
    int silentBlocks = 0;
    uint64_t frames = 0;

    // ---- audio-thread internals ----
    void handle(const Event &e);
    void applySetting(int s, float v);
    void keyDown(int s, int col, bool chromatic);
    void noteDown(int s, int id, float pitch, float vel);
    void keyUp(int s, int col);
    void setSustain(bool on);
    void panic();
    void shiftOctave(int delta);
    void applyXY(int part, int map, float x, float y, bool touching, bool hold);
    void setLeadPatch(const Patch &p);

    int allocVoice();
    float panFor(int part, int slot) const;
    float glideMs(int part) const;
    void opOn(int part, int slot, float pitch, float vel);
    void opGlide(int part, int slot, float pitch);
    void opOff(int part, int slot);
    void partOff(int part, bool hard);
    void leadOp(uint8_t op, int slot, float a, float b = 0);
    // One recorded event played on a part. `keepSound`: the sound changes recorded with it are passed over.
    void applyOp(int part, const LoopEv &e, const Patch *patches, size_t nPatches, bool keepSound = false);

    void chordTap(int degree);
    bool backingActive() const { return backPlaying && (progLen > 0 || holdChord >= 0 || droneMask != 0); }
    void tickBacking(double b0, double b1);

    void loopPedal();
    void loopCommit(bool atLoopEnd);
    void loopBeginLayer();
    void loopUndo();
    void loopClear();
    void loopPlayStop();
    void loopRecord(uint8_t op, int slot, float a, float b);
    void tickLoop(double b0, double b1);
    void playLayers(double p0, double p1);
    void resetLayerPart(int li);
    void quietPart(int p);                   // its notes end, its crusher, modulation and bend are let go
    void trigClick(bool accent);
    void drumHit(int drum, float vel);
    void beatCmd(int cmd, int drum, int step, int arg);
    void tickBeat(double b0, double b1);
    bool transportIdle() const { return !trackPlaying && !backingActive() && (loopState == L_EMPTY || loopState == L_STOPPED); }
    void trackStart(int fromBar, bool once);
    void trackStop();
    void tickTrack(double b0, double b1);
    void runLanes(const TrackData &t, double a, double b, double globalBeat);
    void runLane(const TrackData &t, int lane, double a, double b, double globalBeat);
    void laneOff(int lane);

    void block(float *l, float *r, int n);
    void publish();
};

} // namespace td
