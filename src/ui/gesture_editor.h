#pragma once

#include <string>
#include <vector>

#include "config/configuration_store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {

/// Edits gesture definitions, training samples, and their global shortcuts.
class GestureEditor {
 public:
  GestureEditor(HWND window, HINSTANCE instance, config::ConfigurationBundle& configuration)
      : window_(window), instance_(instance), configuration_(&configuration) {}

  void create();
  void set_visible(bool visible) const noexcept;
  void refresh();
  /// Fills the name and shortcut fields from the selected gesture.
  void load();
  /// Handles one command, reporting whether it belonged to this editor.
  [[nodiscard]] bool handle(int command, int notification);
  /// Returns the selected gesture identifier, empty when nothing is selected.
  [[nodiscard]] std::string selected() const;
  /// Validates the shortcut field and applies it to the target bundle.
  ///
  /// Args:
  ///   target: The pending configuration copy the settings window will commit.
  /// Returns:
  ///   False when the shortcut is invalid, after reporting it to the user.
  [[nodiscard]] bool save(config::ConfigurationBundle& target) const;

 private:
  static LRESULT CALLBACK preview_proc(HWND, UINT, WPARAM, LPARAM);
  void paint_preview(HWND preview, HDC target) const noexcept;
  void refresh_preview() const noexcept;
  void add();
  void rename();
  void erase();
  void train();
  void drop_sample();
  void toggle();
  void assign();
  void remove_action();
  void select_action();
  [[nodiscard]] int index() const noexcept;

  HWND window_;
  HINSTANCE instance_;
  config::ConfigurationBundle* configuration_;
  HWND preview_{};
  std::vector<std::string> action_gesture_ids_;
};

}  // namespace strokes::ui
