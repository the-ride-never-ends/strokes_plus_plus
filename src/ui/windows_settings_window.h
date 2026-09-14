#pragma once
#include <optional>

#include "config/configuration_store.h"
#include "ui/gesture_editor.h"
#include "ui/gesture_inventory.h"
#include "ui/profile_editor.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {
/// Edits a working configuration copy and commits it only when the user saves.
///
/// Owns the window, the modal loop, DPI scaling and the global options; gesture
/// and profile editing belong to the two editors it hosts.
class WindowsSettingsWindow final {
 public:
  [[nodiscard]] bool show(HINSTANCE instance, config::ConfigurationBundle& configuration);

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  LRESULT handle_command(WPARAM);
  void create_controls();
  void select_editor_tab() const noexcept;
  void show_settings(bool visible) const noexcept;
  void show_advanced_settings(bool settings_visible) const noexcept;
  void add_tooltip(HWND control, const wchar_t* description) const noexcept;
  void load_values();
  void load_help() const noexcept;
  [[nodiscard]] bool save_values();
  void rescale_children(UINT old_dpi, UINT new_dpi) noexcept;
  HINSTANCE instance_{};
  HWND window_{};
  HWND tooltip_{};
  HWND editor_tabs_{};
  config::ConfigurationBundle* destination_{};
  config::ConfigurationBundle working_;
  std::optional<GestureEditor> gestures_;
  std::optional<GestureInventory> inventory_;
  std::optional<ProfileEditor> profiles_;
  bool accepted_{};
  bool finished_{};
  bool advanced_expanded_{};
  UINT current_dpi_{96};
};
}  // namespace strokes::ui
