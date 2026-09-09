#pragma once
#include "config/configuration_store.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {
/// Edits a working configuration copy and commits it only when the user saves.
class WindowsSettingsWindow final {
 public:
  [[nodiscard]] bool show(HINSTANCE instance, config::ConfigurationBundle& configuration);

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  void create_controls();
  void load_values();
  void refresh_gestures();
  [[nodiscard]] int selected_gesture() const noexcept;
  void add_gesture();
  void rename_gesture();
  void delete_gesture();
  void train_gesture();
  void toggle_gesture();
  void add_profile();
  void update_profile();
  void delete_profile();
  void toggle_profile();
  void refresh_profiles();
  void refresh_criteria();
  void load_criterion();
  void add_criterion();
  void update_criterion();
  void remove_criterion();
  void assign_profile();
  void assign_global();
  void load_gesture();
  void load_profile();
  [[nodiscard]] int selected_profile() const noexcept;
  [[nodiscard]] int selected_criterion() const noexcept;
  [[nodiscard]] bool save_values();
  void rescale_children(UINT old_dpi, UINT new_dpi) noexcept;
  HINSTANCE instance_{};
  HWND window_{};
  config::ConfigurationBundle* configuration_{};
  config::ConfigurationBundle* destination_{};
  config::ConfigurationBundle working_;
  bool accepted_{};
  bool finished_{};
  UINT current_dpi_{96};
};
}  // namespace strokes::ui
