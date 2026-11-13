// venv-vrequency — the instrument: string keyboard, scale lock, backing sequencer,
// event looper, voice pool and master effects.
//
// Threading model: one real-time thread calls process(). Any other thread
// talks to it only through the lock-free queues (push / setPatch), reads state
// through uiState()/scope, or — for rare bulk operations (session save/load) —
// through snapshot()/restore(), which take a short mutex.
#pragma once
#include "drums.h"
#include "fx.h"
#include "params.h"
#include "scale.h"
#include "voice.h"
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace td {

constexpr int kStrings = 3;
constexpr int kMaxHeld = 12;
constexpr int kVoices = 28;
constexpr int kLayers = 6;
constexpr int kTrackPart0 = 2 + kLayers;  // the track's two loop lanes each get kLayers parts
constexpr int kParts = 2 + 3 * kLayers; // 0 = lead, 1 = backing, 2.. = loop layers, then the track's loop lanes
constexpr int kSlots = 12;          // voice slots per part
constexpr int kChordSlots = 5;      // backing slots 0..4 = chord voices
constexpr int kDroneSlot0 = 5;      // backing slots 5..11 = drones (degree 0..6)
constexpr int kProgMax = 8;
constexpr int kLayerEvents = 6000;
constexpr int kLayerPatches = 8;
constexpr double kMaxLoopBeats = 256; // 64 bars
constexpr int kScopeSize = 16384; // 340 ms at 48 kHz: enough for the slowest timebase

template <typename T, int N> // single-producer single-consumer ring, N power of two
class Spsc {
    T buf[N];
    std::atomic<uint32_t> w{0}, r{0};
public:
    bool push(const T &v) {
        uint32_t wi = w.load(std::memory_order_relaxed);
        if (wi - r.load(std::memory_order_acquire) >= (uint32_t)N) return false;
        buf[wi & (N - 1)] = v;
        w.store(wi + 1, std::memory_order_release);
        return true;
    }
    bool pop(T &v) {
        uint32_t ri = r.load(std::memory_order_relaxed);
        if (ri == w.load(std::memory_order_acquire)) return false;
        v = buf[ri & (N - 1)];
        r.store(ri + 1, std::memory_order_release);
        return true;
    }
    uint32_t size() const { return w.load() - r.load(); }
};

enum EvType : uint8_t {
    EV_KEY,        // a = string (0 = lowest), b = column, c = down, d = chromatic
    EV_CHORD,      // a = degree 0..6 (tap)
    EV_SUSTAIN,    // a = on/off
    EV_BEND,       // f = -1..+1
    EV_XY,         // f = x, g = y (0..1), a = touching
    EV_PANIC,
    EV_SET,        // a = Setting, f = value
    EV_PATCHPARAM, // a = Param, f = value (lead part)
    EV_LOOP,       // a = LoopCmd
    EV_BACK,       // a = BackCmd, b = argument
    EV_OCTAVE,     // a = delta (+1 / -1): held notes sweep with it
    EV_DRUM,       // a = Drum, f = velocity (0 = full): a pad hit; written into the beat while recording
    EV_BEAT,       // a = BeatCmd, b = drum, c = step
    EV_TRACK,      // a = TrackCmd, b = bar to start from
};

enum Setting {
    S_ROOT, S_SCALE, S_OCTAVE, S_ROWINT, S_BPM, S_VOLUME, S_XYMAP, S_XYHOLD,
    S_BACKMODE, S_BACKSTYLE, S_CHORDBEATS, S_ARPRATE, S_SEVENTH, S_METRO,
    S_BENDRANGE, S_BENDTIME, S_GLIDESCALE, S_BACKVOL, S_LOOPVOL, S_LATENCY,
    S_KIT, S_BEATVOL,
    S_COUNT
};

enum LoopCmd { LC_PEDAL, LC_UNDO, LC_CLEAR, LC_PLAYSTOP };
enum BackCmd { BC_PLAYSTOP, BC_CLEAR, BC_REMOVE_LAST, BC_RESYNC_SOUND, BC_SELECT_SLOT };
// BT_ERASE: b = drum (-1 = every drum). BT_STEP: b = drum, c = step, d = 0 off / 1 on / 2 toggle.
// Switching to another beat: BT_NEXTROW (b = drum, c = its sixteen steps as bits) for each drum,
// then BT_SWITCH (d = kit). Stopped, it switches at once; playing, at the start of the next bar.
enum BeatCmd { BT_PLAYSTOP, BT_REC, BT_ERASE, BT_STEP, BT_NEXTROW, BT_SWITCH };
// TC_PLAY: from bar b, round and round (or round the cycle). TC_ONCE: from bar b to the end, then stop.
enum TrackCmd { TC_PLAY, TC_ONCE, TC_STOP };
enum BackMode { BM_PROG, BM_HOLD, BM_DRONE, BM_COUNT };
enum BackStyle { BS_SUSTAIN, BS_PULSE, BS_ARP_UP, BS_ARP_DOWN, BS_ARP_UPDOWN, BS_COUNT };
enum LoopState { L_EMPTY, L_COUNTIN, L_REC, L_PLAY, L_OVERDUB, L_STOPPED };
enum XyMap { XY_FILTER, XY_WOBBLE, XY_MORPH, XY_SPACE, XY_BEND, XY_GLITCH, XY_COUNT };

inline const char *xyMapName(int m) {
    static const char *N[XY_COUNT] = {"FILTER", "WOBBLE", "MORPH", "SPACE", "BEND", "GLITCH"};
    return N[m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m)];
}
inline const char *xyAxisName(int m, bool yAxis) {
    static const char *X[XY_COUNT] = {"CUTOFF", "CUTOFF", "TIMBRE", "DELAY", "PITCH", "COARSE"};
    static const char *Y[XY_COUNT] = {"RESONANCE", "VIBRATO", "DRIVE", "REVERB", "CUTOFF", "GRIT"};
    m = m < 0 ? 0 : (m >= XY_COUNT ? XY_COUNT - 1 : m);
    return yAxis ? Y[m] : X[m];
}
inline const char *backModeName(int m) {
    static const char *N[BM_COUNT] = {"PROGRESSION", "HOLD", "DRONE"};
    return N[m < 0 ? 0 : (m >= BM_COUNT ? BM_COUNT - 1 : m)];
}
inline const char *backStyleName(int m) {
    static const char *N[BS_COUNT] = {"SUSTAIN", "PULSE", "ARP UP", "ARP DOWN", "ARP UP-DN"};
    return N[m < 0 ? 0 : (m >= BS_COUNT ? BS_COUNT - 1 : m)];
}

struct Event {
    uint8_t type = 0;
    int16_t a = 0, b = 0, c = 0, d = 0;
    float f = 0, g = 0;
};

struct Settings {
    Tuning tuning;
    float bpm = 100;
    float volume = 0.7f;
    int xyMap = XY_FILTER;
    bool xyHold = false;
    int backMode = BM_PROG;
    int backStyle = BS_SUSTAIN;
    int chordBeats = 4;
    int arpRate = 2;       // steps per beat
    bool seventh = false;
    bool metro = false;
    float bendRange = 2;   // semitones
    float bendTime = 120;  // ms
    float glideScale = 1;  // multiplies each patch's glide time
    float backVol = 0.7f;
    float loopVol = 0.9f;
    float latencyMs = 0;   // output delay (e.g. Bluetooth): loop recordings are shifted earlier by this
    int kit = KIT_SOFT;    // drum kit
    float beatVol = 0.7f;
};

enum LoopOp : uint8_t { OP_ON, OP_GLIDE, OP_OFF, OP_BEND, OP_XY, OP_PATCH, OP_PARAM };

struct LoopEv {
    double beat = 0;
    float a = 0, b = 0;
    uint32_t seq = 0;
    uint8_t op = 0, slot = 0;
};

struct LayerData {
    std::vector<LoopEv> ev;
    std::vector<Patch> patches;
};

// Everything needed to save and restore a musical session.
struct SessionData {
    Settings settings;
    Patch lead, backing;
    int prog[kProgMax] = {0};
    int progLen = 0;
    int holdChord = -1;
    int droneMask = 0;
    double loopBeats = 0;
    std::vector<LayerData> layers;
    uint16_t beat[kDrums] = {0}; // one bit per sixteenth-note step, bit 0 = step 1
};

// ---- the track: clips kept from the beat, the backing and the looper, arranged on lanes ----

enum ClipKind { CK_BEAT, CK_CHORDS, CK_LOOP };
enum Lane { LN_BEAT, LN_CHORDS, LN_LOOPA, LN_LOOPB, kLanes };
constexpr int kLaneLevelNormal = 80;
constexpr int kTrackMaxBars = 64;

// A sound picked for a clip after it was kept. It stands in for the recorded one: every layer
// of a loop plays it, a chords clip backs with it, a beat clip uses the kit. The octave moves
// the notes of a loop or chords clip, with or without a picked sound.
struct SoundPick {
    bool on = false;  // `patch` stands in for the recorded sound
    Patch patch;
    int kit = -1;     // 0 or more: this kit stands in for the clip's own
    int octave = 0;   // -2..+2
    bool own = false; // on a placement: this one goes its own way, even back to the recorded sound
    bool any() const { return on || kit >= 0 || octave != 0; }
};
bool samePick(const SoundPick &a, const SoundPick &b);

struct Clip {
    int kind = CK_BEAT;
    char name[20] = {0};
    int bars = 1;
    // beat
    uint16_t beat[kDrums] = {0};
    int kit = KIT_SOFT;
    // chords: a copy of the backing as it was
    int prog[kProgMax] = {0};
    int progLen = 0, holdChord = -1, droneMask = 0;
    int backStyle = BS_SUSTAIN, chordBeats = 4, arpRate = 2;
    bool seventh = false;
    Patch patch;
    // loop: the layers, each with its events in time order
    std::vector<LayerData> layers;
    SoundPick pick;          // for every placement of this clip that has no pick of its own
    double beats() const { return bars * 4.0; }
};

// A placed clip is played as pieces laid end to end. Uncut, it is one piece covering the clip;
// chopping makes more. Together the pieces always last exactly one pass of the clip.
struct Piece { double src = 0, len = 4; bool mute = false; };

struct Placement {
    int clip = 0;
    int bar = 0, bars = 1;   // where it sits; longer than the clip = the clip repeats
    bool mute = false;
    std::vector<Piece> pieces;
    SoundPick pick;          // for this placement only; wins over the clip's
};

// What a placement really plays: its own pick, else the clip's, else what was recorded.
const SoundPick &pickFor(const Clip &c, const Placement &pl);
const char *soundName(const Clip &c, const SoundPick &pick);   // "" when a loop has no sound at all
const char *recordedSoundName(const Clip &c);

// One lane's place in the mix. Level 80 plays a lane as recorded; 100 is about 4 dB louder.
struct LaneMix {
    int level = kLaneLevelNormal; // 0..100
    bool mute = false, solo = false;
};

struct TrackData {
    std::vector<std::shared_ptr<const Clip>> clips; // shared, never changed: copying a track is cheap
    std::vector<Placement> lane[kLanes];
    int bars = 8;
    int cycleFrom = 0, cycleTo = 0; // in bars; cycleTo <= cycleFrom means the whole track
    LaneMix mix[kLanes];
};

// What the mix makes of a lane: 0 when muted or another lane is soloed, 1 at level 80.
float laneGain(const TrackData &t, int lane);
bool laneHeard(const TrackData &t, int lane);
inline bool sameMix(const LaneMix &a, const LaneMix &b) { return a.level == b.level && a.mute == b.mute && a.solo == b.solo; }

inline int laneForKind(int kind) { return kind == CK_BEAT ? LN_BEAT : (kind == CK_CHORDS ? LN_CHORDS : LN_LOOPA); }
inline bool laneTakes(int lane, int kind) { return kind == CK_LOOP ? (lane == LN_LOOPA || lane == LN_LOOPB) : lane == laneForKind(kind); }

// Clips from what is live now (false = there is nothing of that kind to keep).
bool beatClip(const SessionData &s, Clip &out);
bool chordsClip(const SessionData &s, Clip &out);
bool loopClip(const SessionData &s, Clip &out);
bool sameClip(const Clip &a, const Clip &b);
Placement placeClip(const Clip &c, int bar);
inline int addClip(TrackData &t, const Clip &c) { t.clips.push_back(std::make_shared<const Clip>(c)); return (int)t.clips.size() - 1; }
// Arranging. Bars are whole numbers; nothing may overlap in a lane.
bool laneFree(const TrackData &t, int lane, int bar, int bars, int ignore = -1);
int placementAt(const TrackData &t, int lane, int bar);
void sortLane(TrackData &t, int lane);
// Chopping. Positions are beats inside one pass of the clip.
double piecesLength(const Placement &p);
int pieceAt(const Placement &p, double beat, double *start = nullptr);
bool cutPiece(Placement &p, double beat);            // false when that is already an edge
bool joinPieces(Placement &p, int first);            // mend the cut after piece `first`; false if they do not belong together
void cutEvery(Placement &p, double every);
void resetPieces(Placement &p, const Clip &c);
bool movePiece(Placement &p, int i, int dir);        // swap with its neighbour
bool repeatPiece(Placement &p, int i);               // the next piece plays this one's material again
double sourceAt(const Placement &p, double beat);    // where in the clip a moment of the pass reads from; -1 = muted
std::vector<Piece> rotatedPieces(const Placement &p, double offset); // the pass as heard from `offset` beats in
// Splitting a placed clip at a bar line makes two that sound the same as before; joining undoes it.
bool splitPlacement(TrackData &t, int lane, int idx, int atBar);
bool joinPlacements(TrackData &t, int lane, int idx); // with the one after it; false if they do not belong together

struct StringUi {
    bool gate = false;
    float note = 0, target = 0;
    int held = 0;      // number of keys held on this string
    int topCol = -1;   // column of the sounding key
};

struct UiState {
    StringUi str[kStrings];
    Settings set;
    double beat = 0;
    // backing
    bool backPlaying = false;
    int prog[kProgMax] = {0};
    int progLen = 0, progPos = -1, selSlot = -1;
    int holdChord = -1, droneMask = 0, curChord = -1;
    char backName[28] = {0};
    // looper
    int loopState = L_EMPTY;
    float loopPos = 0;
    int loopBars = 0, layers = 0, countBeat = 0;
    bool loopFull = false, loopClosing = false;
    // beat
    bool beatPlaying = false, beatRec = false;
    int beatStep = -1;                 // the step sounding now, -1 when stopped
    bool beatSwitching = false;        // another beat waits for the next bar
    uint16_t beatPat[kDrums] = {0};
    uint32_t drumHits[kDrums] = {0};   // counts every hit of each drum (pads light up on a change)
    // track
    bool trackPlaying = false;
    double trackPos = 0;               // beats from the start of the track
    uint32_t trackPasses = 0;          // counts play-once passes that reached the end
    // meters
    float lanePeak[kLanes] = {0};      // what each track lane is putting out (linear, held and falling)
    float peak = 0, limiter = 1;
    int voices = 0;
    bool sustain = false;
    float bend = 0, xyX = 0.5f, xyY = 0.5f;
    bool xyTouch = false;
    bool glitch = false;        // the crusher is on
    float glitchX = 0, glitchY = 0;
    char leadName[28] = {0};
    uint64_t frames = 0;
    uint32_t droppedEvents = 0;
};

class Instrument {
public:
    explicit Instrument(float sampleRate = 48000.0f);
    ~Instrument();

    // ---- any thread ----
    bool push(const Event &e) { if (evq.push(e)) return true; ++dropped; return false; }
    void key(int string, int col, bool down, bool chromatic = false) {
        Event e; e.type = EV_KEY; e.a = (int16_t)string; e.b = (int16_t)col; e.c = down; e.d = chromatic; push(e);
    }
    void set(int setting, float v) { Event e; e.type = EV_SET; e.a = (int16_t)setting; e.f = v; push(e); }
    void cmd(uint8_t type, int a = 0, int b = 0) { Event e; e.type = type; e.a = (int16_t)a; e.b = (int16_t)b; push(e); }
    void hit(int drum, float vel = 1.0f) { Event e; e.type = EV_DRUM; e.a = (int16_t)drum; e.f = vel; push(e); }
    void trackDo(int cmd, int bar = 0) { Event e; e.type = EV_TRACK; e.a = (int16_t)cmd; e.b = (int16_t)bar; push(e); }
    void beatDo(int cmd, int drum = -1, int step = 0, int arg = 2) {
        Event e; e.type = EV_BEAT; e.a = (int16_t)cmd; e.b = (int16_t)drum; e.c = (int16_t)step; e.d = (int16_t)arg; push(e);
    }
    void setPatch(int part, const Patch &p); // lock-free hand-off
    void setTrack(std::shared_ptr<const TrackData> t); // short mutex; the old track is freed on the caller's thread
    UiState uiState() const;
    void readScope(float *dst, int n) const; // most recent n mono samples

    // master output tap for WAV recording (interleaved stereo float)
    void setRecording(bool on) { recOn.store(on); }
    int readRecorded(float *dst, int maxFrames);
    uint32_t recOverruns() const { return recDrops.load(); }

    // bulk state (short mutex)
    void snapshot(SessionData &out);
    void restore(const SessionData &in);

    // ---- audio thread ----
    void process(float *outL, float *outR, int frames);
    float sampleRate() const { return sr; }

    // exposed for tests
    const Voice &voice(int i) const { return voices[i]; }
    const Part &part(int i) const { return parts[i]; }
    const Settings &settings() const { return st; }
    const Crush &crusher() const { return crush; }
    const DrumKit &drumKit() const { return drums; }

private:
    struct StringState {
        int col[kMaxHeld];
        float pitch[kMaxHeld];
        int n = 0;
        bool pendingOff = false;
    };
    struct Layer {
        std::vector<LoopEv> ev;
        int n = 0;
        Patch patches[kLayerPatches];
        int nPatches = 0;
        int cursor = 0;
    };
    struct PatchMsg { int part; Patch patch; };

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
    double loopLen = 0, loopStart = 0, recStartBeat = 0, lastPos = 0;
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
    struct LaneState { int pl = -1, piece = -1; double lastLp = -1; };
    struct LiveBacking { int prog[kProgMax]; int progLen, holdChord, droneMask, backStyle, chordBeats, arpRate; bool seventh; Patch patch; };
    std::shared_ptr<const TrackData> track;
    bool trackPlaying = false, trackOnce = false, trackDirty = false, liveSaved = false;
    double trackPos = 0;
    uint32_t trackPasses = 0;
    int trackKit = -1;                 // the kit of the beat clip that is playing, -1 = none
    LaneState laneSt[kLanes];
    LiveBacking live;
    // click
    float clickEnv = 0, clickPh = 0, clickFreq = 1500;
    long lastClickBeat = -1;

    // fx
    Chorus chorus; Delay delay; Reverb reverb; Crush crush; Limiter limiter;
    int glitchOwner = -1; // the part whose pad (live or looped) switched the crusher on
    float volSm = 0.7f;
    float sendSm[kParts][3];

    // queues & shared state
    Spsc<Event, 1024> evq;
    Spsc<PatchMsg, 16> patchq;
    std::atomic<uint32_t> dropped{0};
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
    void applyOp(int part, Layer &ly, const LoopEv &e);

    void chordTap(int degree);
    bool backingActive() const { return backPlaying && (progLen > 0 || holdChord >= 0 || droneMask != 0); }
    void tickBacking(double b0, double b1);
    void backStop();

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
    void applyTrackOp(int part, const LayerData &ld, const LoopEv &e, bool keepSound);

    void block(float *l, float *r, int n);
    void publish();
};

} // namespace td
