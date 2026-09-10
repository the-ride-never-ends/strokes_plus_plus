#include <vector>

#include "input/mouse_input_router.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using input::ActivationButton;
using input::MouseEventType;
using input::MouseInputEvent;
using input::MouseInputRouter;

MouseInputEvent event(MouseEventType type, double x, double y,
                      ActivationButton button = ActivationButton::right) {
  return {type, {x, y}, button, {}};
}

void click_routing() {
  std::vector<MouseInputEvent> delivered;
  MouseInputRouter router([&](const auto& value) {
    delivered.push_back(value);
    return true;
  });

  check(router.route(event(MouseEventType::button_down, 10, 10)).suppress_input,
        "configured activation down is suppressed");
  const auto small_move = router.route(event(MouseEventType::pointer_moved, 12, 12));
  check(!small_move.suppress_input && !small_move.event_delivered,
        "sub-threshold movement passes through without queue traffic");
  check(router.route(event(MouseEventType::button_up, 12, 12)).suppress_input,
        "activation release is suppressed for click emulation");
  check(delivered.size() == 2 && delivered.front().type == MouseEventType::button_down &&
            delivered.back().type == MouseEventType::button_up,
        "pending click delivers only its down and up events");
  check(!router.interaction_active(), "release completes routing interaction");
}

void gesture_and_unrelated_routing() {
  std::vector<MouseInputEvent> delivered;
  MouseInputRouter router({ActivationButton::middle, 5.0}, [&](const auto& value) {
    delivered.push_back(value);
    return true;
  });

  check(!router.route(event(MouseEventType::button_down, 0, 0, ActivationButton::right))
             .suppress_input,
        "unconfigured button passes through");
  (void)router.route(event(MouseEventType::button_down, -10, 0, ActivationButton::middle));
  const auto moved = router.route(event(MouseEventType::pointer_moved, -5, 0));
  check(!moved.suppress_input && moved.event_delivered,
        "movement at threshold is delivered without freezing the system cursor");
  check(!router.route(event(MouseEventType::button_up, -5, 0, ActivationButton::right))
             .suppress_input,
        "unrelated release passes through during interaction");
  (void)router.route(event(MouseEventType::button_up, -5, 0, ActivationButton::middle));
  check(delivered.size() == 3, "gesture routes down, qualifying move, and up");
}

void enabled_left_click_passes_through() {
  int deliveries = 0;
  MouseInputRouter router({ActivationButton::right, 8.0}, [&](const auto&) {
    ++deliveries;
    return true;
  });
  const auto down =
      router.route(event(MouseEventType::button_down, 10, 10, ActivationButton::left));
  const auto up = router.route(event(MouseEventType::button_up, 10, 10, ActivationButton::left));
  check(!down.suppress_input && !up.suppress_input && deliveries == 0 &&
            !router.interaction_active(),
        "enabled right-button gestures never suppress or consume a left click");
}

void disabled_and_saturated_behavior() {
  MouseInputRouter disabled([](const auto&) { return true; });
  disabled.set_enabled(false);
  check(!disabled.route(event(MouseEventType::button_down, 0, 0)).suppress_input,
        "disabled router passes activation through");

  MouseInputRouter saturated([](const auto&) { return false; });
  const auto down = saturated.route(event(MouseEventType::button_down, 0, 0));
  check(!down.suppress_input && !saturated.interaction_active(),
        "failed initial delivery leaves native input untouched");

  int deliveries = 0;
  MouseInputRouter release_saturated([&](const auto&) { return ++deliveries == 1; });
  (void)release_saturated.route(event(MouseEventType::button_down, 0, 0));
  const auto release = release_saturated.route(event(MouseEventType::button_up, 0, 0));
  check(release.suppress_input && !release.event_delivered,
        "release remains suppressed if the event sink is saturated");
  check(!release_saturated.interaction_active(),
        "failed release delivery does not wedge input routing");
}

void disable_during_capture() {
  std::vector<MouseInputEvent> delivered;
  MouseInputRouter router([&](const auto& value) {
    delivered.push_back(value);
    return true;
  });
  (void)router.route(event(MouseEventType::button_down, 0, 0));
  (void)router.route(event(MouseEventType::pointer_moved, 20, 0));
  router.set_enabled(false);
  check(!router.interaction_active() && delivered.back().type == MouseEventType::cancel,
        "disabling during capture delivers cancellation and clears router state");
  router.set_enabled(false);
  router.configure({ActivationButton::middle, 5.0});
  check(router.route(event(MouseEventType::button_up, 20, 0)).suppress_input,
        "reconfiguration preserves pending release suppression");
}

void explicit_cancellation() {
  MouseInputRouter router([](const auto&) { return true; });
  check(!router.cancel_interaction(), "idle router has nothing to cancel");
  (void)router.route(event(MouseEventType::button_down, 0, 0));
  check(router.cancel_interaction(), "active interaction can be cancelled synchronously");
  check(!router.interaction_active(), "explicit cancellation clears router state");
  check(router.route(event(MouseEventType::button_up, 0, 0)).suppress_input,
        "release after cancellation is suppressed to avoid an orphan button-up");
  check(!router.route(event(MouseEventType::button_up, 0, 0)).suppress_input,
        "only the matching cancelled release is suppressed");
}

void invalid_sequence_cancellation() {
  std::vector<MouseInputEvent> delivered;
  MouseInputRouter router([&](const auto& value) {
    delivered.push_back(value);
    return true;
  });
  (void)router.route(event(MouseEventType::button_down, 0, 0));
  const auto unrelated =
      router.route(event(MouseEventType::button_down, 1, 1, ActivationButton::left));
  check(!unrelated.suppress_input && unrelated.event_delivered && !router.interaction_active(),
        "another mouse-button press passes through and cancels the active gesture");
  check(delivered.back().type == MouseEventType::cancel,
        "invalid input sequence delivers cancellation to the engine");
  check(router.route(event(MouseEventType::button_up, 1, 1)).suppress_input,
        "activation release after invalid-sequence cancellation remains balanced");
}

void returning_to_origin() {
  std::vector<MouseInputEvent> delivered;
  MouseInputRouter router([&](const auto& value) {
    delivered.push_back(value);
    return true;
  });
  (void)router.route(event(MouseEventType::button_down, 0, 0));
  (void)router.route(event(MouseEventType::pointer_moved, 10, 0));
  const auto returned = router.route(event(MouseEventType::pointer_moved, 0, 0));
  check(returned.event_delivered && delivered.back().position.x == 0,
        "all movement is delivered after capture starts, including a return to origin");
}

void dpi_scaled_thresholds() {
  for (const double scale : {1.0, 1.25, 1.5, 2.0}) {
    std::vector<MouseInputEvent> delivered;
    MouseInputRouter router({ActivationButton::right, 8.0, [scale] { return scale; }},
                            [&](const auto& value) {
                              delivered.push_back(value);
                              return true;
                            });
    (void)router.route(event(MouseEventType::button_down, 0, 0));
    const double threshold = 8.0 * scale;
    const auto below = router.route(event(MouseEventType::pointer_moved, threshold - .25, 0));
    const auto at = router.route(event(MouseEventType::pointer_moved, threshold, 0));
    check(!below.event_delivered && at.event_delivered,
          "movement threshold tracks the foreground DPI scale");
  }
}

}  // namespace

void run_mouse_input_router_tests() {
  click_routing();
  gesture_and_unrelated_routing();
  enabled_left_click_passes_through();
  disabled_and_saturated_behavior();
  disable_during_capture();
  explicit_cancellation();
  invalid_sequence_cancellation();
  returning_to_origin();
  dpi_scaled_thresholds();
}

}  // namespace strokes::tests
