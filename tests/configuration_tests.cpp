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
  check(clamped && clamped.value->minimum_point_distance == 1.0 && !clamped.warning.empty() &&
            clamped.error.empty(),
        "point distance is recoverably clamped to the movement threshold");
  for (auto [field, value] :
       std::initializer_list<std::pair<const char*, json::Value>>{
           {"movement_threshold", -1.0}, {"minimum_point_distance", 1001.0},
           {"maximum_points", 1.0}, {"recognition_threshold", -0.1}}) {
    check(!decode_options(json::Object{{field, std::move(value)}}),
          std::string("invalid bound is rejected for ") + field);
  }
  check(!decode_options(json::Object{{"overlay", json::Object{{"line_width", 101.0}}}}),
        "oversized overlay line width is rejected");
  check(!decode_options(json::Object{{"overlay", json::Object{{"opacity", 0.0}}}}),
        "zero overlay opacity is rejected");
  check(!decode_options(json::Object{{"overlay", json::Object{{"opacity", 0.001}}}}),
        "an overlay opacity that rounds to a transparent window is rejected");
  check(static_cast<bool>(decode_options(json::Object{
            {"overlay", json::Object{{"opacity", GlobalOptions::minimum_overlay_opacity}}}})),
        "the smallest visible overlay opacity is accepted");
  check(!decode_options(json::Object{{"overlay", json::Object{{"color", 0.0}}}}),
        "black overlay color is rejected");

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
            !recovered.value->gestures[0].enabled && !recovered.warning.empty(),
        "untrained gesture is retained but disabled with a warning");

  auto mixed = encode(input);
  auto* gestures = mixed.get_if<json::Object>()->at("gestures").get_if<json::Array>();
  gestures->push_back(json::Object{{"id", "broken"}});
  recovered = decode_gestures(mixed);
  check(recovered && recovered.value->gestures.size() == 1 && !recovered.warning.empty(),
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
  input.global_actions.emplace("left", actions::ActionDefinition::keyboard("ALT+LEFT"));
  input.global_actions.emplace("minimize",
                               actions::ActionDefinition::keyboard("ALT+SPACE,N"));
  input.global_actions.emplace(
      "process", actions::ActionDefinition{1, actions::ActionType::process,
                                            actions::ProcessParameters{
                                                actions::ProcessOperation::launch, "tool.exe",
                                                "--flag", "C:\\Tools"}});
  input.global_actions.emplace(
      "url", actions::ActionDefinition{1, actions::ActionType::url,
                                        actions::UrlParameters{"https://example.com"}});
  input.global_actions.emplace(
      "mouse", actions::ActionDefinition{
                   1, actions::ActionType::mouse,
                   actions::MouseParameters{
                       actions::MouseOperation::double_click, actions::MouseButton::x_button_1,
                       {actions::PositionTarget::absolute, gestures::Point{-250, 400}}}});
  input.global_actions.emplace(
      "window", actions::ActionDefinition{
                    1, actions::ActionType::window,
                    actions::WindowParameters{actions::WindowOperation::move_resize,
                                              actions::WindowTarget::gesture_window, -100, 20,
                                              1200, 800}});
  input.global_actions.emplace(
      "media", actions::ActionDefinition{1, actions::ActionType::media,
                                          actions::MediaParameters{
                                              actions::MediaOperation::previous_track}});
  input.global_actions.emplace(
      "volume", actions::ActionDefinition{1, actions::ActionType::volume,
                                           actions::VolumeParameters{
                                               actions::VolumeOperation::decrease, 7.5}});
  input.global_actions.emplace(
      "desktop", actions::ActionDefinition{
                     1, actions::ActionType::virtual_desktop,
                     actions::VirtualDesktopParameters{
                         actions::VirtualDesktopOperation::previous}});
  input.global_actions.emplace(
      "process-minimal",
      actions::ActionDefinition{1, actions::ActionType::process,
                                actions::ProcessParameters{actions::ProcessOperation::launch,
                                                           "tool.exe", {}, {}}});
  input.global_actions.emplace(
      "mouse-current",
      actions::ActionDefinition{1, actions::ActionType::mouse,
                                actions::MouseParameters{
                                    actions::MouseOperation::move, std::nullopt,
                                    {actions::PositionTarget::current_cursor, std::nullopt}}});
  input.global_actions.emplace(
      "mouse-end",
      actions::ActionDefinition{1, actions::ActionType::mouse,
                                actions::MouseParameters{
                                    actions::MouseOperation::button_up,
                                    actions::MouseButton::right,
                                    {actions::PositionTarget::gesture_end, std::nullopt}}});
  input.global_actions.emplace(
      "window-minimal",
      actions::ActionDefinition{1, actions::ActionType::window,
                                actions::WindowParameters{actions::WindowOperation::close,
                                                          actions::WindowTarget::foreground_window}});
  input.global_actions.emplace(
      "window-move",
      actions::ActionDefinition{1, actions::ActionType::window,
                                actions::WindowParameters{actions::WindowOperation::move,
                                                          actions::WindowTarget::gesture_window,
                                                          -400, 25}});
  input.global_actions.emplace(
      "window-resize",
      actions::ActionDefinition{1, actions::ActionType::window,
                                actions::WindowParameters{actions::WindowOperation::resize,
                                                          actions::WindowTarget::gesture_window,
                                                          std::nullopt, std::nullopt, 640, 480}});
  input.global_actions.emplace(
      "volume-default",
      actions::ActionDefinition{1, actions::ActionType::volume,
                                actions::VolumeParameters{actions::VolumeOperation::increase,
                                                          std::nullopt}});
  input.profiles.push_back(
      {"chrome",
       "Chrome",
       true,
       {{context::ApplicationProperty::process_name, context::MatchMode::exact, "chrome.exe"},
        {context::ApplicationProperty::window_title, context::MatchMode::contains, "GitHub"}},
       {{"left", actions::ActionDefinition::keyboard("CTRL+SHIFT+TAB")}}});
  auto result = round_trip(input, decode_profiles);
  check(static_cast<bool>(result), "profile file round trips");
  check(result.value->profiles.size() == 1 && result.value->profiles[0].criteria.size() == 2,
        "profile criteria are retained");
  check(*actions::keyboard_shortcut(result.value->global_actions.at("left")) == "ALT+LEFT" &&
            *actions::keyboard_shortcut(
                result.value->profiles[0].actions_by_gesture.at("left")) == "CTRL+SHIFT+TAB",
        "global and profile actions are retained");
  check(*actions::keyboard_shortcut(result.value->global_actions.at("minimize")) == "ALT+SPACE,N",
        "the minimize shortcut round trips as an ordinary editable shortcut");
  for (const auto* id : {"process", "url", "mouse", "window", "media", "volume", "desktop",
                         "process-minimal", "mouse-current", "mouse-end", "window-minimal",
                         "window-move", "window-resize", "volume-default"}) {
    check(result.value->global_actions.at(id) == input.global_actions.at(id),
          "a generic action and all of its parameters round trip");
  }

  const auto legacy_json = json::parse(
      R"({"version":1,"profiles":[],"global_actions":{"left":{"type":"keyboard","shortcut":"CTRL+W"}}})");
  const auto legacy = legacy_json ? decode_profiles(*legacy_json.value)
                                  : DecodeResult<ProfileFile>{{}, "invalid test JSON"};
  check(legacy &&
            *actions::keyboard_shortcut(legacy.value->global_actions.at("left")) == "CTRL+W",
        "Phase 1 keyboard actions without an action version remain compatible");
  const auto mixed_actions_json = json::parse(
      R"({"version":1,"profiles":[],"global_actions":{"good":{"type":"url","version":1,"url":"https://example.com"},"bad":{"type":"future_action","version":1}}})");
  const auto mixed_actions = mixed_actions_json
                                 ? decode_profiles(*mixed_actions_json.value)
                                 : DecodeResult<ProfileFile>{{}, "invalid test JSON"};
  check(mixed_actions && mixed_actions.value->global_actions.contains("good") &&
            !mixed_actions.value->global_actions.contains("bad"),
        "a corrupt generic action does not discard its valid sibling");
  check(mixed_actions.warning.find("global:bad") != std::string::npos,
        "invalid action diagnostics preserve the rejected mapping identity");
  auto mixed = encode(input);
  mixed.get_if<json::Object>()->at("profiles").get_if<json::Array>()->push_back(false);
  result = decode_profiles(mixed);
  check(result && result.value->profiles.size() == 1 && !result.warning.empty(),
        "valid profiles survive a malformed sibling entry");
  auto malformed_actions = encode(input);
  malformed_actions.get_if<json::Object>()->at("profiles").get_if<json::Array>()->front()
      .get_if<json::Object>()->insert_or_assign("actions", json::Array{});
  result = decode_profiles(malformed_actions);
  check(result && result.value->profiles.size() == 1 &&
            result.value->profiles.front().actions_by_gesture.empty(),
        "malformed profile actions are discarded without dropping the profile");
  auto malformed_fields = encode(input);
  auto& profile = *malformed_fields.get_if<json::Object>()->at("profiles").get_if<json::Array>()->front()
                       .get_if<json::Object>();
  profile.insert_or_assign("enabled", "true");
  profile.insert_or_assign("criteria", false);
  result = decode_profiles(malformed_fields);
  check(result && result.value->profiles.size() == 1 && !result.value->profiles.front().enabled &&
            result.value->profiles.front().criteria.empty() && !result.warning.empty(),
        "malformed profile fields retain the disabled parent record");

}
}  // namespace

void run_configuration_tests() {
  check(config::GlobalOptions{}.gestures_enabled,
        "a fresh application starts with gestures enabled");
  global_tests();
  gesture_tests();
  profile_tests();
}
}  // namespace strokes::tests
