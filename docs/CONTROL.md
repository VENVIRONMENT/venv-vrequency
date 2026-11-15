# The control socket

venv-vrequency opens a local Unix socket so an AI copilot (an agent such as Claude Code, through an MCP
server that speaks this protocol) can play the synth, edit it and chat with the player in the app's Chat
page. It is **on by default**; start with `vrequency --no-control` to switch it off.

- **Where**: `$XDG_RUNTIME_DIR/vrequency.sock`; if that is not set, `$VREQUENCY_HOME/control.sock` (default
  `~/vrequency/control.sock`). `$VREQUENCY_CONTROL_SOCKET` overrides both.
- **Who**: a Unix socket with permissions `0600`: only the same user can connect. There is no network
  listener of any kind. The file is removed when the app quits. A leftover file from a crashed run is
  replaced at the next start; a socket something still answers on is never taken over.
- **How**: newline-delimited JSON, one object per line, in both directions. Up to 8 clients at once. A request
  line longer than 1 MiB is refused.

```
-> {"id":1,"cmd":"note","string":1,"degree":2,"dur_ms":400}
<- {"id":1,"ok":true,"result":{"played":[{"string":1,"midi":57,"hz":220.0,"id":1024}]}}
-> {"id":2,"cmd":"note","string":9,"degree":0}
<- {"id":2,"ok":false,"error":"'string' must be between 0 and 2"}
```

`id` can be any JSON value and is returned unchanged (`null` if the request had none or was not valid JSON).
Every request is answered, whatever it contains.

**Safety.** Every command runs on the app's GUI thread and reaches the audio thread through the same lock-free
queue the keyboard uses; the audio thread is never blocked. Every argument is validated (type and range);
a bad one gets an error and changes nothing. Numbers are never clamped silently except where a command
says so (`set_param`). Sound goes through the engine's own limits: voice limit of 28, the limiter and soft
clip stay on the output. Changes are applied by the audio thread a few milliseconds after the reply, so
`state` read straight after a command may still show the old value.

Strings are numbered **0 = lowest** (the bottom key row), **1 = middle**, **2 = highest**. A *degree* is a
step of the current scale counted from the string's first key: `0` is the string's first key (`Z`, `A` or
`Q`), `1` its second, `-1` the note below, `5` can be past an octave. A *midi* note is an exact pitch
(12 to 120) that ignores the scale. Notes played over the socket are separate from the player's keys on
the same string: both can sound, and a key pressed on a sounding string slides it, as always.

## Commands

### state
Everything at a glance: `page`, `sound` (name, `slot` 1-10 or null, `slots`, `favourites`, `params` by key),
`scale` (name, index, root, octave), `tempo`, `volume`, `looper`, `pulse` (`sync`, and `pads`: for each of the eight
`type`, `tone`, `length`, `rate`, `level`, `drive`, `fold`, `crush`, `bend`, `latched`), `backing` (`playing`, `mode`, `style`, `seventh`, `chords`, `degrees`, `position`), `playing`, `voices`, `toast` (the last message the app showed), `chat`
(`listening`, `last_seq`, `unread`) and `chat_tail` (last ten messages).
```
{"id":1,"cmd":"state"}
```

### note
Play one note. `string` 0-2 required; exactly one of `degree` (-48..48) or `midi` (12..120); `dur_ms`
20..30000 (default 500); `velocity` 0..1 (default 1).
```
{"id":2,"cmd":"note","string":0,"degree":0,"dur_ms":800}
{"id":3,"cmd":"note","string":2,"midi":81,"velocity":0.6}
```

### notes / chord
Several notes at once. `notes`: a list of objects like `note`'s (each may have its own `dur_ms` and `velocity`;
defaults at the top level). Or `chord` with `degrees` or `midis`: up to 3 values, put on strings 0, 1, 2 in order.
```
{"id":4,"cmd":"notes","notes":[{"string":0,"degree":0},{"string":2,"midi":72}],"dur_ms":600}
{"id":5,"cmd":"chord","degrees":[0,2,4],"dur_ms":1200}
```

### slide
Play `from`, then glide to `to` on the same string (at the sound's own glide time). `string`, `from`, `to`
required; `unit` `"degree"` (default) or `"midi"`; `ms` 50..30000 total (default 600); `velocity`.
```
{"id":6,"cmd":"slide","string":1,"from":0,"to":3,"ms":900}
```

### release / panic
`release` lets go of the notes played over the socket (optionally one `string`). `panic` silences everything,
as `Backspace` does (also drops every pulse latch and stops the backing and the loop).
```
{"id":7,"cmd":"release","string":1}
{"id":8,"cmd":"panic"}
```

### set_param / get_params
`set_param`: `name` is a key from `get_params` (`cutoff`, `attack`, `wave1`, `reverbmix`, the warp keys `fold`, `crush`,
`downsample`, `ring`, `bendin`...), `value` is
**clamped** to the parameter's range (stepped ones rounded); the reply says the value used and whether it was
clamped. `get_params` lists every parameter with its value, range and default.
```
{"id":9,"cmd":"set_param","name":"cutoff","value":1200}
{"id":10,"cmd":"get_params"}
```

### sound
`slot` 1-10, or `name`: a favourite's name, `"random"`, `"mutate"` or `"undo"`.
```
{"id":11,"cmd":"sound","slot":2}
{"id":12,"cmd":"sound","name":"Haze"}
{"id":13,"cmd":"sound","name":"mutate"}
```

### save_favourite
Saves the current sound as a favourite under `name` (1-24 characters). A favourite that already has exactly that name is updated in place (the reply has `"updated":true`).
```
{"id":14,"cmd":"save_favourite","name":"Rook's pad"}
```

### scale
`name` (major, minor, majpent, minpent, blues, dorian, mixolydian, chromatic, or the full names such as
"Minor pentatonic") and/or `root` (a note name `A`, `C#`, `Bb`, or 0-11).
```
{"id":15,"cmd":"scale","name":"dorian","root":"D"}
```

### tempo, octave, bend, sustain
`tempo`: `bpm` 40-240. `octave`: `value` -3..3 or `delta`. `bend`: `value` -1..1 (held until changed), optional
`ms` to spring back after that long. `sustain`: `on` true/false.
```
{"id":16,"cmd":"tempo","bpm":96}
{"id":17,"cmd":"octave","delta":1}
{"id":18,"cmd":"bend","value":0.5,"ms":400}
{"id":19,"cmd":"sustain","on":true}
```

### loop
`action`: `record` (the pedal: count-in, record, or finish/overdub), `play`, `stop`, `clear`, `undo`.
`stop` on a loop that is not playing and `play` with nothing recorded are refused.
```
{"id":20,"cmd":"loop","action":"record"}
```

### backing
`chords`: a new progression of 1-8 chords, each 1-7 or a numeral `"I"`..`"VII"` (replaces the old one and
starts it). Or `action`: `play`, `stop`, `clear`, `remove_last`. Optional: `style` (sustain, pulse, arp up,
arp down, arp up-dn), `seventh` (bool), `beats` per chord 1-8.
```
{"id":21,"cmd":"backing","chords":["I","vi",4,5],"style":"pulse"}
{"id":22,"cmd":"backing","action":"stop"}
```

### pulse
The pulse pads. Any of: `type` (click, metal, snap, scratch, clap, tom, thump, 808), `tone` (Hz, 20-18000),
`length` (ms, 1-2000), `rate` (Hz, 0.25-200), `level` (0-1), the warp: `drive`, `fold`, `crush` (each 0-1, 0 = off)
and `bend` (semitones the pitch, or a noise kind's band, moves across the hit, -24 to 24, 0 = off), `latch` (true / false), `hit` (true: plays it now)
for one pad, `pad` 0-7 (required with any of those); and `sync` (true / false: every pad's rate follows the
tempo). Settings are applied before `latch` and `hit`. What each field means per kind is in GUIDE chapter 8.
```
{"id":23,"cmd":"pulse","pad":5,"type":"tom","tone":1000,"length":250,"rate":40,"level":0.8,"hit":true}
{"id":24,"cmd":"pulse","pad":4,"latch":true}
{"id":26,"cmd":"pulse","pad":6,"drive":0.6,"fold":0.3,"bend":-12,"hit":true}
{"id":25,"cmd":"pulse","sync":true}
```
The `beat` command of earlier versions is gone (the step sequencer was removed in 1.0); it is refused as unknown.

### page
`name`: synth, sound, pulse, freq, backing, tune, system, help or chat (`beat`, the old name, still opens Pulse). Changing page clears any highlight.
```
{"id":28,"cmd":"page","name":"chat"}
```

### session
`action` save or load, `name` (letters, digits, `-`, `_`), in the app's sessions folder.
```
{"id":29,"cmd":"session","action":"save","name":"jam-1"}
```

### screenshot
Writes a 720x720 PNG of the screen to the temporary folder and returns its `path`. The newest six are kept.
```
{"id":30,"cmd":"screenshot"}
```

## Chat

Messages have a `seq` (1, 2, 3...), `from`, `text`, `kind` (`say`, `tip`, `ask`), `ts` (ms since the epoch) and
`user` (true when the player typed it, who is shown as `you`). The last 200 are kept in the state file, so the
history survives restarts and the numbering carries on.

### chat_say
`text` 1-2000 characters; `from` (default `"rook"`, up to 16 characters; `"you"` is the player and refused);
`kind` `say` (default), `tip` or `ask`. Shows on the Chat page and, while the player is on another page, in a
two-line ticker at the bottom of the screen for a few seconds (it takes no key; touching it opens the chat dock). The `Ctrl`+`Y` hotkey now opens the dock, see below.
```
{"id":31,"cmd":"chat_say","from":"claude","kind":"ask","text":"Want a slower tempo?"}
```

### chat_poll
`since` (default 0), optional `limit`. Returns every message with `seq` > `since`, from both sides, and
`last_seq`. Counts as "the copilot is listening" for 45 s.
```
{"id":32,"cmd":"chat_poll","since":12}
```

### chat_wait
Long poll: `since`, `timeout_ms` 0-30000 (default 25000). The reply comes as soon as a message **typed by the player**
with `seq` > `since` exists, with `timed_out` false; otherwise at the timeout with `timed_out` true and an
empty `messages` list. Nothing else is blocked meanwhile, and several clients can wait at once.
```
{"id":33,"cmd":"chat_wait","since":14,"timeout_ms":20000}
```
While any chat_wait is pending, the Chat page says `copilot: listening`.

### tour
Opens or closes the player's tour (see GUIDE chapter 17). It only draws; it never changes sound or settings.
`action`: `open` (default) or `close`. `page`: tour page id (synth, sound, sound-deep, pulse, freq, backing, tune, system,
key-test, help, chat; the aliases play, back, sys and params are accepted; case and spaces ignored). `element`: a control or zone name to select on that page
(unknown names open the page with nothing selected). `guided`: true starts the nine-page walk. Bad types, unknown
pages, or a page over 24 or element over 64 characters, are refused. The pictures are built after the reply, so the reply shows
`"building":true` the first time. The reply carries the tour state (`open`, `building`, `page`, `selected`, `concept`, `concepts_open`, `guided`, `step`), which is also `state.tour`:
```
{"id":30,"cmd":"tour","page":"pulse","element":"pad0"}
-> {"ok":true,"open":true,"building":true,"page":"pulse","selected":"pad0","guided":false,...}
{"id":31,"cmd":"tour","action":"close"}
```
The `page` and `highlight` commands close the tour first. The player can also open it with `Ctrl`+`O` or the Help
page's Tour button. When the player taps **Ask Rook** on a card, their message arrives with `via:"tour"`, `page`
set to the card's real page and `focus` set to the control's zone name (for a concept card, `concepts.<id>`).

## Guided help: pointing at the screen

Every control on the screen is a named **zone**. An agent can ring the zones it is talking about, learn what
exists on a page, and see the player's answer arrive from a small chat dock that opens over whatever page the
player is on (`Ctrl`+`Y`), together with where the player was when they asked.

### highlight
`zones`: 1-16 zone names (each up to 48 characters); `label`: optional caption up to 40 characters (control
characters become spaces); `ms`: how long, 100-30000 (default 8000); `page`: optional page to switch to
**first, in the same step** (any page name as for `page`); `sub`: with `help`, which section of the Help page
(`playing`, `commands`, `pulse` or 0-2).

The named zones of the page being shown get a pulsing double rule (two white lines, 2 px clear of the control).
With a `label` a small caption tag sits next to the first one: always on screen, never over a highlighted zone,
and never under the chat dock. A name can also be an **alias** (`octave` = `octave_down` + `octave_up`; `strings`,
`tabs`, `chords`, `pads`) or end in `*` to mean every zone with that prefix (`chord*`). Names that do not exist
on that page are returned in `unknown`; the others still highlight (if none exist, nothing is highlighted and
`highlighted` is empty). The reply:
```
-> {"id":40,"cmd":"highlight","zones":["octave"],"label":"OCTAVE","ms":10000}
<- {"id":40,"ok":true,"result":{"page":"synth","highlighted":["octave_down","octave_up"],"unknown":[],
    "ms":10000,"caption":true,"dock":{"open":false}}}
-> {"id":41,"cmd":"highlight","zones":["help_ask","nope"],"page":"help"}
<- {"id":41,"ok":true,"result":{"page":"help","highlighted":["help_ask"],"unknown":["nope"],"ms":8000,"caption":false,"dock":{"open":false}}}
```
A highlight ends when its time is up, on `unhighlight`, or **when the player changes page** (issue it again
if you want it back). If the chat dock is open it moves clear of the highlighted zones: shorter, then to the
top of the screen, and the dock's header shows `▸ OCTAVE` (the label, or else the name asked for) while that is
what is being discussed. A new agent message that follows a highlight within 5 seconds keeps that tag.

### unhighlight
Clears the highlight at once. The reply says how many zones were cleared.
```
{"id":42,"cmd":"unhighlight"}
```

### zones
`page` (default: the page shown), `sub` for Help. Returns every named zone of that page as
`{name, label, x, y, w, h}` (canvas pixels, 720 x 720) and `dock_open`; a zone the open dock covers has
`covered_by_dock: true`. `label` is the text drawn on the control where there is one, else a readable name.
Another page is drawn off-screen to list it: the player stays where they are and nothing is touched.
```
-> {"id":43,"cmd":"zones","page":"synth"}
<- {"id":43,"ok":true,"result":{"page":"synth","dock_open":false,"zones":[{"name":"octave_up","label":"Octave up","x":396,"y":628,"w":54,"h":80}, ...]}}
```

### help_focus (in state)
```
"help_focus":{"page":"synth","highlighted":["octave_down","octave_up"],"dock_open":true,"dock_focus":true,
              "last_question_zone":"octave_down","topic":"OCTAVE"}
```
`last_question_zone` is the first focus zone of the last message the player sent. `topic` is what the dock's tag says.

### What the player's message carries
A message typed in the dock (or on the Chat page) is an ordinary chat message with the next `seq`; `chat_poll` and
`chat_wait` return it with extra fields: `page` (where the player was: `synth`, `help`...), `focus` (the highlighted
zones, else the zones the last agent message was about, else the control the player touched last on that page;
possibly empty) and `via` (`dock`, `chat` or `tour`). So an agent reads "asked on synth with octave_down, octave_up
highlighted" and can answer about exactly that. Tapping a row on the Help page asks about that row: `focus`
is the row's zone name.
```
{"seq":57,"from":"you","user":true,"text":"why does it jump so far?","page":"synth","focus":["octave_down","octave_up"],"via":"dock",...}
```

### The chat dock (what the player sees)
`Ctrl`+`Y` on any page except Chat opens a dock over the bottom of the screen (the top, if something highlighted is
down there), with the last few lines, an input and Send. The keyboard goes to it: letters type, `Enter` sends,
`Esc` gives the keys back to the page and keeps the dock; `Esc` again closes it; `Ctrl`+`Y` toggles the
keyboard while it is open; `Ctrl`+`Shift`+`Y` opens the full Chat page. A new agent message while the dock is
closed shows the ticker, which can be touched (or `Ctrl`+`Y` pressed) to open the dock. Dock zones: `dock`, `dock_history`,
`dock_input`, `dock_send`, `dock_full`, `dock_close`, `ticker`. They are not accepted by `highlight`.

### Zone names

Names are stable; add new ones rather than renaming. Names ending in a digit are numbered from 0 (`string0`
lowest, `pad0` first pulse pad, `chord0` first chord pad...). The authoritative list for the current screen is
`zones`.

| Page | Zones |
|---|---|
| every page but Synth | `tab_synth` `tab_sound` `tab_pulse` `tab_freq` `tab_backing` `tab_tune` `tab_system` `tab_help` `tab_chat` |
| synth | `hdr_sound` (name) `hdr_tune` (key, tempo, octave) `key_readout` `tempo_readout` `octave_readout` `xy` (scope pad) `xymap` `xyhold` `string0` `string1` `string2` `pslot0`-`pslot7` (filled progression slots) `chord0`-`chord6` `backmode` `backplay` `loop_pedal` `loop_undo` `loop_playstop` `sustain` `octave_down` `octave_up` `new_sound` `menu` |
| sound | `slot0`-`slot9` `newsound` `mutate` `undosound` `slotsave` `fav` `deep` `warp` (the Warp view) `param<N>` `deepprev` `deepnext` `favitem0`-`favitem5` `favprev` `favdel` |
| pulse | `pad0`-`pad7` `pulsewave` (the selected pad's picture) `pulsetone` `pulselength` `pulserate` `pulselevel` `pulselast` (the strip) `pulseloop` `pulselatch` `pulsesync` `pulsewarp` `pulsetype` (the pad's name: tap for the next kind); with Warp on, `pulsedrive` `pulsefold` `pulsecrush` `pulsebend` |
| freq | `freqscope` `freqcycles` `freqspectrum` `freqtone0`-`freqtone3` (rows) `freqwave0`-`3` `freqhz0`-`3` (tap: type an Hz, drag: scrub) `freqlevel0`-`3` `freqon0`-`3` `freqmod` (2 > 1) `freqfold` `freqfilter` `freqdrive` `freqcrush` `freqdelay` `freqhold` `freqgate` `freqrate-/+` `freqkeys` `freqsnap` |
| backing | `bmode0`-`bmode2` `bstyle0`-`bstyle4` `bslot0`-`bslot7` `chord0`-`chord6` `backmode` `backplay` `bremove` `bclear` `bseventh` `bmetro` `chordbeats-/+` `arprate-/+` `bpm2-/+` `backvol` `bresync` |
| tune | `root0`-`root11` `scale0`-`scale7` `oct-/+` `rowint-/+` `bendr-/+` `bpm-/+` `tap` `glidescale` `listen` `lkey` `ltempo` `lboth` `lnext` |
| system | `volume` `loopvol` `xymap0`-`xymap5` `xyhold2` `chordrow` `recwav` `playlast` `savesession` `keytest` `session0`... `panic` `latency-/+` `factory` `quit` |
| help | `help_section` `help_next` `help_ask` `help_back` and one `help_row_<description>` per row, e.g. `help_row_loop_pedal_count_in_record_play_ov`; three sections (`sub`), list them with `zones` |
| chat | `chat_history` `chat_input` `chat_send` |

Renamed in 0.12.0 (old name -> new): `tab0`-`tab8` -> `tab_<page>`; `loop` -> `loop_pedal`; `loopundo` -> `loop_undo`;
`loopstop` -> `loop_playstop`; `sus` -> `sustain`; `octdn` `octup` -> `octave_down` `octave_up`; `dice` -> `new_sound`;
`keep` -> `track_keep`; `shelf<N>` -> `shelf_clip<N>`; `shelfpage` -> `track_shelf_page`; `ruler` `lanes` -> `track_ruler` `track_lanes`;
`tplace tchop tsound trepeat tremove tplay tcycle tlen texport tsplit tjoin tmute tundo tmix tview` -> `track_place ... track_view`;
`chalves cbeats creset cdone cleft cright cmute crepeat chopstrip` -> `chop_halves chop_beats chop_reset chop_done chop_move_left chop_move_right chop_mute_piece chop_repeat_piece chop_strip`;
`skind<N> scell<N> shear soct sscope srec sdone` -> `sound_kind<N> sound_cell<N> sound_hear sound_octave sound_scope sound_as_recorded sound_done`;
`mxlane<N> mxfader<N> mxmute<N> mxsolo<N> mxplay mxreset mxdone` -> `mixer_lane<N> ... mixer_done`; `chathist chatsend` -> `chat_history chat_send`;
`helppage helpplay` -> `help_next help_back`. Removed with the Track page: every `track_*` `chop_*` `sound_*` `mixer_*` `shelf_clip*` zone and `tab_track`
(the Backing page and its zones came back in 0.14.0). Zones added: `octave_readout` `key_readout` `tempo_readout`, `chat_input`, `help_*`, `dock*`, `ticker`.

## Errors you may see

`bad JSON: ...`, `a request must be a JSON object`, `missing 'cmd'`, `unknown command 'x'`,
`'string' must be between 0 and 2`, `'x' must be a number` / `a string` / `true or false`, `missing 'x'`,
`request line too long`. A refused request changes nothing.
