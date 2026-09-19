#include "config/configuration_codec.h"

#include <cmath>
#include <limits>
#include <regex>
#include <string_view>
#include <unordered_set>

#include "actions/keyboard_shortcut.h"
#include "gestures/normalizer.h"

namespace strokes::config {
namespace {

using json::Array;
using json::Object;
using json::Value;

const Value* field(const Object& object, std::string_view name) {
  const auto iterator = object.find(name);
  return iterator == object.end() ? nullptr : &iterator->second;
}

template <class T>
const T* field_as(const Object& object, std::string_view name) {
  const auto* value = field(object, name);
  return value == nullptr ? nullptr : value->get_if<T>();
}

std::optional<int> integer_value(const Value& value) {
  const auto* number = value.get_if<double>();
  if (number == nullptr || !std::isfinite(*number) || std::floor(*number) != *number ||
      *number < std::numeric_limits<int>::min() || *number > std::numeric_limits<int>::max()) {
    return std::nullopt;
  }
  return static_cast<int>(*number);
}

std::string encode_button(input::ActivationButton value) {
  switch (value) {
    case input::ActivationButton::left:
      return "left";
    case input::ActivationButton::right:
      return "right";
    case input::ActivationButton::middle:
      return "middle";
    case input::ActivationButton::x_button_1:
      return "xbutton1";
    case input::ActivationButton::x_button_2:
      return "xbutton2";
  }
  return {};
}

std::optional<input::ActivationButton> decode_button(std::string_view value) {
  if (value == "left") return input::ActivationButton::left;
  if (value == "right") return input::ActivationButton::right;
  if (value == "middle") return input::ActivationButton::middle;
  if (value == "xbutton1") return input::ActivationButton::x_button_1;
  if (value == "xbutton2") return input::ActivationButton::x_button_2;
  return std::nullopt;
}

std::string encode_property(context::ApplicationProperty value) {
  switch (value) {
    case context::ApplicationProperty::process_name:
      return "process";
    case context::ApplicationProperty::window_title:
      return "title";
    case context::ApplicationProperty::window_class:
      return "class";
  }
  return {};
}

std::optional<context::ApplicationProperty> decode_property(std::string_view value) {
  if (value == "process") return context::ApplicationProperty::process_name;
  if (value == "title") return context::ApplicationProperty::window_title;
  if (value == "class") return context::ApplicationProperty::window_class;
  return std::nullopt;
}

std::string encode_mode(context::MatchMode value) {
  switch (value) {
    case context::MatchMode::exact:
      return "exact";
    case context::MatchMode::contains:
      return "contains";
    case context::MatchMode::regex:
      return "regex";
  }
  return {};
}

std::optional<context::MatchMode> decode_mode(std::string_view value) {
  if (value == "exact") return context::MatchMode::exact;
  if (value == "contains") return context::MatchMode::contains;
  if (value == "regex") return context::MatchMode::regex;
  return std::nullopt;
}

std::string encode(actions::MouseOperation value) {
  switch (value) {
    case actions::MouseOperation::click: return "click";
    case actions::MouseOperation::double_click: return "double_click";
    case actions::MouseOperation::button_down: return "down";
    case actions::MouseOperation::button_up: return "up";
    case actions::MouseOperation::move: return "move";
  }
  return {};
}

std::optional<actions::MouseOperation> decode_mouse_operation(std::string_view value) {
  if (value == "click") return actions::MouseOperation::click;
  if (value == "double_click") return actions::MouseOperation::double_click;
  if (value == "down") return actions::MouseOperation::button_down;
  if (value == "up") return actions::MouseOperation::button_up;
  if (value == "move") return actions::MouseOperation::move;
  return std::nullopt;
}

std::string encode(actions::MouseButton value) {
  switch (value) {
    case actions::MouseButton::left: return "left";
    case actions::MouseButton::right: return "right";
    case actions::MouseButton::middle: return "middle";
    case actions::MouseButton::x_button_1: return "xbutton1";
    case actions::MouseButton::x_button_2: return "xbutton2";
  }
  return {};
}

std::optional<actions::MouseButton> decode_mouse_button(std::string_view value) {
  if (value == "left") return actions::MouseButton::left;
  if (value == "right") return actions::MouseButton::right;
  if (value == "middle") return actions::MouseButton::middle;
  if (value == "xbutton1") return actions::MouseButton::x_button_1;
  if (value == "xbutton2") return actions::MouseButton::x_button_2;
  return std::nullopt;
}

std::string encode(actions::PositionTarget value) {
  switch (value) {
    case actions::PositionTarget::current_cursor: return "current_cursor";
    case actions::PositionTarget::gesture_start: return "gesture_start";
    case actions::PositionTarget::gesture_end: return "gesture_end";
    case actions::PositionTarget::absolute: return "absolute";
  }
  return {};
}

std::optional<actions::PositionTarget> decode_position_target(std::string_view value) {
  if (value == "current_cursor") return actions::PositionTarget::current_cursor;
  if (value == "gesture_start") return actions::PositionTarget::gesture_start;
  if (value == "gesture_end") return actions::PositionTarget::gesture_end;
  if (value == "absolute") return actions::PositionTarget::absolute;
  return std::nullopt;
}

std::string encode(actions::WindowOperation value) {
  switch (value) {
    case actions::WindowOperation::close: return "close";
    case actions::WindowOperation::minimize: return "minimize";
    case actions::WindowOperation::maximize: return "maximize";
    case actions::WindowOperation::restore: return "restore";
    case actions::WindowOperation::activate: return "activate";
    case actions::WindowOperation::move: return "move";
    case actions::WindowOperation::resize: return "resize";
    case actions::WindowOperation::move_resize: return "move_resize";
    case actions::WindowOperation::toggle_maximize_restore: return "toggle_maximize_restore";
    case actions::WindowOperation::center: return "center";
  }
  return {};
}

std::optional<actions::WindowOperation> decode_window_operation(std::string_view value) {
  if (value == "close") return actions::WindowOperation::close;
  if (value == "minimize") return actions::WindowOperation::minimize;
  if (value == "maximize") return actions::WindowOperation::maximize;
  if (value == "restore") return actions::WindowOperation::restore;
  if (value == "activate") return actions::WindowOperation::activate;
  if (value == "move") return actions::WindowOperation::move;
  if (value == "resize") return actions::WindowOperation::resize;
  if (value == "move_resize") return actions::WindowOperation::move_resize;
  if (value == "toggle_maximize_restore")
    return actions::WindowOperation::toggle_maximize_restore;
  if (value == "center") return actions::WindowOperation::center;
  return std::nullopt;
}

std::string encode(actions::WindowTarget value) {
  switch (value) {
    case actions::WindowTarget::gesture_window: return "gesture_window";
    case actions::WindowTarget::foreground_window: return "foreground_window";
    case actions::WindowTarget::window_at_gesture_start: return "window_at_gesture_start";
  }
  return {};
}

std::optional<actions::WindowTarget> decode_window_target(std::string_view value) {
  if (value == "gesture_window") return actions::WindowTarget::gesture_window;
  if (value == "foreground_window") return actions::WindowTarget::foreground_window;
  if (value == "window_at_gesture_start") return actions::WindowTarget::window_at_gesture_start;
  return std::nullopt;
}

std::string encode(actions::MediaOperation value) {
  switch (value) {
    case actions::MediaOperation::play_pause: return "play_pause";
    case actions::MediaOperation::next_track: return "next_track";
    case actions::MediaOperation::previous_track: return "previous_track";
    case actions::MediaOperation::stop: return "stop";
  }
  return {};
}

std::optional<actions::MediaOperation> decode_media_operation(std::string_view value) {
  if (value == "play_pause") return actions::MediaOperation::play_pause;
  if (value == "next_track") return actions::MediaOperation::next_track;
  if (value == "previous_track") return actions::MediaOperation::previous_track;
  if (value == "stop") return actions::MediaOperation::stop;
  return std::nullopt;
}

std::string encode(actions::VolumeOperation value) {
  switch (value) {
    case actions::VolumeOperation::increase: return "increase";
    case actions::VolumeOperation::decrease: return "decrease";
    case actions::VolumeOperation::mute_toggle: return "mute_toggle";
  }
  return {};
}

std::optional<actions::VolumeOperation> decode_volume_operation(std::string_view value) {
  if (value == "increase") return actions::VolumeOperation::increase;
  if (value == "decrease") return actions::VolumeOperation::decrease;
  if (value == "mute_toggle") return actions::VolumeOperation::mute_toggle;
  return std::nullopt;
}

std::string encode(actions::VirtualDesktopOperation value) {
  switch (value) {
    case actions::VirtualDesktopOperation::next: return "next";
    case actions::VirtualDesktopOperation::previous: return "previous";
    case actions::VirtualDesktopOperation::create: return "create";
    case actions::VirtualDesktopOperation::close: return "close";
  }
  return {};
}

std::optional<actions::VirtualDesktopOperation> decode_desktop_operation(std::string_view value) {
  if (value == "next") return actions::VirtualDesktopOperation::next;
  if (value == "previous") return actions::VirtualDesktopOperation::previous;
  if (value == "create") return actions::VirtualDesktopOperation::create;
  if (value == "close") return actions::VirtualDesktopOperation::close;
  return std::nullopt;
}

Value encode_position(const actions::PositionDefinition& position) {
  Object result{{"type", encode(position.target)}};
  if (position.absolute) {
    result.emplace("x", position.absolute->x);
    result.emplace("y", position.absolute->y);
  }
  return result;
}

std::optional<actions::PositionDefinition> decode_position(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (!object) return std::nullopt;
  const auto* type = field_as<std::string>(*object, "type");
  if (!type) return std::nullopt;
  const auto target = decode_position_target(*type);
  if (!target) return std::nullopt;
  actions::PositionDefinition result{*target, std::nullopt};
  if (*target == actions::PositionTarget::absolute) {
    const auto* x = field_as<double>(*object, "x");
    const auto* y = field_as<double>(*object, "y");
    if (!x || !y || !std::isfinite(*x) || !std::isfinite(*y)) return std::nullopt;
    result.absolute = gestures::Point{*x, *y};
  }
  return result;
}

void add_optional_integer(Object& object, std::string name, const std::optional<int>& value) {
  if (value) object.emplace(std::move(name), double(*value));
}

std::optional<int> optional_integer(const Object& object, std::string_view name, bool& valid) {
  const auto* value = field(object, name);
  if (!value) return std::nullopt;
  const auto decoded = integer_value(*value);
  if (!decoded) valid = false;
  return decoded;
}

Value encode_action(const actions::ActionDefinition& action) {
  Object result{{"version", double(action.version)}};
  switch (action.type) {
    case actions::ActionType::keyboard_shortcut: {
      result.emplace("type", "keyboard");
      const auto* value = std::get_if<actions::KeyboardParameters>(&action.parameters);
      if (value) result.emplace("shortcut", value->shortcut);
      break;
    }
    case actions::ActionType::process: {
      result.emplace("type", "process");
      const auto* value = std::get_if<actions::ProcessParameters>(&action.parameters);
      if (value) {
        result.emplace("operation", "launch");
        result.emplace("path", value->path);
        if (!value->arguments.empty()) result.emplace("arguments", value->arguments);
        if (!value->working_directory.empty())
          result.emplace("working_directory", value->working_directory);
      }
      break;
    }
    case actions::ActionType::url: {
      result.emplace("type", "url");
      const auto* value = std::get_if<actions::UrlParameters>(&action.parameters);
      if (value) result.emplace("url", value->uri);
      break;
    }
    case actions::ActionType::mouse: {
      result.emplace("type", "mouse");
      const auto* value = std::get_if<actions::MouseParameters>(&action.parameters);
      if (value) {
        result.emplace("operation", encode(value->operation));
        if (value->button) result.emplace("button", encode(*value->button));
        result.emplace("position", encode_position(value->position));
      }
      break;
    }
    case actions::ActionType::window: {
      result.emplace("type", "window");
      const auto* value = std::get_if<actions::WindowParameters>(&action.parameters);
      if (value) {
        result.emplace("operation", encode(value->operation));
        result.emplace("target", encode(value->target));
        add_optional_integer(result, "x", value->x);
        add_optional_integer(result, "y", value->y);
        add_optional_integer(result, "width", value->width);
        add_optional_integer(result, "height", value->height);
      }
      break;
    }
    case actions::ActionType::media: {
      result.emplace("type", "media");
      const auto* value = std::get_if<actions::MediaParameters>(&action.parameters);
      if (value) result.emplace("operation", encode(value->operation));
      break;
    }
    case actions::ActionType::volume: {
      result.emplace("type", "volume");
      const auto* value = std::get_if<actions::VolumeParameters>(&action.parameters);
      if (value) {
        result.emplace("operation", encode(value->operation));
        if (value->amount) result.emplace("amount", *value->amount);
      }
      break;
    }
    case actions::ActionType::virtual_desktop: {
      result.emplace("type", "virtual_desktop");
      const auto* value = std::get_if<actions::VirtualDesktopParameters>(&action.parameters);
      if (value) result.emplace("operation", encode(value->operation));
      break;
    }
  }
  return result;
}

std::optional<actions::ActionDefinition> decode_action(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (!object) return std::nullopt;
  const auto* type = field_as<std::string>(*object, "type");
  if (!type) return std::nullopt;
  int version = actions::ActionDefinition::current_version;
  if (const auto* encoded_version = field(*object, "version")) {
    const auto decoded = integer_value(*encoded_version);
    if (!decoded) return std::nullopt;
    version = *decoded;
  }

  std::optional<actions::ActionDefinition> result;
  if (*type == "keyboard") {
    const auto* shortcut = field_as<std::string>(*object, "shortcut");
    if (shortcut) result = actions::ActionDefinition::keyboard(*shortcut);
  } else if (*type == "process") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto* path = field_as<std::string>(*object, "path");
    if (operation && *operation == "launch" && path) {
      const auto* arguments = field_as<std::string>(*object, "arguments");
      const auto* working = field_as<std::string>(*object, "working_directory");
      result = actions::ActionDefinition{
          version, actions::ActionType::process,
          actions::ProcessParameters{actions::ProcessOperation::launch, *path,
                                     arguments ? *arguments : std::string{},
                                     working ? *working : std::string{}}};
    }
  } else if (*type == "url") {
    const auto* uri = field_as<std::string>(*object, "url");
    if (uri) result = actions::ActionDefinition{version, actions::ActionType::url,
                                                actions::UrlParameters{*uri}};
  } else if (*type == "mouse") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto* encoded_position = field(*object, "position");
    if (operation) {
      const auto decoded_operation = decode_mouse_operation(*operation);
      const auto position = encoded_position
                                ? decode_position(*encoded_position)
                                : std::optional<actions::PositionDefinition>{
                                      actions::PositionDefinition{}};
      std::optional<actions::MouseButton> button;
      bool button_valid = true;
      if (const auto* encoded_button = field_as<std::string>(*object, "button")) {
        button = decode_mouse_button(*encoded_button);
        button_valid = button.has_value();
      } else if (field(*object, "button")) {
        button_valid = false;
      }
      if (decoded_operation && position && button_valid)
        result = actions::ActionDefinition{
            version, actions::ActionType::mouse,
            actions::MouseParameters{*decoded_operation, button, *position}};
    }
  } else if (*type == "window") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto* target = field_as<std::string>(*object, "target");
    if (operation && target) {
      const auto decoded_operation = decode_window_operation(*operation);
      const auto decoded_target = decode_window_target(*target);
      bool integers_valid = true;
      auto x = optional_integer(*object, "x", integers_valid);
      auto y = optional_integer(*object, "y", integers_valid);
      auto width = optional_integer(*object, "width", integers_valid);
      auto height = optional_integer(*object, "height", integers_valid);
      if (decoded_operation && decoded_target && integers_valid)
        result = actions::ActionDefinition{
            version, actions::ActionType::window,
            actions::WindowParameters{*decoded_operation, *decoded_target, x, y, width, height}};
    }
  } else if (*type == "media") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto decoded = operation ? decode_media_operation(*operation) : std::nullopt;
    if (decoded) result = actions::ActionDefinition{version, actions::ActionType::media,
                                                    actions::MediaParameters{*decoded}};
  } else if (*type == "volume") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto decoded = operation ? decode_volume_operation(*operation) : std::nullopt;
    std::optional<double> amount;
    bool amount_valid = true;
    if (const auto* encoded_amount = field_as<double>(*object, "amount"))
      amount = *encoded_amount;
    else if (field(*object, "amount"))
      amount_valid = false;
    if (decoded && amount_valid)
      result = actions::ActionDefinition{version, actions::ActionType::volume,
                                         actions::VolumeParameters{*decoded, amount}};
  } else if (*type == "virtual_desktop") {
    const auto* operation = field_as<std::string>(*object, "operation");
    const auto decoded = operation ? decode_desktop_operation(*operation) : std::nullopt;
    if (decoded)
      result = actions::ActionDefinition{version, actions::ActionType::virtual_desktop,
                                         actions::VirtualDesktopParameters{*decoded}};
  }

  if (!result) return std::nullopt;
  result->version = version;
  return actions::validate(*result).valid ? result : std::nullopt;
}

Object encode_actions(const actions::ActionResolver::GlobalActions& actions) {
  Object object;
  for (const auto& [gesture_id, action] : actions) {
    object.emplace(gesture_id, encode_action(action));
  }
  return object;
}

bool decode_actions(const Value& value, actions::ActionResolver::GlobalActions& actions,
                    std::size_t& skipped, std::string_view scope,
                    std::vector<std::string>& invalid_actions) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return false;
  for (const auto& [gesture_id, encoded] : *object) {
    auto action = decode_action(encoded);
    if (gesture_id.empty() || !action) {
      ++skipped;
      invalid_actions.push_back(std::string(scope) + (gesture_id.empty() ? "<empty>" : gesture_id));
      continue;
    }
    actions.emplace(gesture_id, std::move(*action));
  }
  return true;
}

bool decode_version(const Object& object, int current_version, int& version) {
  const auto* encoded = field(object, "version");
  if (encoded == nullptr) {
    version = current_version;
    return true;
  }
  const auto decoded = integer_value(*encoded);
  if (!decoded || *decoded < 1 || *decoded > current_version) return false;
  version = *decoded;
  return true;
}

}  // namespace

Value encode(const GlobalOptions& options) {
  return Object{{"version", double(options.version)},
                {"gestures_enabled", options.gestures_enabled},
                {"gesture_button", encode_button(options.gesture_button)},
                {"movement_threshold", options.movement_threshold},
                {"minimum_point_distance", options.minimum_point_distance},
                {"maximum_points", double(options.maximum_points)},
                {"recognition_threshold", options.recognition_threshold},
                {"overlay", Object{{"enabled", options.overlay.enabled},
                                   {"line_width", double(options.overlay.line_width)},
                                   {"opacity", options.overlay.opacity},
                                   {"color", double(options.overlay.color)}}}};
}

DecodeResult<GlobalOptions> decode_options(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return {{}, "global configuration must be an object"};
  GlobalOptions result;
  if (!decode_version(*object, GlobalOptions::current_version, result.version)) {
    return {{}, "unsupported global configuration version"};
  }

  if (const auto* encoded = field(*object, "gestures_enabled")) {
    const auto* decoded = encoded->get_if<bool>();
    if (decoded == nullptr) return {{}, "gestures_enabled must be a boolean"};
    result.gestures_enabled = *decoded;
  }
  if (const auto* encoded = field(*object, "gesture_button")) {
    const auto* text = encoded->get_if<std::string>();
    const auto decoded = text == nullptr ? std::nullopt : decode_button(*text);
    if (!decoded) return {{}, "gesture_button is invalid"};
    result.gesture_button = *decoded;
  }
  if (const auto* encoded = field(*object, "movement_threshold")) {
    const auto* decoded = encoded->get_if<double>();
    if (decoded == nullptr || !std::isfinite(*decoded) || *decoded < 0.0 ||
        *decoded > GlobalOptions::maximum_movement_threshold) {
      return {{}, "movement_threshold must be between 0 and " +
                       std::to_string(GlobalOptions::maximum_movement_threshold)};
    }
    result.movement_threshold = *decoded;
  }
  if (const auto* encoded = field(*object, "minimum_point_distance")) {
    const auto* decoded = encoded->get_if<double>();
    if (decoded == nullptr || !std::isfinite(*decoded) || *decoded < 0.0 ||
        *decoded > GlobalOptions::maximum_point_distance) {
      return {{}, "minimum_point_distance must be between 0 and " +
                       std::to_string(GlobalOptions::maximum_point_distance)};
    }
    result.minimum_point_distance = *decoded;
  }
  if (const auto* encoded = field(*object, "maximum_points")) {
    const auto decoded = integer_value(*encoded);
    if (!decoded || *decoded < 2 ||
        static_cast<std::size_t>(*decoded) > GlobalOptions::maximum_point_limit)
      return {{}, "maximum_points must be between 2 and " +
                       std::to_string(GlobalOptions::maximum_point_limit)};
    result.maximum_points = static_cast<std::size_t>(*decoded);
  }
  if (const auto* encoded = field(*object, "recognition_threshold")) {
    const auto* decoded = encoded->get_if<double>();
    if (decoded == nullptr || !std::isfinite(*decoded) || *decoded < 0.0 || *decoded > 1.0) {
      return {{}, "recognition_threshold must be between 0 and 1"};
    }
    result.recognition_threshold = *decoded;
  }
  std::string warning;
  if (result.minimum_point_distance > result.movement_threshold) {
    result.minimum_point_distance = result.movement_threshold;
    warning = "minimum_point_distance was clamped to movement_threshold";
  }

  if (const auto* encoded = field(*object, "overlay")) {
    const auto* overlay = encoded->get_if<Object>();
    if (overlay == nullptr) return {{}, "overlay must be an object"};
    if (const auto* item = field(*overlay, "enabled")) {
      const auto* decoded = item->get_if<bool>();
      if (decoded == nullptr) return {{}, "overlay.enabled must be a boolean"};
      result.overlay.enabled = *decoded;
    }
    if (const auto* item = field(*overlay, "line_width")) {
      const auto decoded = integer_value(*item);
      if (!decoded || *decoded <= 0 || *decoded > GlobalOptions::maximum_overlay_line_width)
        return {{}, "overlay.line_width must be between 1 and " +
                         std::to_string(GlobalOptions::maximum_overlay_line_width)};
      result.overlay.line_width = *decoded;
    }
    if (const auto* item = field(*overlay, "opacity")) {
      const auto* decoded = item->get_if<double>();
      if (decoded == nullptr || !std::isfinite(*decoded) ||
          *decoded < GlobalOptions::minimum_overlay_opacity || *decoded > 1.0) {
        return {{}, "overlay.opacity must be between " +
                        std::to_string(GlobalOptions::minimum_overlay_opacity) + " and 1"};
      }
      result.overlay.opacity = *decoded;
    }
    if (const auto* item = field(*overlay, "color")) {
      const auto decoded = integer_value(*item);
      if (!decoded || *decoded <= 0 || *decoded > 0xFFFFFF) {
        return {{}, "overlay.color must be a non-black 24-bit integer"};
      }
      result.overlay.color = static_cast<std::uint32_t>(*decoded);
    }
  }
  return {result, {}, std::move(warning)};
}

Value encode(const GestureFile& file) {
  Array gestures;
  for (const auto& gesture : file.gestures) {
    Array templates;
    for (const auto& gesture_template : gesture.templates) {
      Array points;
      for (const auto& point : gesture_template.points) {
        points.emplace_back(Object{{"x", point.x}, {"y", point.y}});
      }
      templates.emplace_back(Object{{"id", gesture_template.id}, {"points", std::move(points)}});
    }
    gestures.emplace_back(Object{{"id", gesture.id},
                                 {"name", gesture.name},
                                 {"enabled", gesture.enabled},
                                 {"templates", std::move(templates)}});
  }
  return Object{{"version", double(file.version)}, {"gestures", std::move(gestures)}};
}

DecodeResult<GestureFile> decode_gestures(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return {{}, "gesture file must be an object"};
  GestureFile result;
  if (!decode_version(*object, GestureFile::current_version, result.version)) {
    return {{}, "unsupported gesture file version"};
  }
  const auto* encoded_gestures = field(*object, "gestures");
  const auto* gestures = encoded_gestures == nullptr ? nullptr : encoded_gestures->get_if<Array>();
  if (gestures == nullptr) return {{}, "gestures must be an array"};

  std::size_t skipped = 0;
  std::unordered_set<std::string> gesture_ids;
  const auto normalizer = gestures::default_normalizer();
  for (const auto& encoded_gesture : *gestures) {
    const auto* gesture = encoded_gesture.get_if<Object>();
    const auto* id = gesture == nullptr ? nullptr : field_as<std::string>(*gesture, "id");
    const auto* name = gesture == nullptr ? nullptr : field_as<std::string>(*gesture, "name");
    const auto* encoded_templates = gesture == nullptr ? nullptr : field(*gesture, "templates");
    const auto* templates =
        encoded_templates == nullptr ? nullptr : encoded_templates->get_if<Array>();
    if (id == nullptr || id->empty() || name == nullptr || name->empty() || templates == nullptr ||
        gesture_ids.contains(*id)) {
      ++skipped;
      continue;
    }

    bool enabled = true;
    if (const auto* encoded = field(*gesture, "enabled")) {
      const auto* decoded = encoded->get_if<bool>();
      if (decoded == nullptr) {
        ++skipped;
        enabled = false;
      } else {
        enabled = *decoded;
      }
    }
    gestures::GestureDefinition definition{*id, *name, enabled, {}};
    std::unordered_set<std::string> template_ids;
    for (const auto& encoded_template : *templates) {
      const auto* gesture_template = encoded_template.get_if<Object>();
      const auto* template_id =
          gesture_template == nullptr ? nullptr : field_as<std::string>(*gesture_template, "id");
      const auto* encoded_points =
          gesture_template == nullptr ? nullptr : field(*gesture_template, "points");
      const auto* points = encoded_points == nullptr ? nullptr : encoded_points->get_if<Array>();
      if (template_id == nullptr || template_id->empty() || points == nullptr ||
          template_ids.contains(*template_id)) {
        ++skipped;
        continue;
      }
      gestures::Stroke stroke;
      stroke.reserve(points->size());
      bool valid = true;
      for (const auto& encoded_point : *points) {
        const auto* point = encoded_point.get_if<Object>();
        const auto* x = point == nullptr ? nullptr : field_as<double>(*point, "x");
        const auto* y = point == nullptr ? nullptr : field_as<double>(*point, "y");
        if (x == nullptr || y == nullptr || !std::isfinite(*x) || !std::isfinite(*y)) {
          valid = false;
          break;
        }
        stroke.push_back({*x, *y});
      }
      if (!valid || !normalizer.normalize(stroke)) {
        ++skipped;
        continue;
      }
      template_ids.insert(*template_id);
      definition.templates.push_back({*template_id, std::move(stroke)});
    }
    if (definition.enabled && definition.templates.empty()) {
      definition.enabled = false;
      ++skipped;
    }
    gesture_ids.insert(*id);
    result.gestures.push_back(std::move(definition));
  }
  return {std::move(result), {},
          skipped == 0 ? std::string{}
                       : "recovered with " + std::to_string(skipped) +
                             " invalid or disabled gesture/template entries"};
}

Value encode(const ProfileFile& file) {
  Array profiles;
  for (const auto& profile : file.profiles) {
    Array criteria;
    for (const auto& criterion : profile.criteria) {
      criteria.emplace_back(Object{{"property", encode_property(criterion.property)},
                                   {"mode", encode_mode(criterion.mode)},
                                   {"value", criterion.value}});
    }
    profiles.emplace_back(Object{{"id", profile.id},
                                 {"name", profile.name},
                                 {"enabled", profile.enabled},
                                 {"criteria", std::move(criteria)},
                                 {"actions", encode_actions(profile.actions_by_gesture)}});
  }
  return Object{{"version", double(file.version)},
                {"profiles", std::move(profiles)},
                {"global_actions", encode_actions(file.global_actions)}};
}

DecodeResult<ProfileFile> decode_profiles(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return {{}, "profile file must be an object"};
  ProfileFile result;
  if (!decode_version(*object, ProfileFile::current_version, result.version)) {
    return {{}, "unsupported profile file version"};
  }
  const auto* encoded_profiles = field(*object, "profiles");
  const auto* profiles = encoded_profiles == nullptr ? nullptr : encoded_profiles->get_if<Array>();
  if (profiles == nullptr) return {{}, "profiles must be an array"};

  std::size_t skipped = 0;
  std::vector<std::string> invalid_actions;
  if (const auto* global_actions = field(*object, "global_actions")) {
    if (!decode_actions(*global_actions, result.global_actions, skipped, "global:",
                        invalid_actions)) {
      return {{}, "global_actions must be an object"};
    }
  }
  std::unordered_set<std::string> profile_ids;
  for (const auto& encoded_profile : *profiles) {
    const auto* profile = encoded_profile.get_if<Object>();
    const auto* id = profile == nullptr ? nullptr : field_as<std::string>(*profile, "id");
    const auto* name = profile == nullptr ? nullptr : field_as<std::string>(*profile, "name");
    if (id == nullptr || id->empty() || name == nullptr || name->empty() ||
        profile_ids.contains(*id)) {
      ++skipped;
      continue;
    }
    bool enabled = true;
    if (const auto* encoded = field(*profile, "enabled")) {
      const auto* decoded = encoded->get_if<bool>();
      if (decoded == nullptr) {
        ++skipped;
        enabled = false;
      } else {
        enabled = *decoded;
      }
    }
    context::ApplicationProfile decoded{*id, *name, enabled, {}, {}};
    if (const auto* encoded_criteria = field(*profile, "criteria")) {
      const auto* criteria = encoded_criteria->get_if<Array>();
      if (criteria == nullptr) {
        ++skipped;
        decoded.enabled = false;
      } else for (const auto& encoded_criterion : *criteria) {
        const auto* criterion = encoded_criterion.get_if<Object>();
        const auto* property_text =
            criterion == nullptr ? nullptr : field_as<std::string>(*criterion, "property");
        const auto* mode_text =
            criterion == nullptr ? nullptr : field_as<std::string>(*criterion, "mode");
        const auto* criterion_value =
            criterion == nullptr ? nullptr : field_as<std::string>(*criterion, "value");
        const auto property =
            property_text == nullptr ? std::nullopt : decode_property(*property_text);
        const auto mode = mode_text == nullptr ? std::nullopt : decode_mode(*mode_text);
        if (!property || !mode || criterion_value == nullptr || criterion_value->empty()) {
          ++skipped;
          continue;
        }
        context::MatchCriterion candidate{*property, *mode, *criterion_value};
        if (!context::prepare_criterion(candidate)) {
          ++skipped;
          continue;
        }
        decoded.criteria.push_back(std::move(candidate));
      }
    } else {
      ++skipped;
    }
    if (const auto* encoded_actions = field(*profile, "actions")) {
      if (!decode_actions(*encoded_actions, decoded.actions_by_gesture, skipped,
                          "profile:" + *id + ":", invalid_actions)) {
        ++skipped;
      }
    }
    if (decoded.criteria.empty()) decoded.enabled = false;
    profile_ids.insert(*id);
    result.profiles.push_back(std::move(decoded));
  }
  std::string warning;
  if (skipped != 0) {
    warning = "recovered with " + std::to_string(skipped) +
              " invalid or disabled profile/criterion/action entries";
    if (!invalid_actions.empty()) {
      warning += "; invalid action mappings: ";
      for (std::size_t index = 0; index < invalid_actions.size(); ++index) {
        if (index != 0) warning += ", ";
        warning += invalid_actions[index];
      }
    }
  }
  return {std::move(result), {}, std::move(warning)};
}

}  // namespace strokes::config
