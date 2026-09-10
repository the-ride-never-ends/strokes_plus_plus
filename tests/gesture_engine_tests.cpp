#include <span>
#include <vector>

#include "engine/gesture_engine.h"
#include "input/mouse_input_router.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using namespace actions;
using namespace context;
using namespace engine;
using namespace gestures;
using namespace input;

class FixedContext final : public IApplicationContextProvider {
 public:
  ApplicationContext value;
  bool available{true};
  mutable std::uintptr_t requested_window{};
  std::optional<ApplicationContext> foreground_application() const override {
    return available ? std::optional<ApplicationContext>{value} : std::nullopt;
  }
  std::optional<ApplicationContext> window_application(std::uintptr_t window) const override {
    requested_window = window;
    return available ? std::optional<ApplicationContext>{value} : std::nullopt;
  }
};

class FixedModifiers final : public IModifierStateProvider {
 public:
  ModifierState value;
  ModifierState current_modifiers() const override { return value; }
};

class FakeClick final : public IMouseClick {
 public:
  int calls{};
  ActivationButton button{};
  Point position{};
  bool click(ActivationButton value, Point click_position) override {
    ++calls;
    button = value;
    position = click_position;
    return true;
  }
};

class FakeKeyboard final : public IKeyboardInput {
 public:
  std::vector<KeyEvent> events;
  bool result{true};
  bool is_key_down(VirtualKey) const override { return false; }
  bool send(std::span<const KeyEvent> value) override {
    events.assign(value.begin(), value.end());
    return result;
  }
};

class FakeFeedback final : public IGestureFeedback {
 public:
  int shows{}, updates{}, hides{};
  bool visible{};
  Stroke last_points;
  void show(const Stroke& points) override {
    ++shows;
    visible = true;
    last_points = points;
  }
  void update(const Stroke& points) override {
    ++updates;
    last_points = points;
  }
  void hide() noexcept override {
    ++hides;
    visible = false;
    last_points.clear();
  }
};

Stroke right_line() { return {{0, 0}, {10, 0}, {20, 0}, {30, 0}}; }

MouseInputEvent mouse(MouseEventType type, double x, double y, std::uintptr_t target = 0) {
  return {type, {x, y}, ActivationButton::right, {}, target};
}

struct Fixture {
  Recognizer recognizer{0.90};
  std::vector<ApplicationProfile> profiles;
  ActionResolver::GlobalActions globals;
  FixedContext context;
  FixedModifiers modifiers;
  FakeClick click;
  FakeKeyboard keyboard;

  Fixture() {
    (void)recognizer.add_gesture({"right", "Right", true, {{"sample", right_line()}}});
    context.value = {10, 20, "chrome.exe", "Chrome", "ChromeClass"};
  }

  GestureEngine engine() {
    return {recognizer, profiles, globals, context, modifiers, click, keyboard};
  }
};

void ordinary_click_pipeline() {
  Fixture fixture;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 1, 1));
  check(result.click_replayed && fixture.click.calls == 1,
        "below-threshold interaction replays an ordinary click");
  check(fixture.click.position == Point{0, 0}, "ordinary click is replayed at its press position");
  check(engine.state() == GestureState::idle, "click pipeline finishes idle");
}

void global_action_pipeline() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+LEFT"});
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 10, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 30, 0));
  check(result.recognition && result.recognition->gesture_id == "right",
        "completed stroke is recognized by the engine pipeline");
  check(result.action_source == ActionSource::global, "global action is selected");
  check(result.action_attempted && result.action_succeeded, "resolved keyboard action executes");
  check(fixture.keyboard.events.size() == 4, "keyboard action emits its complete key sequence");
  check(engine.state() == GestureState::idle, "action pipeline finishes idle");
}

void profile_override_and_context_snapshot() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+LEFT"});
  fixture.profiles.push_back({"chrome",
                              "Chrome",
                              true,
                              {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"}},
                              {{"right", {ActionType::keyboard_shortcut, "CTRL+W"}}}});
  fixture.modifiers.value.control = true;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0, 0x1234));
  check(fixture.context.requested_window == 0x1234,
        "gesture activation resolves context for the captured target window");
  fixture.context.value.executable_name = "notepad.exe";
  (void)engine.process(mouse(MouseEventType::pointer_moved, 10, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 30, 0));
  check(result.action_source == ActionSource::application_profile,
        "profile action uses application captured at gesture start");
  check(result.profile_id == "chrome", "engine reports the matched profile identity");
}

void no_match_executes_nothing() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+LEFT"});
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 0, 10));
  const auto result = engine.process(mouse(MouseEventType::button_up, 0, 30));
  check(!result.recognition && !result.action_attempted && fixture.keyboard.events.empty(),
        "unknown stroke executes no action");
  check(engine.state() == GestureState::idle, "no-match pipeline returns idle");
  check(feedback.hides == 1 && !feedback.visible, "recognition rejection hides gesture feedback");
}

void disabled_feedback_does_not_disable_recognition() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+RIGHT"});
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, nullptr};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.recognition && result.action_succeeded,
        "recognition and actions work when gesture feedback is disabled at the engine boundary");
}

void action_failure_does_not_stop_engine() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+RIGHT"});
  fixture.keyboard.result = false;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto failed = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(failed.action_attempted && !failed.action_succeeded,
        "action injection failure is reported");
  check(engine.state() == GestureState::idle, "action failure leaves engine operational");
  fixture.keyboard.result = true;
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  check(engine.process(mouse(MouseEventType::button_up, 40, 0)).action_succeeded,
        "gesture after an action failure still executes");
}

void cancellation_executes_nothing() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+RIGHT"});
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto cancelled = engine.process(mouse(MouseEventType::cancel, 20, 0));
  check(cancelled.gesture.capture_cancelled && !cancelled.action_attempted &&
            fixture.keyboard.events.empty(),
        "cancellation executes no action");
  check(feedback.hides == 1 && !feedback.visible && feedback.last_points.empty(),
        "cancellation hides and clears gesture feedback");
  check(engine.state() == GestureState::idle, "cancelled engine pipeline returns idle");
}

void closed_target_uses_safe_snapshot() {
  Fixture fixture;
  fixture.profiles.push_back({"chrome",
                              "Chrome",
                              true,
                              {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"}},
                              {{"right", {ActionType::keyboard_shortcut, "CTRL+W"}}}});
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  fixture.context.available = false;
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.action_succeeded && result.profile_id == "chrome",
        "closing the original target does not invalidate captured context or action data");
}

void cross_monitor_coordinates_pipeline() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+RIGHT"});
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, -300, 40));
  (void)engine.process(mouse(MouseEventType::pointer_moved, -100, 40));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 100, 40));
  const auto result = engine.process(mouse(MouseEventType::button_up, 300, 40));
  check(result.recognition && result.action_succeeded,
        "stroke crossing negative and positive screen coordinates is recognized and executed");
}

void router_engine_feedback_integration() {
  Fixture fixture;
  fixture.globals.emplace("right", Action{ActionType::keyboard_shortcut, "ALT+RIGHT"});
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  MouseInputRouter router([&](const MouseInputEvent& value) {
    (void)engine.process(value);
    return true;
  });
  check(router.route(mouse(MouseEventType::button_down, -20, 0)).suppress_input,
        "abstract hook router suppresses configured activation input");
  (void)router.route(mouse(MouseEventType::pointer_moved, 0, 0));
  (void)router.route(mouse(MouseEventType::pointer_moved, 20, 0));
  (void)router.route(mouse(MouseEventType::button_up, 40, 0));
  check(feedback.shows == 1 && feedback.updates >= 1 && feedback.hides == 1,
        "router-to-engine pipeline owns the complete overlay lifecycle");
  check(fixture.keyboard.events.size() == 4 && engine.state() == GestureState::idle,
        "router-to-engine pipeline captures context, recognizes, and executes its action");
  for (int attempt = 0; attempt < 50; ++attempt) {
    (void)router.route(mouse(MouseEventType::button_down, -20, 0));
    (void)router.route(mouse(MouseEventType::pointer_moved, 0, 0));
    (void)router.route(mouse(MouseEventType::button_up, 40, 0));
  }
  check(!feedback.visible && feedback.last_points.empty() && feedback.shows == feedback.hides,
        "rapid gestures leave no visible or retained overlay state");

  router.set_enabled(false);
  check(!router.route(mouse(MouseEventType::button_down, 0, 0)).suppress_input &&
            !router.route(mouse(MouseEventType::pointer_moved, 20, 0)).suppress_input &&
            !router.route(mouse(MouseEventType::button_up, 40, 0)).suppress_input,
        "disabled router passes the complete mouse interaction through");
  check(!feedback.visible, "disabled end-to-end path produces no overlay");
  router.set_enabled(true);
  (void)router.route(mouse(MouseEventType::button_down, 0, 0));
  (void)router.route(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto resumed = router.route(mouse(MouseEventType::button_up, 40, 0));
  check(resumed.suppress_input && engine.state() == GestureState::idle,
        "re-enabled router resumes complete gesture processing");
}

}  // namespace

void run_gesture_engine_tests() {
  ordinary_click_pipeline();
  global_action_pipeline();
  profile_override_and_context_snapshot();
  no_match_executes_nothing();
  disabled_feedback_does_not_disable_recognition();
  action_failure_does_not_stop_engine();
  cancellation_executes_nothing();
  closed_target_uses_safe_snapshot();
  cross_monitor_coordinates_pipeline();
  router_engine_feedback_integration();
}

}  // namespace strokes::tests
