#include "actions/action_definition.h"

#include <cctype>
#include <cmath>
#include <string_view>
#include <utility>

#include "actions/keyboard_shortcut.h"

namespace strokes::actions {
namespace {

ActionValidationResult valid() { return {true, {}, {}}; }

ActionValidationResult invalid(std::string code, std::string message) {
  return {false, std::move(code), std::move(message)};
}

bool has_uri_scheme(std::string_view uri) {
  const auto separator = uri.find(':');
  if (separator == std::string_view::npos || separator == 0) return false;
  if (!std::isalpha(static_cast<unsigned char>(uri.front()))) return false;
  for (const unsigned char character : uri.substr(1, separator - 1)) {
    if (!std::isalnum(character) && character != '+' && character != '-' && character != '.')
      return false;
  }
  return true;
}

template <typename Parameters>
const Parameters* parameters(const ActionDefinition& definition) {
  return std::get_if<Parameters>(&definition.parameters);
}

bool known(ProcessOperation value) { return value == ProcessOperation::launch; }

bool known(MouseOperation value) {
  switch (value) {
    case MouseOperation::click:
    case MouseOperation::double_click:
    case MouseOperation::button_down:
    case MouseOperation::button_up:
    case MouseOperation::move:
      return true;
  }
  return false;
}

bool known(MouseButton value) {
  switch (value) {
    case MouseButton::left:
    case MouseButton::right:
    case MouseButton::middle:
    case MouseButton::x_button_1:
    case MouseButton::x_button_2:
      return true;
  }
  return false;
}

bool known(PositionTarget value) {
  switch (value) {
    case PositionTarget::current_cursor:
    case PositionTarget::gesture_start:
    case PositionTarget::gesture_end:
    case PositionTarget::absolute:
      return true;
  }
  return false;
}

bool known(WindowOperation value) {
  switch (value) {
    case WindowOperation::close:
    case WindowOperation::minimize:
    case WindowOperation::maximize:
    case WindowOperation::restore:
    case WindowOperation::activate:
    case WindowOperation::move:
    case WindowOperation::resize:
    case WindowOperation::move_resize:
      return true;
  }
  return false;
}

bool known(WindowTarget value) {
  switch (value) {
    case WindowTarget::gesture_window:
    case WindowTarget::foreground_window:
    case WindowTarget::window_at_gesture_start:
      return true;
  }
  return false;
}

bool known(MediaOperation value) {
  switch (value) {
    case MediaOperation::play_pause:
    case MediaOperation::next_track:
    case MediaOperation::previous_track:
    case MediaOperation::stop:
      return true;
  }
  return false;
}

bool known(VolumeOperation value) {
  switch (value) {
    case VolumeOperation::increase:
    case VolumeOperation::decrease:
    case VolumeOperation::mute_toggle:
      return true;
  }
  return false;
}

bool known(VirtualDesktopOperation value) {
  switch (value) {
    case VirtualDesktopOperation::next:
    case VirtualDesktopOperation::previous:
    case VirtualDesktopOperation::create:
    case VirtualDesktopOperation::close:
      return true;
  }
  return false;
}

}  // namespace

ActionValidationResult validate(const ActionDefinition& definition) {
  if (definition.version != ActionDefinition::current_version)
    return invalid("unsupported_version", "The action schema version is unsupported.");

  switch (definition.type) {
    case ActionType::keyboard_shortcut: {
      const auto* value = parameters<KeyboardParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Keyboard parameters are required.");
      if (!parse_shortcut_sequence(value->shortcut))
        return invalid("invalid_shortcut", "The keyboard shortcut is invalid.");
      return valid();
    }
    case ActionType::process: {
      const auto* value = parameters<ProcessParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Process parameters are required.");
      if (!known(value->operation))
        return invalid("unknown_operation", "The process operation is unsupported.");
      if (value->path.empty())
        return invalid("missing_executable", "An executable path is required.");
      return valid();
    }
    case ActionType::url: {
      const auto* value = parameters<UrlParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "URL parameters are required.");
      if (!has_uri_scheme(value->uri))
        return invalid("invalid_uri", "A URI with a valid scheme is required.");
      return valid();
    }
    case ActionType::mouse: {
      const auto* value = parameters<MouseParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Mouse parameters are required.");
      if (!known(value->operation))
        return invalid("unknown_operation", "The mouse operation is unsupported.");
      if (!known(value->position.target))
        return invalid("unknown_position_target", "The mouse position target is unsupported.");
      if (value->button && !known(*value->button))
        return invalid("unknown_mouse_button", "The mouse button is unsupported.");
      if (value->operation != MouseOperation::move && !value->button)
        return invalid("missing_mouse_button", "The mouse operation requires a button.");
      if (value->position.target == PositionTarget::absolute && !value->position.absolute)
        return invalid("missing_position", "Absolute mouse positions require X and Y coordinates.");
      return valid();
    }
    case ActionType::window: {
      const auto* value = parameters<WindowParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Window parameters are required.");
      if (!known(value->operation))
        return invalid("unknown_operation", "The window operation is unsupported.");
      if (!known(value->target))
        return invalid("unknown_window_target", "The window target is unsupported.");
      const bool moves = value->operation == WindowOperation::move ||
                         value->operation == WindowOperation::move_resize;
      const bool resizes = value->operation == WindowOperation::resize ||
                           value->operation == WindowOperation::move_resize;
      if (moves && (!value->x || !value->y))
        return invalid("missing_coordinates", "Window movement requires X and Y coordinates.");
      if (resizes && (!value->width || !value->height || *value->width <= 0 || *value->height <= 0))
        return invalid("invalid_dimensions", "Window resizing requires positive width and height.");
      return valid();
    }
    case ActionType::media: {
      const auto* value = parameters<MediaParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Media parameters are required.");
      return known(value->operation)
                 ? valid()
                 : invalid("unknown_operation", "The media operation is unsupported.");
    }
    case ActionType::volume: {
      const auto* value = parameters<VolumeParameters>(definition);
      if (!value) return invalid("parameter_type_mismatch", "Volume parameters are required.");
      if (!known(value->operation))
        return invalid("unknown_operation", "The volume operation is unsupported.");
      if (value->operation == VolumeOperation::mute_toggle && value->amount)
        return invalid("unexpected_amount", "Mute toggle does not accept an amount.");
      if (value->amount && (!std::isfinite(*value->amount) || *value->amount <= 0.0 ||
                            *value->amount > 100.0))
        return invalid("invalid_amount", "Volume amount must be greater than 0 and at most 100.");
      return valid();
    }
    case ActionType::virtual_desktop: {
      const auto* value = parameters<VirtualDesktopParameters>(definition);
      if (!value)
        return invalid("parameter_type_mismatch", "Virtual desktop parameters are required.");
      return known(value->operation)
                 ? valid()
                 : invalid("unknown_operation", "The virtual desktop operation is unsupported.");
    }
  }
  return invalid("unknown_action_type", "The action type is unsupported.");
}

std::string action_type_name(ActionType type) {
  switch (type) {
    case ActionType::keyboard_shortcut: return "keyboard";
    case ActionType::process: return "process";
    case ActionType::url: return "url";
    case ActionType::mouse: return "mouse";
    case ActionType::window: return "window";
    case ActionType::media: return "media";
    case ActionType::volume: return "volume";
    case ActionType::virtual_desktop: return "virtual_desktop";
  }
  return "unknown";
}

std::string action_operation_name(const ActionDefinition& definition) {
  if (std::holds_alternative<ProcessParameters>(definition.parameters)) return "launch";
  if (const auto* value = std::get_if<MouseParameters>(&definition.parameters)) {
    switch (value->operation) {
      case MouseOperation::click: return "click";
      case MouseOperation::double_click: return "double_click";
      case MouseOperation::button_down: return "down";
      case MouseOperation::button_up: return "up";
      case MouseOperation::move: return "move";
    }
  }
  if (const auto* value = std::get_if<WindowParameters>(&definition.parameters)) {
    switch (value->operation) {
      case WindowOperation::close: return "close";
      case WindowOperation::minimize: return "minimize";
      case WindowOperation::maximize: return "maximize";
      case WindowOperation::restore: return "restore";
      case WindowOperation::activate: return "activate";
      case WindowOperation::move: return "move";
      case WindowOperation::resize: return "resize";
      case WindowOperation::move_resize: return "move_resize";
    }
  }
  if (const auto* value = std::get_if<MediaParameters>(&definition.parameters)) {
    switch (value->operation) {
      case MediaOperation::play_pause: return "play_pause";
      case MediaOperation::next_track: return "next_track";
      case MediaOperation::previous_track: return "previous_track";
      case MediaOperation::stop: return "stop";
    }
  }
  if (const auto* value = std::get_if<VolumeParameters>(&definition.parameters)) {
    switch (value->operation) {
      case VolumeOperation::increase: return "increase";
      case VolumeOperation::decrease: return "decrease";
      case VolumeOperation::mute_toggle: return "mute_toggle";
    }
  }
  if (const auto* value = std::get_if<VirtualDesktopParameters>(&definition.parameters)) {
    switch (value->operation) {
      case VirtualDesktopOperation::next: return "next";
      case VirtualDesktopOperation::previous: return "previous";
      case VirtualDesktopOperation::create: return "create";
      case VirtualDesktopOperation::close: return "close";
    }
  }
  return definition.type == ActionType::keyboard_shortcut ? "shortcut" : "open";
}

std::string action_target_name(const ActionDefinition& definition) {
  if (const auto* value = std::get_if<ProcessParameters>(&definition.parameters)) return value->path;
  if (const auto* value = std::get_if<UrlParameters>(&definition.parameters)) return value->uri;
  if (const auto* value = std::get_if<MouseParameters>(&definition.parameters)) {
    switch (value->position.target) {
      case PositionTarget::current_cursor: return "current_cursor";
      case PositionTarget::gesture_start: return "gesture_start";
      case PositionTarget::gesture_end: return "gesture_end";
      case PositionTarget::absolute: return "absolute";
    }
  }
  if (const auto* value = std::get_if<WindowParameters>(&definition.parameters)) {
    switch (value->target) {
      case WindowTarget::gesture_window: return "gesture_window";
      case WindowTarget::foreground_window: return "foreground_window";
      case WindowTarget::window_at_gesture_start: return "window_at_gesture_start";
    }
  }
  return {};
}

}  // namespace strokes::actions
