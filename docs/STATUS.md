# Status — what works, what is unproven, what is missing

Version 0.6.0, 2026-11-13 (0.5.0 plus a Beat page with eight drum pads in three kits, and a Play last button for listening back to a recording; 0.5.0 added five sine-based "dream" sounds, 0.4.0 the Glitch touch-pad mapping). "Tested" below means an automated test measured it; it does **not** mean a
person heard it or pressed a physical key. Those checks are listed separately.

## Working and covered by automated tests

`vrequency_tests` renders audio offline and measures it (396 checks). `vrequency --selftest` drives the real
application with real key and touch events while the real audio thread runs (69 checks).

| Area | What the tests establish |
|---|---|
| Startup | Audio stream opens on PipeWire and runs in real time; every page renders at 720×720 with every one of its controls inside the screen |
| Notes | Key-to-note tables are right; a key press produces the right frequency (measured: A2 = 110 Hz); a tap shorter than one audio block still sounds |
| Slides | A second key on a held string glides continuously and monotonically, with the same voice and no re-attack; releasing the newer key slides back; glide speed setting is honoured |
| Chord glides | Three strings sound together; re-fingering a held shape glides all three voices to the new chord |
| Scale lock | For every scale and every root, every key is in the scale and pitch rises left to right; chromatic override gives semitones; octave shift sweeps held notes without retriggering |
| Sounds | The 10 factory sounds differ measurably; switching sound under a held note makes no click |
| Dream sounds | Haze, Fuzz, Teeth, Bend and Grain each keep more than 85 % of their energy in the fundamental sine when measured dry (Warm Lead: under 80 %); three held notes stay inside full scale; each rings on after release; they appear as five favourites on first start, in order, and are not added twice |
| Randomiser | 120 generated sounds: all audible, all different, none over the level ceiling; mutate stays close; undo and favourites work through the app |
| Backing | Diatonic triads and sevenths named and built correctly; a tapped I–IV–V–IV progression loops in tempo under a live melody; remove / replace / stop / start / clear; hold and drone modes; arpeggio; backing keeps its sound when the lead changes |
| Looper | Count-in, bar snap, playback, overdub, undo, stop/start, clear; a recorded slide replays as a slide; an 18-second loop; a layer keeps the sound it was recorded with |
| Glitch | With the pad down the wave becomes flat steps with only a few levels left, the note is still present, loudness stays in range and inside full scale; the grit is the same at low volume; lifting the finger, changing mapping or panic restores the clean sound; Hold latches it |
| Beat | All 24 drum sounds exist, end in silence and differ between kits; a pad hit sounds inside 30 ms; steps fall on the tempo grid (checked to about 20 ms) and follow a tempo change; hits played with Rec on land on the nearest step, are not sounded twice, and play back; a 200 ms output delay is compensated; erase, stop and panic work; starting the backing does not restart the bar; every drum on every step stays inside full scale; pads, pad keys, Rec, Erase, kit, Space and Enter work through the app; the beat is saved in the state file and in sessions |
| Listening back | Play last starts the system player on the newest recording and the same button stops it (the test does not check what comes out of the speaker) |
| Live control | XY filter and vibrato mappings, pitch bend, sustain; touch on a string plays and slides (synthetic touch events) |
| Safety | Master output cannot exceed full scale even with a deliberately abusive sound; panic frees every voice |
| Files | Favourites, sessions (key, sounds, progression, loop) and settings save and load back the same; WAV export contains the audio that was played |
| Key handling | Auto-repeat does not retrigger; no input events dropped |
| Listen | Key and tempo found on synthetic test music; silence is refused rather than guessed; noise gives low confidence |

Implemented but with **no automated test**: loops following a tempo change, bends and XY moves inside loops,
voice stealing at the 28-voice limit (benchmarked for speed, not checked for clicks), releasing all notes
when the window loses focus, the scope's frequency readout, trigger and auto-ranging (checked by eye on rendered pictures only), the Glitch colour streaks (seen on one rendered picture only), Glitch moves replayed from inside a loop, pulse and up/down arpeggio styles, the pads lighting up when a drum sounds, the beat while a loop is recording or playing, old `keymap.json` files picking up the new key bindings, the on-screen sound editor's individual controls.

## Measured on this device

| | |
|---|---|
| Engine cost, 28 voices + all effects | 5 % of one core (19.6× faster than real time); worst 256-frame block 0.35 ms of the 5.33 ms budget |
| Engine load while the self-test runs on the real audio thread | 1.3 % average, 7.5 % peak, 0 late blocks |
| Drawing one frame of the play page | 4.8 ms (25 frames a second while anything sounds, about two a second when idle) |
| Idle CPU (app open, nothing playing) | 2.3 % of one core, measured headless with silent audio; not re-measured on the real screen since 0.1 (was about 8 %) |
| Memory | 45 MB headless; on-screen figure not re-measured since 0.1 (was about 120 MB) |
| Launch to first frame | not re-measured since 0.1 (was 0.07 s headless, about 0.3 s on screen) |
| Output latency added by venv vrequency | one 5.3 ms block plus PipeWire's own buffering |
| Output latency of the built-in speaker | about 200 ms (Bluetooth; see HARDWARE.md) |

## Needs a person — NOT verified

Nobody has yet confirmed any of these. See the test list at the end of this file.

1. That sound actually comes out of the speaker at a comfortable level.
2. That the physical Q20 keys reach the app as expected (the letter rows almost certainly do; the
   modifier, number-layer, arrow-layer, `$`, Esc and Tab keys are less certain).
3. How many keys the keyboard reports at once, and whether it ghosts (HARDWARE.md has the suspect combinations).
4. That touch on the real screen plays strings, pads and the XY area (tested only with synthetic pointer events).
7. That the new look reads well on the real LCD: 11–13 px raster text, the dim grey secondary labels, the faint scan lines.
5. That the 200 ms loop timing fix feels right on the Bluetooth speaker.
6. Long-session stability (longest automated run is about a minute).

## Partial or simplified, on purpose

- **Black and white only.** States that used colour before are now shown by inversion (selected, sounding, recording),
  a double frame (armed: "pick a slot", a chord slot chosen for replacement, a drone note) or blinking (overdub, WAV recording).
- **The Freq readout is the waveform's true repeat rate.** A sound with a sub-oscillator reads an octave below the note name
  shown beside it (Warm Lead playing A3 reads 110 Hz). That is what a real scope would show.
- **The scope ranges itself.** There are no time/div or volts/div controls; the scales step automatically after the picture
  has been wrong for about a third of a second.
- **Three strings, not four.** The Q20 has three letter rows. Chords live on `Alt` + bottom row, the
  on-screen pads, or the `$` chord-row toggle.
- **Bottom string has 7 keys, middle 9, top 10.** The strings still line up in pitch columns.
- **Bluetooth latency** is compensated for loop recording only. Live playing on the built-in speaker
  has an audible delay that software cannot remove.
- **Listen** is implemented but has no input to listen to on this hardware.
- **Windowed mode** (`--windowed`) gets an odd size from GNOME on this screen; full screen is the default and is what was designed.
- **Key remapping** is by editing `~/vrequency/keymap.json`; there is no on-screen editor.
- **WAV export** records what you play from the moment you press record; it cannot render a loop offline after the fact.
- **Beat** is one bar of sixteen steps. Hits are written by playing the pads with Rec on; the strip is a display and cannot be tapped. Every hit has the same loudness (off-beat hats are a little quieter). The loop pedal does not record the pads.
- **Drum sounds were rebuilt inside the engine** from the sketches that were auditioned, so they are close to those clips but not identical; the hi-hat repeats and snare stutters in the glitchy sketch were part of that demo's pattern and are not a feature.
- **Play last** uses the system's `pw-play` (or `aplay`); it plays the newest file only, with no list to choose from.
- **Looper layers** all share one loop length (the first recording sets it).

## Not implemented

- Tilt / motion control (no sensor in the device).
- MIDI in or out.
- Audio-rate looping (the looper stores notes and gestures, not sound; that is deliberate).
- Launch at boot (not requested).
- A fourth "virtual" string.

## What I changed on the system

- Connected the already-paired Bluetooth speaker `XWF-M18-M28-M38`, which made it the default audio output (and reconnected it once after the device restarted). Nothing else about audio was touched.
- The app was first installed as Tendril; that install was removed with its own uninstall script before the rename. No `~/Tendril` data folder had been created.
- Installed for this user only: `~/.local/opt/vrequency/`, `~/.local/bin/vrequency`, `~/.local/share/applications/vrequency.desktop`, an icon under `~/.local/share/icons/`. User data goes in `~/vrequency/`.
- No packages installed, no system files edited, no services enabled. `scripts/uninstall.sh` removes all of the above.
- During development a few test windows briefly appeared and took keyboard focus; later tests ran headless.

## Please test (about five minutes)

Start with `vrequency`. If there is no sound, check that GNOME's sound output is the built-in speaker.

| # | Do | Expect | If not |
|---|---|---|---|
| 1 | Press `A`, `S`, `D` one at a time | Three rising notes, comfortable volume | Note what Menu → System says under Output |
| 2 | Hold `A`, press and hold `F`, let `F` go | Pitch slides up, then back down, no second attack | |
| 3 | Hold `Z`, `S`, `E` together | Three-note chord | Open Key test, see which key is missing |
| 4 | Hold `Shift`, play `A S D F` | Semitone steps | Key test: does Shift appear as held? |
| 5 | Hold `Space`, tap `Q W E`, release `Space` | Notes ring until Space is released | |
| 6 | `Alt` + `Z`, then `Alt` + `V` | Backing chords I and IV start looping | Use the chord pads on screen instead; tell me what Alt does |
| 7 | Number layer + `2`, `3`, `4` | Sound changes | Tell me which key is the layer key on your unit |
| 8 | `Enter`, wait for the count, play a bar, `Enter` again | Your phrase loops in time | If it is early or late, change System → Loop fix and tell me the value that works |
| 9 | Drag a finger across the scope | Sound changes with the finger; a square cursor follows it | |
| 9c | Tap the Filter button on the scope until it says Glitch, hold a note, drag on the scope | Sound turns rough and digital while touched, coloured streaks trail from the wave, both stop when you lift | Tell me if it is too harsh, too quiet, or the colours are wrong |
| 9d | Sound page, tap Haze (then Fuzz, Teeth, Bend, Grain) in Favourites, hold notes and slide | Soft, slow, washed sound; the scope draws a round wave | Tell me which are too quiet, too washed out or not buzzy enough. Nobody has heard these on the device yet |
| 9e | Beat tab. Tap each pad, then press `Q W E R` and `A S D F` | Eight different drum sounds, each pad flashes | Tell me which sounds are wrong or too loud or quiet. Nobody has heard the drums on the device yet |
| 9f | Beat tab: `Enter`, play kick and snare in time with the click for two bars, `Enter` again | Your hits appear in the strip and keep playing | If hits land a step early or late, change System → Loop fix and tell me the value that works |
| 9g | Kit + on the Beat page while the beat plays | The same beat with crunchy, then glitchy sounds | |
| 9h | System → Record, play a little, Record again, then Play last | You hear what you just played | Tell me what the message on screen says |
| 9b | Hold a low note, then a high one | The trace stays still and the ms/div readout steps by itself; pitch is unaffected | |
| 10 | `Ctrl` + `R` a few times, `Ctrl` + `Z` | New sounds each time; undo brings the last one back | |
| 11 | Menu → System → Key test, then try `Z`+`S`+`W`, `Space`+`Z`+`X`, `Alt`+`A`+`J` | Only the keys you hold are listed | A key you did not press appears: the keyboard ghosts on that combination |
| 12 | `Backspace` | Silence | |

The most useful things to report back: the "Most keys held at once" number, any phantom keys in test 11,
and whether the delay between key and sound is tolerable on the built-in speaker.
