#pragma once

#include <optional>
#include <string>
#include <variant>
#include <utility>

#include "actions/action.h"
#include "gestures/point.h"

namespace strokes::actions {

enum class ProcessOperation { launch };
enum class MouseOperation { click, double_click, button_down, button_up, move };
enum class MouseButton { left, right, middle, x_button_1, x_button_2 };
enum class PositionTarget { current_cursor, gesture_start, gesture_end, absolute };
enum class WindowOperation { close, minimize, maximize, restore, activate, move, resize, move_resize };
enum class WindowTarget { gesture_window, foreground_window, window_at_gesture_start };
enum class MediaOperation { play_pause, next_track, previous_track, stop };
enum class VolumeOperation { increase, decrease, mute_toggle };
enum class VirtualDesktopOperation { next, previous, create, close };

struct KeyboardParameters {
  std::string shortcut;
  friend bool operator==(const KeyboardParameters&, const KeyboardParameters&) = default;
};

struct ProcessParameters {
  ProcessOperation operation{ProcessOperation::launch};
  std::string path;
  std::string arguments;
  std::string working_directory;
  friend bool operator==(const ProcessParameters&, const ProcessParameters&) = default;
};

struct UrlParameters {
  std::string uri;
  friend bool operator==(const UrlParameters&, const UrlParameters&) = default;
};

struct PositionDefinition {
  PositionTarget target{PositionTarget::current_cursor};
  std::optional<gestures::Point> absolute;
  friend bool operator==(const PositionDefinition&, const PositionDefinition&) = default;
};

struct MouseParameters {
  MouseOperation operation{MouseOperation::click};
  std::optional<MouseButton> button;
  PositionDefinition position;
  friend bool operator==(const MouseParameters&, const MouseParameters&) = default;
};

struct WindowParameters {
  WindowOperation operation{WindowOperation::close};
  WindowTarget target{WindowTarget::gesture_window};
  std::optional<int> x;
  std::optional<int> y;
  std::optional<int> width;
  std::optional<int> height;
  friend bool operator==(const WindowParameters&, const WindowParameters&) = default;
};

struct MediaParameters {
  MediaOperation operation{MediaOperation::play_pause};
  friend bool operator==(const MediaParameters&, const MediaParameters&) = default;
};

struct VolumeParameters {
  VolumeOperation operation{VolumeOperation::mute_toggle};
  std::optional<double> amount;
  friend bool operator==(const VolumeParameters&, const VolumeParameters&) = default;
};

struct VirtualDesktopParameters {
  VirtualDesktopOperation operation{VirtualDesktopOperation::next};
  friend bool operator==(const VirtualDesktopParameters&, const VirtualDesktopParameters&) = default;
};

using ActionParameters =
    std::variant<KeyboardParameters, ProcessParameters, UrlParameters, MouseParameters,
                 WindowParameters, MediaParameters, VolumeParameters, VirtualDesktopParameters>;

struct ActionDefinition {
  static constexpr int current_version = 1;
  int version{current_version};
  ActionType type{ActionType::keyboard_shortcut};
  ActionParameters parameters{KeyboardParameters{}};

  [[nodiscard]] static ActionDefinition keyboard(std::string shortcut) {
    return {current_version, ActionType::keyboard_shortcut,
            KeyboardParameters{std::move(shortcut)}};
  }

  friend bool operator==(const ActionDefinition&, const ActionDefinition&) = default;
};

[[nodiscard]] inline const std::string* keyboard_shortcut(const ActionDefinition& definition) {
  const auto* value = std::get_if<KeyboardParameters>(&definition.parameters);
  return definition.type == ActionType::keyboard_shortcut && value != nullptr
             ? &value->shortcut
             : nullptr;
}

[[nodiscard]] inline std::string* keyboard_shortcut(ActionDefinition& definition) {
  auto* value = std::get_if<KeyboardParameters>(&definition.parameters);
  return definition.type == ActionType::keyboard_shortcut && value != nullptr
             ? &value->shortcut
             : nullptr;
}

struct ActionValidationResult {
  bool valid{};
  std::string code;
  std::string message;
};

/// Validates type/parameter agreement and all persistence-time requirements.
[[nodiscard]] ActionValidationResult validate(const ActionDefinition& definition);
[[nodiscard]] std::string action_type_name(ActionType type);
[[nodiscard]] std::string action_operation_name(const ActionDefinition& definition);
[[nodiscard]] std::string action_target_name(const ActionDefinition& definition);
/// Returns a concise user-facing description including the action's meaningful parameters.
[[nodiscard]] std::string action_display_name(const ActionDefinition& definition);
/// Returns a short user-facing label suitable for action lists.
[[nodiscard]] std::string action_label(const ActionDefinition& definition);

}  // namespace strokes::actions
