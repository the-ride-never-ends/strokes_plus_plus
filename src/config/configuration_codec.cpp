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

Value encode_action(const actions::Action& action) {
  return Object{{"type", "keyboard"}, {"shortcut", action.value}};
}

std::optional<actions::Action> decode_action(const Value& value) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return std::nullopt;
  const auto* type = field_as<std::string>(*object, "type");
  const auto* shortcut = field_as<std::string>(*object, "shortcut");
  if (type == nullptr || *type != "keyboard" || shortcut == nullptr ||
      !actions::parse_shortcut(*shortcut)) {
    return std::nullopt;
  }
  return actions::Action{actions::ActionType::keyboard_shortcut, *shortcut};
}

Object encode_actions(const actions::ActionResolver::GlobalActions& actions) {
  Object object;
  for (const auto& [gesture_id, action] : actions) {
    object.emplace(gesture_id, encode_action(action));
  }
  return object;
}

bool decode_actions(const Value& value, actions::ActionResolver::GlobalActions& actions,
                    std::size_t& skipped) {
  const auto* object = value.get_if<Object>();
  if (object == nullptr) return false;
  for (const auto& [gesture_id, encoded] : *object) {
    auto action = decode_action(encoded);
    if (gesture_id.empty() || !action) {
      ++skipped;
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
  if (!decoded || *decoded != current_version) return false;
  version = *decoded;
  return true;
}

bool legacy_match(const Object& profile, std::vector<context::MatchCriterion>& criteria) {
  const auto* encoded_match = field(profile, "match");
  const auto* match = encoded_match == nullptr ? nullptr : encoded_match->get_if<Object>();
  if (match == nullptr) return false;
  for (const auto property :
       {context::ApplicationProperty::process_name, context::ApplicationProperty::window_title,
        context::ApplicationProperty::window_class}) {
    const auto* value = field_as<std::string>(*match, encode_property(property));
    if (value != nullptr && !value->empty()) {
      criteria.push_back({property, context::MatchMode::exact, *value});
    }
  }
  return !criteria.empty();
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
  if (const auto* global_actions = field(*object, "global_actions")) {
    if (!decode_actions(*global_actions, result.global_actions, skipped)) {
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
    } else if (!legacy_match(*profile, decoded.criteria)) {
      ++skipped;
    }
    if (const auto* encoded_actions = field(*profile, "actions")) {
      if (!decode_actions(*encoded_actions, decoded.actions_by_gesture, skipped)) {
        ++skipped;
      }
    }
    if (decoded.criteria.empty()) decoded.enabled = false;
    profile_ids.insert(*id);
    result.profiles.push_back(std::move(decoded));
  }
  return {std::move(result), {},
          skipped == 0 ? std::string{}
                       : "recovered with " + std::to_string(skipped) +
                             " invalid or disabled profile/criterion/action entries"};
}

}  // namespace strokes::config
