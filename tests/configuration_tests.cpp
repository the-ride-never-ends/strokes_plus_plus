#include "config/configuration_codec.h"
#include "test_support.h"

namespace strokes::tests {
namespace {
using namespace config;

template<class T, class Decoder>
auto round_trip(const T& value, Decoder decoder) {
    const auto text = json::serialize(encode(value));
    const auto parsed = json::parse(text);
    check(static_cast<bool>(parsed), "encoded configuration is valid JSON");
    return decoder(*parsed.value);
}

void global_tests() {
    GlobalOptions input;
    input.gestures_enabled=false; input.gesture_button=input::ActivationButton::x_button_2;
    input.movement_threshold=12; input.minimum_point_distance=3; input.maximum_points=512;
    input.recognition_threshold=.92; input.overlay={false,7,.5,0x123456};
    auto result=round_trip(input,decode_global_options);
    check(static_cast<bool>(result), "global options round trip");
    check(result.value->gesture_button==input::ActivationButton::x_button_2 && result.value->maximum_points==512,
          "global input settings are retained");
    check(result.value->overlay.line_width==7 && result.value->overlay.color==0x123456,
          "overlay settings are retained");
    auto invalid=encode(input); *invalid.get_if<json::Object>()->at("recognition_threshold").get_if<double>()=2;
    check(!decode_global_options(invalid), "invalid recognition threshold is rejected");
}

void gesture_tests() {
    GestureFile input;
    input.gestures.push_back({"caret","Caret",true,{{"one",{{0,10},{10,0},{20,10}}},{"two",{{1,11},{11,1},{21,11}}}}});
    auto result=round_trip(input,decode_gesture_file);
    check(static_cast<bool>(result), "gesture file round trips");
    check(result.value->gestures.size()==1 && result.value->gestures[0].templates.size()==2,
          "gesture templates and identity are retained");
    GestureFile invalid; invalid.gestures.push_back({"bad","Bad",true,{}});
    auto recovered=decode_gesture_file(encode(invalid));
    check(recovered&&recovered.value->gestures.empty()&&!recovered.error.empty(),
          "invalid gesture entry is skipped with a warning");

    auto mixed=encode(input);auto* gestures=mixed.get_if<json::Object>()->at("gestures").get_if<json::Array>();
    gestures->push_back(json::Object{{"id","broken"}});recovered=decode_gesture_file(mixed);
    check(recovered&&recovered.value->gestures.size()==1&&!recovered.error.empty(),
          "valid gestures survive a malformed sibling entry");
}

void profile_tests() {
    ProfileFile input;
    input.global_actions.emplace("left",actions::Action{actions::ActionType::keyboard_shortcut,"ALT+LEFT"});
    input.profiles.push_back({"chrome","Chrome",true,
        {{context::ApplicationProperty::process_name,context::MatchMode::exact,"chrome.exe"},
         {context::ApplicationProperty::window_title,context::MatchMode::contains,"GitHub"}},
        {{"left",{actions::ActionType::keyboard_shortcut,"CTRL+SHIFT+TAB"}}}});
    auto result=round_trip(input,decode_profile_file);
    check(static_cast<bool>(result), "profile file round trips");
    check(result.value->profiles.size()==1 && result.value->profiles[0].criteria.size()==2,
          "profile criteria are retained");
    check(result.value->global_actions.at("left").value=="ALT+LEFT" &&
          result.value->profiles[0].actions_by_gesture.at("left").value=="CTRL+SHIFT+TAB",
          "global and profile actions are retained");
    auto mixed=encode(input);mixed.get_if<json::Object>()->at("profiles").get_if<json::Array>()->push_back(false);
    result=decode_profile_file(mixed);
    check(result&&result.value->profiles.size()==1&&!result.error.empty(),
          "valid profiles survive a malformed sibling entry");
}
}

void run_configuration_tests(){global_tests();gesture_tests();profile_tests();}
}  // namespace strokes::tests
