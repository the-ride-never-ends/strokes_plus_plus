#pragma once

#include <cstddef>
#include <string>

#include "config/configuration_store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {

/// Edits application profiles, their match criteria, and gesture overrides.
class ProfileEditor {
 public:
  ProfileEditor(HWND window, HINSTANCE instance, config::ConfigurationBundle& configuration)
      : window_(window), instance_(instance), configuration_(&configuration) {}

  void create();
  void set_visible(bool visible) const noexcept;
  void refresh();
  /// Fills the profile fields, including the override for one gesture.
  ///
  /// Args:
  ///   gesture_id: The gesture whose override is shown, empty when none.
  void load(const std::string& gesture_id);
  /// Handles one command, reporting whether it belonged to this editor.
  [[nodiscard]] bool handle(int command, int notification, const std::string& gesture_id);

 private:
  void add(const std::string& gesture_id);
  void rename();
  void erase();
  void toggle();
  void assign(const std::string& gesture_id);
  void refresh_criteria();
  void load_criterion();
  void add_criterion();
  void update_criterion();
  void remove_criterion();
  [[nodiscard]] int profile_index() const noexcept;
  [[nodiscard]] int criterion_index() const noexcept;

  HWND window_;
  HINSTANCE instance_;
  config::ConfigurationBundle* configuration_;
};

}  // namespace strokes::ui
