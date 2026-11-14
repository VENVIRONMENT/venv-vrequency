// venv-vrequency — the name of every touch zone, in one place. The pages register their zones under
// these names; the help cards (docs/helpcards.json), the tour, the control socket (docs/CONTROL.md) and
// the self-test refer to them, so a name is never retyped as a string. docs/zones.txt lists them by page.
//
// A constant is spelled exactly like the name it holds. Three kinds of name are built from one:
//   at(slot, 3)   -> "slot3"    a numbered family: the constant is the part before the number
//   minus(bpm)    -> "bpm-"     the two halves of a stepper
//   plus(bpm)     -> "bpm+"
#pragma once
#include <QString>

namespace zn {
// ---- the tabs: tab("synth") -> "tab_synth" ----
inline constexpr char tab_prefix[] = "tab_";

// ---- Synth ----
inline constexpr char hdr_sound[] = "hdr_sound";
inline constexpr char hdr_tune[] = "hdr_tune";
inline constexpr char key_readout[] = "key_readout";
inline constexpr char loop_pedal[] = "loop_pedal";
inline constexpr char loop_playstop[] = "loop_playstop";
inline constexpr char loop_undo[] = "loop_undo";
inline constexpr char menu[] = "menu";
inline constexpr char new_sound[] = "new_sound";
inline constexpr char octave_down[] = "octave_down";
inline constexpr char octave_readout[] = "octave_readout";
inline constexpr char octave_up[] = "octave_up";
inline constexpr char pslot[] = "pslot";   // family
inline constexpr char string[] = "string";   // family
inline constexpr char sustain[] = "sustain";
inline constexpr char tempo_readout[] = "tempo_readout";
inline constexpr char xy[] = "xy";
inline constexpr char xyhold[] = "xyhold";
inline constexpr char xymap[] = "xymap";   // family

// ---- Sound ----
inline constexpr char deep[] = "deep";
inline constexpr char fav[] = "fav";
inline constexpr char favdel[] = "favdel";
inline constexpr char favitem[] = "favitem";   // family
inline constexpr char favprev[] = "favprev";
inline constexpr char mutate[] = "mutate";
inline constexpr char newsound[] = "newsound";
inline constexpr char param[] = "param";   // family
inline constexpr char slot[] = "slot";   // family
inline constexpr char slotsave[] = "slotsave";
inline constexpr char undosound[] = "undosound";

// ---- Sound: all parameters ----
inline constexpr char deepnext[] = "deepnext";
inline constexpr char deepprev[] = "deepprev";

// ---- Backing ----
inline constexpr char arprate[] = "arprate";
inline constexpr char backmode[] = "backmode";
inline constexpr char backplay[] = "backplay";
inline constexpr char backvol[] = "backvol";
inline constexpr char bclear[] = "bclear";
inline constexpr char bmetro[] = "bmetro";
inline constexpr char bmode[] = "bmode";   // family
inline constexpr char bpm2[] = "bpm2";
inline constexpr char bremove[] = "bremove";
inline constexpr char bresync[] = "bresync";
inline constexpr char bseventh[] = "bseventh";
inline constexpr char bslot[] = "bslot";   // family
inline constexpr char bstyle[] = "bstyle";   // family
inline constexpr char chord[] = "chord";   // family
inline constexpr char chordbeats[] = "chordbeats";

// ---- Beat ----
inline constexpr char beaterase[] = "beaterase";
inline constexpr char beatmore[] = "beatmore";
inline constexpr char beatnew[] = "beatnew";
inline constexpr char beatplay[] = "beatplay";
inline constexpr char beatrec[] = "beatrec";
inline constexpr char beatslot[] = "beatslot";   // family
inline constexpr char beatvol[] = "beatvol";
inline constexpr char bpm3[] = "bpm3";
inline constexpr char kit[] = "kit";
inline constexpr char pad[] = "pad";   // family

// ---- Tune ----
inline constexpr char bendr[] = "bendr";
inline constexpr char bpm[] = "bpm";
inline constexpr char glidescale[] = "glidescale";
inline constexpr char lboth[] = "lboth";
inline constexpr char listen[] = "listen";
inline constexpr char lkey[] = "lkey";
inline constexpr char lnext[] = "lnext";
inline constexpr char ltempo[] = "ltempo";
inline constexpr char metro[] = "metro";
inline constexpr char oct[] = "oct";
inline constexpr char root[] = "root";   // family
inline constexpr char rowint[] = "rowint";
inline constexpr char scale[] = "scale";   // family
inline constexpr char tap[] = "tap";

// ---- System ----
inline constexpr char chordrow[] = "chordrow";
inline constexpr char factory[] = "factory";
inline constexpr char keytest[] = "keytest";
inline constexpr char latency[] = "latency";
inline constexpr char loopvol[] = "loopvol";
inline constexpr char panic[] = "panic";
inline constexpr char playlast[] = "playlast";
inline constexpr char quit[] = "quit";
inline constexpr char recwav[] = "recwav";
inline constexpr char savesession[] = "savesession";
inline constexpr char session[] = "session";   // family
inline constexpr char volume[] = "volume";
inline constexpr char xyhold2[] = "xyhold2";

// ---- Key test ----
inline constexpr char keytestclose[] = "keytestclose";

// ---- Help ----
inline constexpr char help_ask[] = "help_ask";
inline constexpr char help_back[] = "help_back";
inline constexpr char help_next[] = "help_next";
inline constexpr char help_section[] = "help_section";
inline constexpr char help_tour[] = "help_tour";

// ---- Chat ----
inline constexpr char chat_history[] = "chat_history";
inline constexpr char chat_input[] = "chat_input";
inline constexpr char chat_send[] = "chat_send";

// ---- Track ----
inline constexpr char shelf_clip[] = "shelf_clip";   // family
inline constexpr char track_chop[] = "track_chop";
inline constexpr char track_cycle[] = "track_cycle";
inline constexpr char track_export[] = "track_export";
inline constexpr char track_join[] = "track_join";
inline constexpr char track_keep[] = "track_keep";
inline constexpr char track_lanes[] = "track_lanes";
inline constexpr char track_length[] = "track_length";
inline constexpr char track_mixer[] = "track_mixer";
inline constexpr char track_mute[] = "track_mute";
inline constexpr char track_place[] = "track_place";
inline constexpr char track_play[] = "track_play";
inline constexpr char track_remove[] = "track_remove";
inline constexpr char track_repeat[] = "track_repeat";
inline constexpr char track_ruler[] = "track_ruler";
inline constexpr char track_shelf_page[] = "track_shelf_page";
inline constexpr char track_sound[] = "track_sound";
inline constexpr char track_split[] = "track_split";
inline constexpr char track_undo[] = "track_undo";
inline constexpr char track_view[] = "track_view";

// ---- Track: chop ----
inline constexpr char chop_beats[] = "chop_beats";
inline constexpr char chop_done[] = "chop_done";
inline constexpr char chop_halves[] = "chop_halves";
inline constexpr char chop_move_left[] = "chop_move_left";
inline constexpr char chop_move_right[] = "chop_move_right";
inline constexpr char chop_mute_piece[] = "chop_mute_piece";
inline constexpr char chop_repeat_piece[] = "chop_repeat_piece";
inline constexpr char chop_reset[] = "chop_reset";
inline constexpr char chop_strip[] = "chop_strip";

// ---- Track: sound picker ----
inline constexpr char sound_as_recorded[] = "sound_as_recorded";
inline constexpr char sound_cell[] = "sound_cell";   // family
inline constexpr char sound_done[] = "sound_done";
inline constexpr char sound_hear[] = "sound_hear";
inline constexpr char sound_kind[] = "sound_kind";   // family
inline constexpr char sound_octave[] = "sound_octave";
inline constexpr char sound_scope[] = "sound_scope";

// ---- Track: mixer ----
inline constexpr char mixer_done[] = "mixer_done";
inline constexpr char mixer_fader[] = "mixer_fader";   // family
inline constexpr char mixer_lane[] = "mixer_lane";   // family
inline constexpr char mixer_mute[] = "mixer_mute";   // family
inline constexpr char mixer_play[] = "mixer_play";
inline constexpr char mixer_reset[] = "mixer_reset";
inline constexpr char mixer_solo[] = "mixer_solo";   // family

// ---- Overlays: the chat dock, the ticker, the key test ----
inline constexpr char dock[] = "dock";
inline constexpr char dock_close[] = "dock_close";
inline constexpr char dock_full[] = "dock_full";
inline constexpr char dock_history[] = "dock_history";
inline constexpr char dock_input[] = "dock_input";
inline constexpr char dock_send[] = "dock_send";
inline constexpr char ticker[] = "ticker";
inline constexpr char keytestblock[] = "keytestblock";

// ---- The tour ----
inline constexpr char tour_ask[] = "tour_ask";
inline constexpr char tour_back[] = "tour_back";
inline constexpr char tour_close[] = "tour_close";
inline constexpr char tour_conceptlist[] = "tour_conceptlist";
inline constexpr char tour_concepts[] = "tour_concepts";
inline constexpr char tour_free[] = "tour_free";
inline constexpr char tour_next[] = "tour_next";
inline constexpr char tour_picture[] = "tour_picture";
inline constexpr char tour_prompt[] = "tour_prompt";
inline constexpr char tour_prompt_later[] = "tour_prompt_later";
inline constexpr char tour_prompt_yes[] = "tour_prompt_yes";
inline constexpr char tour_var_next[] = "tour_var_next";
inline constexpr char tour_var_prev[] = "tour_var_prev";
inline constexpr char dock_prefix[] = "dock";      // every zone of the chat dock starts with this
inline constexpr char tour_page[] = "tour_page_";   // family
inline constexpr char help_row[] = "help_row_";     // a Help row: help_row + a slug of its description

inline QString at(const char *family, int i) { return QString::fromLatin1(family) + QString::number(i); }
inline QString minus(const char *stepper) { return QString::fromLatin1(stepper) + QLatin1Char('-'); }
inline QString plus(const char *stepper) { return QString::fromLatin1(stepper) + QLatin1Char('+'); }
inline QString tab(const char *pageName) { return QString::fromLatin1(tab_prefix) + QString::fromLatin1(pageName); }
} // namespace zn
