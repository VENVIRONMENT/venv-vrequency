# Help cards: notes for whoever wires the tour

Source of truth for the cards is `docs/helpcards.json` (validated by `scripts/check_helpcards.py`).
Zone names were taken by reading `app/*.cpp` (every `zone()`, `button()`, `slider()`, `stepper()` call,
including generated names such as `name-`/`name+`, `tab%d`, `param%d`). The app was not run;
`docs/zones.txt` does not exist yet, so the checker's zone-coverage step was not exercised (it accepts
`zone`, `page zone` or `page.zone` per line). Coverage below was checked with a throwaway script against
the zone lists read from the code.

## Conventions used in the JSON

- A card's key is a real zone name. Families share one card via `match` (the key is also listed in
  `match`): `chord0..6`, `pslot0..7`, `bslot0..7`, `slot0..9`, `favitem0..5`, `beatslot0..6`, `root0..11`,
  `session0..3`, `shelf0..4`, `scell0..5`, `mxlane/mxfader/mxmute/mxsolo 0..3`, and every `name-`/`name+` pair.
- Tabs: the `track` page has nine individual tab cards (`tab0`..`tab8`). Every other tabbed page has one
  `tab0` card with `match` = all nine tabs ("PAGE TABS").
- The Track page's three panels are their own pages (`track-chop`, `track-sound`, `track-mixer`). While a
  panel is open, the shelf, Keep, ruler, lanes and tab zones are still live, so those cards are repeated
  on each panel page (same text).
- `sound-deep` is the All-parameters editor (40 `param%d` cards). The 8 quick sliders on the Sound page
  (`param16,17,11,14,31,32,34,37`) use the same card text as on `sound-deep`. `key-test` is the overlay.
- `related` ids are `page.zone` (a zone inside a `match` list counts) or `concepts.<id>`.
- The Synth page ("play") draws no tab strip, so it has no `tab%d` zones.

## Coverage (zones registered on the page, including tabs, vs cards)

| Page | Zones | Cards |
|---|---|---|
| track | 35 | 29 |
| track-chop | 27 | 15 |
| track-sound | 35 | 17 |
| track-mixer | 37 | 13 |
| play (Synth) | 33 | 19 |
| sound | 41 | 19 |
| sound-deep | 67 | 50 |
| beat | 34 | 18 |
| back | 46 | 22 |
| tune | 45 | 22 |
| sys | 32 | 20 |
| key-test | 1 | 1 |
| help | 11 | 3 |
| chat | 11 | 3 |
| **total** | 455 | 251 (+ 23 concepts) |

Every zone has a card (through `match` where families share one). `pslot%d` / `bslot%d` zones exist only
for filled progression slots, `beatslot%d` / `favitem%d` only for existing items, `beatmore` / `shelfpage`
only when there is more than one page, and `keep` has two geometries; the cards cover all of them.

## Zone names that could not be mapped

None: every zone name found in code has a card. No names were invented.

## Things on screen that have no zone (so no card key; covered under `concepts` instead)

A tour that wants to point at these needs hotspots added in code (names below are only suggestions):

- Level strip under the Synth header (`concepts.level-strip`).
- Scope readouts: volts/div, time/div, trigger state, kSa/s, Freq, Pk-Pk, Note, axis labels
  (`concepts.scope-readouts`). The scope is one zone, `xy`.
- String brackets and trail on the strings (`concepts.string-brackets`); beat lamp in the Synth header
  (described in `play.hdr_tune`).
- The Beat page's 8 x 16 step grid (display only, cannot be tapped).
- Track play line and clip pictures (the play line is moved through `ruler`; clips through `lanes`).
- Chat input box (no zone: it is keyboard only; Send is `chatsend`), the ticker (`concepts.ticker`), toasts.
- System page Status box, Tune page Listen results, Help page key rows.
- The Loop button's progress bar and state label belong to the `loop` zone and are covered there.

## Behaviour I could not fully verify from code (stated carefully in the cards, or avoided)

- Sound of each drum and of the Keys/Guitar/Bass shelf: only generic descriptions; nobody has listened (STATUS).
- `CHORUS`: the card says "amount of chorus" without claiming per-sound or master; the engine reads
  `P_CHORUS` per part but mixes the effect on a master bus.
- XY moves are recorded in loops (`OP_XY` exists, STATUS lists it as untested); the cards say so for the pad.
- Q20 layer facts (digits on a number layer, arrows on a symbol layer, whether `[ ] - =` are reachable) come
  from GUIDE/README/STATUS; STATUS says physical keys are unproven. `Ctrl+Shift+K/S/Z` rely on Shift being
  the chromatic key, which holds in code but is unproven on the device.
- Bass picks "drop an octave" (GUIDE) was not traced to `libraryOctave`'s exact value.
- The scope's trigger label reads "Stop" in the docs pictures only because they were rendered with null
  audio; on a live device it reads Trig'd or Auto (code, scope.cpp).
- `Halves` is described as one cut at the middle; a second tap was not traced.

## Wrong, confusing or inconsistent things noticed

Likely bugs or footguns in the app:

1. **Key test panel does not block taps under it.** `paintKeyTest` registers only the Close zone, so touches
   on the panel hit the System-page buttons hidden beneath (session list, mapping row, Record...). Same for
   the ticker over the bottom row: it "takes no touch", so a tap on it can hit Quit / Factory sounds /
   Loop fix on the System page, or Menu/New on the Synth page.
2. **Armed modes are not all cleared on leaving a page.** The tab strip resets `deepEdit`, `slotSave`,
   `beatErase`, `removeArmed`, but not `favDelete` (Sound page Delete stays armed after you leave and then
   deletes the next favourite you tap). Moving with `Tab`/`Esc`/`Ctrl+H` resets only `deepEdit` (and
   `Esc` the key test), so Store and Erase can stay armed.
3. **Arp / pulse steps per beat** (Backing stepper) is labelled for both, but Pulse always re-strikes once per
   beat; only the arpeggio styles use it (`tickBacking`).
4. **Paging buttons only go forward**: `favprev`, `shelfpage`, `beatmore` cycle one way (the favourites button
   even looks two-way: `‹ 1/2 ›`, and its zone is called `favprev`). Same for the Kit stepper wrapping.
5. **Tempo step differs by page**: Tune is +-1, Backing and Beat +-2 (same setting).
6. **Menu button on the Synth page opens the Sound page**, and the Synth page has no tab strip, so the only
   way to the other tabs is via Menu or `Tab`. GUIDE section 3 says Menu "opens the other pages".
7. The Sessions list shows only the four newest files; older sessions cannot be loaded from the UI.
8. Saving a favourite with an existing name writes `name-2.json` etc., so duplicates pile up; nothing warns.

Documentation errors:

9. **GUIDE section 2 step 5 and section 7**: "tap two or three chord pads, then tap Play". In Progression mode the
   first chord tap already starts the backing (`chordTap` sets `backPlaying`), so tapping Play then stops it.
10. GUIDE section 10 ("Things it does not do yet: ... a volume for each lane") and STATUS "No per-lane volume"
    are stale: the lane mixer (0.9.0) provides exactly that.
11. GUIDE section 3 lists the tabs without Chat ("Track, Synth, Sound, Beat, Backing, Tune, System, Help").
12. Test counts disagree: README says 517 engine / 227 app checks, STATUS 522 / 238.
13. `docs/shots/9-beat.png` is out of date: only 8 tabs (no Chat), and Beat volume reads 0%; others show 9 tabs.
14. Zone-name oddities worth knowing when writing hotspots: `xymap` (scope button) vs `xymap0..5` (System page),
    `xyhold` vs `xyhold2`, `bpm` / `bpm2` / `bpm3`, `oct` vs `octdn` / `octup`, `favprev`, `hdr_sound`.
