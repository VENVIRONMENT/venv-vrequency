# Status — what works, what is unproven, what is missing

Version 0.12.1, 2026-11-14 (0.12.1 is 0.12.0 plus bug fixes: every armed mode (Delete, Store, Erase, track remove) clears when the page changes by any route; the key test panel swallows touches; the favourites page button goes back and forward; saving a favourite under an existing name updates it; one tempo step (1 bpm) on every page; the Backing step control is labelled Arp only because Pulse ignores it; docs corrected. Tests: 522 engine, 343 app checks. 0.12.0 is 0.11.0 plus guided help: the copilot can ring the controls it is talking about, the player answers from a chat dock on any page, and the Help page is redesigned around both. 0.11.0 was 0.10.0 twice over, merged: kept beats on the Beat page — + New starts an empty beat and keeps the others, switched on the next bar line — and a Chat page with a control socket, so an AI copilot can play and edit the app live and talk with the player. 0.9.0 was 0.8.1 plus a lane mixer on the Track page: a level, mute, solo and meter for each of the four lanes, saved with the track and heard in an export. 0.8.1 was 0.8.0 plus a fix: the scope no longer takes a whole processor core to draw while sound plays. 0.8.0: 0.7.0 plus a sound picker on the Track page: a placed clip can play as one of eighteen new Keys, Guitar and Bass sounds, any of your own sounds or another kit, with an octave shift. 0.7.0 was 0.6.0 plus a Track page for arranging kept beats, chords and loops on lanes, with chop, cycle and export; the tabs now start with Track and the Play page is called Synth. 0.6.0 added the Beat page and Play last, 0.5.0 five sine-based "dream" sounds, 0.4.0 the Glitch touch-pad mapping). "Tested" below means an automated test measured it; it does **not** mean a
person heard it or pressed a physical key. Those checks are listed separately.

## Working and covered by automated tests

`vrequency_tests` renders audio offline and measures it (522 checks). `vrequency --selftest` drives the real
application with real key and touch events while the real audio thread runs (about 452 checks; one of them, "audio backend opened", can only pass with a real audio backend).

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
| Kept beats (engine) | Stopped, a switch brings the other beat and its kit at once; playing, it waits and the bar finishes as it was, then the new beat and kit take over on the bar line with its first kick on that line; stopping the beat lets a waiting one arrive at once |
| Kept beats (app) | The beat you had becomes Beat 1; + New makes an empty beat and keeps the others; + New on an empty beat makes no second empty one; a new beat records hits of its own; Left goes back to the beat before exactly as it was, kit included, and Right returns; tapping a beat while playing waits for the bar line, then switches; with ten beats the row has a More page; every kept beat is in the state file |
| Track (engine) | A beat clip plays on the tempo grid (checked to about 20 ms), a gap is silent and the track goes round; a clip stretched over more bars repeats; chopped pieces play in their new order, a muted piece is silent, a repeated piece plays twice; loop clips play the recorded note at the right pitch on their own parts, two lanes at once, and a note still held at a clip's end is let go; a chords clip drives the backing, a cut-and-swapped one starts on its second chord, and the live backing comes back exactly when the track stops; cycle keeps the position inside its bars; play-once stops at the end; starting the track stops the live beat, and starting the live beat or a loop count-in does not restart the track; panic stops it; everything at once stays inside full scale; split at a bar line sounds the same as before and joins back |
| Track (app) | Keep copies the beat, chords and loop to the shelf once each; tap-to-place, Place, drag to move (one undo step), Repeat, Split, Join, Mute, Remove (clip removal asks twice and re-points the other clips), Cycle, Length, Undo and redo; the Chop panel's cut, mend, Halves, Move, Mute piece, Reset and Done; every Track-page and Chop key listed in the guide except `Ctrl`+`B` (Export is tested through its button); Space plays, the loop's notes and the beat's hats are counted by the engine, letters play no strings on this page, the Synth page plays over the track, `Ctrl`+`Space` stops it; Export writes a WAV of the track's length plus its tail with audio in it; the track reads back the same from the state file and from a session |
| Sound picker (engine) | A sound picked for a placement, or for a clip, replaces the recorded one while the note stays the same (measured: a recorded sine becomes a saw at the same 440 Hz); a placement's own pick wins over its clip's; the octave shift lands on 220 Hz and 1760 Hz for one down and two up; sound changes recorded inside a loop are passed over once a sound is picked; a beat with another kit picked is sample-for-sample that kit's beat; a chords clip takes a picked sound and an octave, and the live backing is back at its own pitch afterwards; split halves with different picks refuse to join. All eighteen shelf sounds sound, stay inside full scale, have their own names (none shared with a factory or dream sound) and are found by name |
| Sound picker (app) | `I` and the Sound button open it on the kind the clip plays now; arrows change kind and stop at the ends; number keys and cells pick; a pick is one undo step and leaves the recorded sound in the clip; picking while nothing plays starts the clip alone, it is heard, and the beat lane stays silent meanwhile; a bass pick drops an octave; arrows and the stepper move the octave and stop at two; Only this one / Every; Backspace and As recorded; `Space` stops Hear; `Enter` and Done close it and stop the clip; a beat clip gets only the kits and no octave; undo and redo; picks read back the same from the state file |
| Lane mixer (engine) | A new track plays every lane as recorded; a lane at 40 comes out a quarter of the level (12 dB down) for a loop lane, the chords lane and the beat lane alike, measured on the output, while the other lanes stay as they were; a muted lane is gone and its meter still; a soloed lane is the only one heard, and a muted lane stays quiet even when soloed; level 100 is 1.56 times (about 4 dB); every lane's meter moves while it plays |
| Lane mixer (app) | `X` opens it in place of the tools; number keys and Left/Right choose a lane; Up/Down move the level by five; `0` puts it back to 80; M mutes and S solos; a fader drag sets the level and is one undo step; Reset; undo and redo of every change; `Esc` closes it; opening the sound picker closes it; the mix reads back the same from the state file |
| Guided help (app) | `zones` lists every named zone of all nine pages with a rectangle and a label, and the list equals what is drawn; listing another page leaves the player where they are; every row of all four Help sections is a named zone (16/16/14/16). `highlight`: an alias (`octave`) and a prefix (`chord*`) expand; unknown names are reported while known ones highlight; a bright rule is drawn around the zone (pixel measured); the caption stays on screen and clear of the highlighted zones; it ends after its time, on `unhighlight`, and when the page changes (and does not return); `page` + highlight switch and highlight in one step; 6,800 hostile requests (wrong types, 40 names, a 500-character name, a label with control characters, out-of-range ms) are answered, none crashed. Dock: `Ctrl`+`Y` opens it with the keyboard without leaving the page; letters type and play no string; `Enter` sends with the next `seq` and wakes a waiting `chat_wait`; the message carries `page`, `focus` and `via` through `chat_poll` and `chat_wait`; `Esc` gives the keys back (a key plays again) and keeps the dock, `Esc` again closes it, then Esc is the old Esc; `Ctrl`+`Y` toggles focus; touch on the input, Send, Hide and Full; `Ctrl`+`Shift`+`Y` and the Chat page unchanged. The dock and the caption were tried over every zone shorter than 330 px of every page (more than 150) and never overlapped it: it shortens, then moves to the top; the ticker moves to the top for the same reason, and a touch on it opens the dock. The topic tag is set by a highlight, kept by a message within 5 s, and cleared by a later one or a page change. Help: Ask Rook and a tapped row open the dock with a starting question; the question goes out with the row as `focus`; all rows stay visible above the dock |
| Control socket and chat | The socket starts, is `0600`, replaces a stale file from a crashed run and refuses to take over a live one; two clients at once; replies carry the request id. A note over the socket has the right pitch (measured with the scope on the output: string 0 degree 0 = 110 Hz, a degree on the middle string follows the scale, MIDI 81 = 880 Hz); it lets go by itself after its duration; the player's own keys slide it; panic empties every voice; a slide moves the pitch. 105 requests cover every command with a good and a refused argument (note, notes, chord, slide, release, panic, set_param, get_params, sound, save_favourite, scale, tempo, octave, bend, sustain, loop, backing, beat, track including a keep/place/play/export round trip, page, session, screenshot, the chat commands); state shows what they did; the export writes a WAV; the screenshot is a 720x720 PNG. Chat: chat_say shows on the page, in the ticker on another page, as an unread count and in `state`; letters typed with synthetic key events fill the input line and play no string, Backspace edits, Enter (or the Send button) sends; a pending chat_wait is answered the moment the message is sent (15 ms measured) while other clients are served, and times out with an empty list; chat_poll returns both directions; `Ctrl`+`Y` goes to Chat and back, `Esc` leaves; the history is in the state file and only the last 200 are kept. Hostile input: garbage, a JSON array, a 40,000-digit number, a 2 MiB line, deeply nested JSON, several requests in one write, a client that disconnects mid-line, and 7,623 requests with one wrongly typed or out-of-range argument for every command: all answered, none crashed, no events dropped, output inside full scale. The socket file is removed on stop |
| Drawing and saving | With three notes sounding, every page redraws well inside a 40 ms frame (slowest in the test: Synth, about 11 ms); a kept clip is in the state file 2.5 s later without anything else being pressed |
| Listening back | Play last starts the system player on the newest recording and the same button stops it (the test does not check what comes out of the speaker) |
| Live control | XY filter and vibrato mappings, pitch bend, sustain; touch on a string plays and slides (synthetic touch events) |
| Safety | Master output cannot exceed full scale even with a deliberately abusive sound; panic frees every voice |
| Files | Favourites, sessions (key, sounds, progression, a recorded loop, the track) and settings save and load back the same; WAV export contains the audio that was played |
| Key handling | Auto-repeat does not retrigger; no input events dropped |
| Listen | Key and tempo found on synthetic test music; silence is refused rather than guessed; noise gives low confidence |

Implemented but with **no automated test**: the scroll marker and finger-drag scrolling of the Chat history, typing characters other than letters and digits (the test sends keys without text), the ticker's two-line wrap on long messages (drawn on two pictures only), `--no-control`, the real default socket path in `$XDG_RUNTIME_DIR`, loops following a tempo change, bends and XY moves inside loops,
voice stealing at the 28-voice limit (benchmarked for speed, not checked for clicks), releasing all notes
when the window loses focus, the scope's frequency readout, trigger and auto-ranging (checked by eye on rendered pictures only), the Glitch colour streaks (seen on one rendered picture only), Glitch moves replayed from inside a loop, pulse and up/down arpeggio styles, the pads lighting up when a drum sounds, the beat while a loop is recording or playing, old `keymap.json` files picking up the new key bindings, the on-screen sound editor's individual controls. On the Track page: the picture of each clip (beat dots, chord names, note dashes) and the moving play line (looked at on two rendered pictures only), the sound picker's Synth kind (your slots and favourites) and its page-turning cell, Hear while the whole track is already playing, choosing another clip while the picker is open, picked sounds inside an exported file and inside a session file (only the state file is checked), following the play line past bar 8, the shelf's second page, keeping more than 40 clips, dragging a loop between Loop A and Loop B, sound changes and touch-pad moves recorded inside a loop clip, editing the track while it plays, what an exported file sounds like (only its length and that it is not silent are checked), and Play last on an exported file.

## Measured on this device

| | |
|---|---|
| Engine cost, 28 voices + all effects | 5 % of one core (19.6× faster than real time); worst 256-frame block 0.35 ms of the 5.33 ms budget |
| Engine load while the self-test runs on the real audio thread | 1.2 % average, 15.9 % peak, 0 late blocks |
| Exporting an 8-bar track at 120 bpm (16.5 s of audio, three lanes in use) | 0.2 s |
| Drawing one frame of the Synth page | With nothing sounding 4.1 ms; with sound 6 to 12 ms (25 frames a second while anything sounds, about two a second when idle). **Before 0.8.1 a frame with sound cost 40 to 64 ms**, because the scope's trace went through Qt's path stroker: the main thread of a running 0.7.0 was measured at 97 to 103 % of a core on this device. The old 4.3 to 5.0 ms figure was taken with nothing sounding and missed it |
| Drawing one frame of any other page, with sound | About 4 ms |
| Idle CPU (app open, nothing playing) | 2.3 % of one core, measured headless with silent audio; not re-measured on the real screen since 0.1 (was about 8 %) |
| Memory | 45 MB headless; on-screen figure not re-measured since 0.1 (was about 120 MB) |
| Launch to first frame | not re-measured since 0.1 (was 0.07 s headless, about 0.3 s on screen) |
| Output latency added by venv-vrequency | one 5.3 ms block plus PipeWire's own buffering |
| Output latency of the built-in speaker | about 200 ms (Bluetooth; see HARDWARE.md) |

### Added in 0.13.0 (the tour)
Covered by `vrequency --selftest` (about 110 checks): the card file is read and validated (14 pages, 258 cards,
28 concepts; a damaged file shows an error card); every control on 18 pictures (565 hotspots, 8 are tour-only
readouts with concept cards) has a card; the tour opens from the Help button, `Ctrl`+`O` and the control socket;
touch and arrow-key selection, Tab order, page chips, guided path and replay; a long tap and key sequence leaves
the synth state untouched; Ask Rook sends via `tour` with focus; the first-run offer is shown once and
`tourSeen` and the last page persist across launches; no card overflows (2 of 286 use a 12 px font); redraw
is well inside a frame; hostile `tour` arguments are refused. The self-test can show one late audio block
while the tour draws its pictures (seen 1 of 2 runs, peak load 112% then 60%): the pictures are built on the
UI thread and compete for the CPU. It is untested on the real audio backend.

Needs a person (not verified):
- **Finger feel.** Page chips are about 46 x 20 px, Related chips about 20 px high; whether a finger hits them reliably is unproven.
- **LCD legibility.** Card text is 13 px (12 or 11 px on a few long cards) and the dimmed picture is small (380 px); readability on the real screen is judged from rendered pictures only.
- **Picture accuracy.** The pictures are drawn from demo data, not your live state; whether they look like your Synth at a glance is a judgement.
- **A real Rook.** Ask Rook and the `tour` command were exercised by the self-test's own client only.

## Needs a person — NOT verified

Added in 0.12.0 (guided help):
- **The dock on the real Q20 and screen.** Typing in the dock, `Esc` / `Esc` / `Ctrl`+`Y` with the real modifier keys, and whether `Ctrl`+`Shift`+`Y` reaches the app were tested only with synthetic key events. Whether the key to the dock feels natural while playing is unproven.
- **Legibility and feel of the highlight.** The pulsing double rule and the caption tag are checked in rendered pictures only: is the pulse noticeable but not annoying on the real LCD at arm's length, and is the caption readable at 13 px?
- **A real Rook driving it.** `highlight`, `zones` and the `page` / `focus` fields were exercised by the self-test's own client; the MCP server and the bridge that wakes Rook have not been updated or tried against them.
- **Dock over the strings.** On the Synth page the dock covers the strings or the header while open; whether that is acceptable when Rook explains while you try to play (the keys do play, the screen does not show them) needs a person.
- **Touch targets in the dock.** Hide and Full are 64 x 24 px; Help rows are 25 px high. Fingers may miss them.

Added in 0.10.0 (the copilot):
- **Typing feel on the Q20.** Typing was tested with synthetic key events only. Whether the real keyboard types
  comfortably (Shift for capitals, the symbol layer for digits and punctuation, key repeat), and whether `Ctrl`+`Y` reaches the app, is unproven.
- **The ticker at arm's length.** Two lines of 13 px raster text at the bottom of the screen for seven seconds: is it readable
  without stopping playing, and is covering the bottom row of buttons acceptable?
- **A real copilot.** The protocol was exercised only by the self-test's own client. The MCP server written by someone else has not been tried against it.

Nobody has yet confirmed any of these. See the test list at the end of this file.

1. That sound actually comes out of the speaker at a comfortable level.
2. That the physical Q20 keys reach the app as expected (the letter rows almost certainly do; the
   modifier, number-layer, arrow-layer, `$`, Esc and Tab keys are less certain).
3. How many keys the keyboard reports at once, and whether it ghosts (HARDWARE.md has the suspect combinations).
4. That touch on the real screen plays strings, pads and the XY area (tested only with synthetic pointer events).
7. That the new look reads well on the real LCD: 11–13 px raster text, the dim grey secondary labels, the faint scan lines.
8. That the Track page is usable by finger: bars are 65 pixels wide and lanes 66 high, and the Chop strip's cut lines can be as close as 43 pixels.
9. That the track sounds right: nobody has heard it, or an exported file.
11. That the new hand-drawn scope trace looks right on the real screen (compared with the old one on two rendered pictures only), and that the app's processor use really drops on the device: the 6 to 12 ms figure is from a headless run.
10. That the eighteen Keys, Guitar and Bass sounds are any good. I chose their settings without being able to listen; only their loudness and that they are not silent were measured. The guitars in particular are a synth's imitation, not a string model.
5. That the 200 ms loop timing fix feels right on the Bluetooth speaker.
6. Long-session stability (longest automated run is about a minute).

## Partial or simplified, on purpose

- **Guided help limits.** One highlight at a time (a new one replaces it); it is never saved. It rings zones that were drawn on the page being shown, so a control hidden inside a closed view (for example the Track page's mixer while the tools are showing) is unknown until that view is open. If a highlight reaches both ends of the screen the dock cannot avoid it and sits at the bottom in its smallest size over part of it. The caption may cover a neighbouring control briefly, but never the highlighted one. The dock covers the page's own bottom controls while open (Esc twice closes it). The Help page's rows are named from their description, so reworded text renames them.
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
- **Record** captures what you play from the moment you press it. Only the Track page's Export renders offline.
- **Track**: clips sit on whole bars; nothing can be placed off the bar line or made shorter than a bar except by muting chopped pieces. One Beat lane, one Chords lane, two Loop lanes. Each lane has its own level, mute and solo on the Mix strips (0.9.0). A clip's chord style is fixed when it is kept; its sound or kit can be changed afterwards with Sound. Chords clips follow the current key and scale, and all clips the current tempo; there are no tempo or key changes inside a track. Clips cannot be renamed.
- **Bluetooth speakers stutter while the Bluetooth settings window is open** (the adapter keeps scanning). Seen with a JBL Go 4 on 2026-11-13: "well for 5 s, then jittery"; closing the window cured it, confirmed by ear by the player. Not something the app can change; it is in the guide's trouble table.
- **Kept beats**: no delete, rename or copy yet; empty beats stay in the row once made (+ New will not make a second empty one in a row). A saved session holds only the beat shown. Nobody has yet used the row on the device.
- **Lane mixer**: four lanes only, no pan, no effects, no master fader (master volume stays on the System page) and no swing. The beat lane's level is set on each drum hit as it starts, so moving its fader changes the next hit, not one already ringing. The beat lane's meter shows the whole drum sound, so a live beat played over the track shows on it too. Nobody has yet heard the mixer on the device or moved a fader by finger.
- **Sound picker**: one sound for the whole clip. A loop with several layers plays all of them in the picked sound, and sound changes recorded inside it are passed over (touch-pad moves still play). The Keys, Guitar and Bass sounds are patches for the existing synth voice, not samples or a plucked-string model. A pick copies the sound into the track, so editing that slot or favourite later does not change the clip. Picking a sound cuts the notes sounding at that moment, as every edit does. Hear plays the chosen clip with everything else silent, not soloed over the rest.
- **Track and the live backing share one voice.** While the track plays, chord taps and the Backing page's play button are ignored, and the live backing is put back when the track stops.
- **One drum bus.** Pads played live over a track that has a beat clip come through that clip's kit colouring (crunch or glitch), not the Beat page's.
- **Starting in the middle** of a long loop clip (playing from a later bar, or after an edit while playing) does not sound notes that began earlier; they come in at their next note.
- **Editing while the track plays** lets go of the loop lanes' sounding notes at each edit.
- **Export** always renders the whole track once at 100 % master volume (the limiter still applies), not the cycle, and not anything played live.
- **Letters are commands on the Track page**, so the strings cannot be played from the keyboard there; several `Ctrl` shortcuts mean something else on that page (listed in the guide).
- **Beat** is one bar of sixteen steps. Hits are written by playing the pads with Rec on; the strip is a display and cannot be tapped. Every hit has the same loudness (off-beat hats are a little quieter). The loop pedal does not record the pads.
- **Drum sounds were rebuilt inside the engine** from the sketches that were auditioned, so they are close to those clips but not identical; the hi-hat repeats and snare stutters in the glitchy sketch were part of that demo's pattern and are not a feature.
- **Play last** uses the system's `pw-play` (or `aplay`); it plays the newest file only, with no list to choose from.
- **Looper layers** all share one loop length (the first recording sets it).

## Not implemented

- An in-app beginner course (asked for, to be designed after the Track page).

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
| 9i | Make a beat, tap two chords, record a loop. Track tab, **+ Keep** | Three clips appear on the left | Tell me what the message says |
| 9j | Tap Beat 1, tap bar 1 and bar 2 in the Beat lane; tap Loop 1, tap bar 1 in Loop A; `Space` | The beat and your loop play together, the line moves, it goes round after the last bar | Tell me what is missing or out of time. Nobody has heard the track on the device yet |
| 9k | Drag a placed clip sideways with a finger | It follows in whole bars | Tell me if it is too fiddly at this screen size |
| 9l | Choose the loop, **Chop**, **Beats**, tap a piece, **Move ›**, **Done**, `Space` | The loop plays in its new order, still in time | |
| 9m | With a clip chosen: `Ctrl`+`R`, `Ctrl`+`T`, `Ctrl`+`Z`, `M`, `Backspace` | Repeat, split at the line, undo, mute, delete | Tell me which keys do not arrive (the Q20's `Ctrl` and symbol keys are unproven) |
| 9n | **Export**, then System → **Play last** | A message with the file name, then you hear the whole track | |
| 9o | Choose a placed loop, **Sound**, **Guitar**, tap Pluck, Steel, Clean, Drive, Mute, Twelve in turn | The loop goes round alone and changes sound at each tap | Tell me which sounds are wrong for a guitar, too loud or too quiet. Nobody has heard them on the device |
| 9p | Same with **Keys** and **Bass**; on Bass try **Octave** − and + | Bass sounds play an octave lower; the stepper moves the whole clip | Tell me if bass is too low or too quiet on your speaker |
| 9q | **Only this one**, pick another sound, **Done**, `Space` | Only the chosen copy changed; the other copies of the clip keep theirs; the name shows beside the clip | |
| 9r | Choose a beat clip, **Sound**, tap another kit; then **As recorded** | The beat changes kit and comes back | |
| 9s | Play the track, tap **Mix**, drag the Chords fader down to about 40, then back to 80 | The chords get clearly quieter and come back, without a click; the Chords meter moves with them | |
| 9t | **M** on Beat, then **S** on Loop A | The beat stops; then only Loop A is heard. The lanes above say "muted" and "off" | |
| 9u | Set a mix, **Export**, then **Play last** on the System page | The exported file sounds like the mix you set | |
| 9v | Beat page, beat playing: **+ New**, play a few pads with Rec on, then tap **Beat 1** | The new beat starts empty on the next bar; tapping Beat 1 brings the old beat back on the next bar line, in time | |
| 9w | Quit and start the synth again | Every kept beat is still in the row | |
| 9b | Hold a low note, then a high one | The trace stays still and the ms/div readout steps by itself; pitch is unaffected | |
| 10 | `Ctrl` + `R` a few times, `Ctrl` + `Z` | New sounds each time; undo brings the last one back | |
| 11 | Menu → System → Key test, then try `Z`+`S`+`W`, `Space`+`Z`+`X`, `Alt`+`A`+`J` | Only the keys you hold are listed | A key you did not press appears: the keyboard ghosts on that combination |
| 12 | `Backspace` | Silence | |

The most useful things to report back: the "Most keys held at once" number, any phantom keys in test 11,
and whether the delay between key and sound is tolerable on the built-in speaker.
