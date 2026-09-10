#pragma once
#include <optional>

#include "config/configuration_store.h"
#include "ui/gesture_editor.h"
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
  void load_values();
  [[nodiscard]] bool save_values();
  void rescale_children(UINT old_dpi, UINT new_dpi) noexcept;
  HINSTANCE instance_{};
  HWND window_{};
  config::ConfigurationBundle* destination_{};
  config::ConfigurationBundle working_;
  std::optional<GestureEditor> gestures_;
  std::optional<ProfileEditor> profiles_;
  bool accepted_{};
  bool finished_{};
  UINT current_dpi_{96};
};
}  // namespace strokes::ui
