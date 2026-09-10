#include "config/configuration_codec.h"
#include "test_support.h"

namespace strokes::tests {
namespace {
using namespace config;

template <class T, class Decoder>
auto round_trip(const T& value, Decoder decoder) {
  const auto text = json::serialize(encode(value));
  const auto parsed = json::parse(text);
  check(static_cast<bool>(parsed), "encoded configuration is valid JSON");
  if (!parsed) return decoder(json::Value{});
  return decoder(*parsed.value);
}

void global_tests() {
  GlobalOptions input;
  input.gestures_enabled = false;
  input.gesture_button = input::ActivationButton::x_button_2;
  input.movement_threshold = 12;
  input.minimum_point_distance = 3;
  input.maximum_points = 512;
  input.recognition_threshold = .92;
  input.overlay = {false, 7, .5, 0x123456};
  auto result = round_trip(input, decode_options);
  check(static_cast<bool>(result), "global options round trip");
  check(result.value->gesture_button == input::ActivationButton::x_button_2 &&
            result.value->maximum_points == 512,
        "global input settings are retained");
  check(result.value->overlay.line_width == 7 && result.value->overlay.color == 0x123456,
        "overlay settings are retained");
  auto invalid = encode(input);
  *invalid.get_if<json::Object>()->at("recognition_threshold").get_if<double>() = 2;
  check(!decode_options(invalid), "invalid recognition threshold is rejected");
  check(!decode_options(json::Object{{"version", 99.0}}),
        "unsupported global configuration version is rejected");
  check(!decode_options(json::Object{{"maximum_points", 1000001.0}}),
        "unbounded maximum point counts are rejected");
  auto clamped = decode_options(
      json::Object{{"movement_threshold", 1.0}, {"minimum_point_distance", 2.0}});
  check(clamped && clamped.value->minimum_point_distance == 1.0 && !clamped.error.empty(),
        "point distance is recoverably clamped to the movement threshold");

  const auto documented = json::parse(R"({
        "gesture_button": "right",
        "movement_threshold": 8,
        "recognition_threshold": 0.80,
        "overlay": {"enabled": true, "line_width": 4, "opacity": 0.85}
    })");
  check(static_cast<bool>(documented), "documented global configuration is valid JSON");
  if (!documented) return;
  const auto documented_result = decode_options(*documented.value);
  check(documented_result && documented_result.value->maximum_points == 4096 &&
            documented_result.value->overlay.color == 0x00A0FF,
        "documented global configuration loads with defaults for omitted fields");
}

void gesture_tests() {
  GestureFile input;
  input.gestures.push_back(
      {"caret",
       "Caret",
       true,
       {{"one", {{0, 10}, {10, 0}, {20, 10}}}, {"two", {{1, 11}, {11, 1}, {21, 11}}}}});
  auto result = round_trip(input, decode_gestures);
  check(static_cast<bool>(result), "gesture file round trips");
  check(result.value->gestures.size() == 1 && result.value->gestures[0].templates.size() == 2,
        "gesture templates and identity are retained");
  GestureFile invalid;
  invalid.gestures.push_back({"bad", "Bad", true, {}});
  auto recovered = decode_gestures(encode(invalid));
  check(recovered && recovered.value->gestures.size() == 1 &&
            !recovered.value->gestures[0].enabled && !recovered.error.empty(),
        "untrained gesture is retained but disabled with a warning");

  auto mixed = encode(input);
  auto* gestures = mixed.get_if<json::Object>()->at("gestures").get_if<json::Array>();
  gestures->push_back(json::Object{{"id", "broken"}});
  recovered = decode_gestures(mixed);
  check(recovered && recovered.value->gestures.size() == 1 && !recovered.error.empty(),
        "valid gestures survive a malformed sibling entry");

  const auto documented = json::parse(R"({
        "gestures": [{"id": "left", "name": "Left", "enabled": true, "templates": []}]
    })");
  check(static_cast<bool>(documented), "documented gesture configuration is valid JSON");
  if (!documented) return;
  const auto documented_result = decode_gestures(*documented.value);
  check(documented_result && documented_result.value->gestures.size() == 1 &&
            !documented_result.value->gestures[0].enabled,
        "documented gesture configuration loads and disables its untrained gesture");
}

void profile_tests() {
  ProfileFile input;
  input.global_actions.emplace("left",
                               actions::Action{actions::ActionType::keyboard_shortcut, "ALT+LEFT"});
  input.profiles.push_back(
      {"chrome",
       "Chrome",
       true,
       {{context::ApplicationProperty::process_name, context::MatchMode::exact, "chrome.exe"},
        {context::ApplicationProperty::window_title, context::MatchMode::contains, "GitHub"}},
       {{"left", {actions::ActionType::keyboard_shortcut, "CTRL+SHIFT+TAB"}}}});
  auto result = round_trip(input, decode_profiles);
  check(static_cast<bool>(result), "profile file round trips");
  check(result.value->profiles.size() == 1 && result.value->profiles[0].criteria.size() == 2,
        "profile criteria are retained");
  check(result.value->global_actions.at("left").value == "ALT+LEFT" &&
            result.value->profiles[0].actions_by_gesture.at("left").value == "CTRL+SHIFT+TAB",
        "global and profile actions are retained");
  auto mixed = encode(input);
  mixed.get_if<json::Object>()->at("profiles").get_if<json::Array>()->push_back(false);
  result = decode_profiles(mixed);
  check(result && result.value->profiles.size() == 1 && !result.error.empty(),
        "valid profiles survive a malformed sibling entry");
  auto malformed_actions = encode(input);
  malformed_actions.get_if<json::Object>()->at("profiles").get_if<json::Array>()->front()
      .get_if<json::Object>()->insert_or_assign("actions", json::Array{});
  result = decode_profiles(malformed_actions);
  check(result && result.value->profiles.size() == 1 &&
            result.value->profiles.front().actions_by_gesture.empty(),
        "malformed profile actions are discarded without dropping the profile");

  const auto documented = json::parse(R"({
        "profiles": [{
            "id": "chrome", "name": "Google Chrome",
            "match": {"process": "chrome.exe"},
            "actions": {"left": {"type": "keyboard", "shortcut": "CTRL+SHIFT+TAB"}}
        }],
        "global_actions": {"left": {"type": "keyboard", "shortcut": "ALT+LEFT"}}
    })");
  check(static_cast<bool>(documented), "documented profile configuration is valid JSON");
  if (!documented) return;
  const auto documented_result = decode_profiles(*documented.value);
  check(documented_result && documented_result.value->profiles.size() == 1 &&
            documented_result.value->profiles[0].criteria.size() == 1,
        "documented legacy profile configuration loads as an exact process match");
}
}  // namespace

void run_configuration_tests() {
  global_tests();
  gesture_tests();
  profile_tests();
}
}  // namespace strokes::tests
