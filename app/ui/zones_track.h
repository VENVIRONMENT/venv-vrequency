// venv-vrequency — the names of the Track page's touch zones (the page, its chop, sound picker and
// mixer views). Part of zones.h, which includes this file: include zones.h, not this.
#pragma once

namespace zn {
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
} // namespace zn
