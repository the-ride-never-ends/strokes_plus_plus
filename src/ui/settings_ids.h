#pragma once

namespace strokes::ui {

// Control identifiers shared by the settings window and its editors. IDOK and
// IDCANCEL are reserved by the dialog manager and are mapped onto save_id and
// cancel_id by the window's command handler.
enum : int {
  enabled_id = 101,
  button_id,
  move_id,
  distance_id,
  max_id,
  threshold_id,
  overlay_id,
  width_id,
  opacity_id,
  shortcut_id,
  gestures_id,
  gesture_name_id,
  gesture_add_id,
  gesture_rename_id,
  gesture_delete_id,
  gesture_train_id,
  gesture_remove_sample_id,
  gesture_toggle_id,
  profiles_id,
  profile_name_id,
  profile_property_id,
  profile_mode_id,
  profile_value_id,
  profile_criteria_id,
  criterion_add_id,
  criterion_update_id,
  criterion_remove_id,
  profile_shortcut_id,
  profile_add_id,
  profile_update_id,
  profile_delete_id,
  profile_toggle_id,
  profile_assign_id,
  global_assign_id,
  save_id,
  cancel_id
};

}  // namespace strokes::ui
