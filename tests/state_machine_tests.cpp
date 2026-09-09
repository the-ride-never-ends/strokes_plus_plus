#include "input/gesture_state_machine.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using input::GestureState;
using input::GestureStateMachine;

void click_without_movement() {
  GestureStateMachine machine;
  check(machine.button_down({10, 10}).suppress_input, "activation down is held while pending");
  check(machine.state() == GestureState::button_pending, "button down enters pending state");
  const auto update = machine.button_up({12, 12});
  check(update.emulate_click, "release below threshold requests a normal click");
  check(machine.state() == GestureState::idle, "normal click returns to idle");
}

void capture_and_complete() {
  GestureStateMachine machine({2.0, 4});
  (void)machine.button_down({0, 0});
  const auto started = machine.pointer_moved({5, 0});
  check(started.capture_started, "movement at threshold begins capture");
  check(machine.state() == GestureState::capturing, "capture state is entered");
  (void)machine.pointer_moved({6, 0});
  (void)machine.pointer_moved({8, 0});
  (void)machine.pointer_moved({20, 0});
  (void)machine.pointer_moved({30, 0});
  check(machine.captured_points().size() == 4, "point filtering and maximum are enforced");

  const auto released = machine.button_up({40, 0});
  check(released.recognition_requested, "captured release requests recognition");
  check(machine.state() == GestureState::recognizing, "release enters recognizing state");
  (void)machine.recognition_finished(true);
  check(machine.state() == GestureState::executing, "resolved action enters executing state");
  (void)machine.execution_finished();
  check(machine.state() == GestureState::idle, "execution completion returns to idle");
}

void cancellation() {
  GestureStateMachine machine;
  (void)machine.button_down({0, 0});
  (void)machine.pointer_moved({20, 0});
  const auto cancelled = machine.cancel();
  check(cancelled.capture_cancelled, "active capture can be cancelled");
  check(machine.captured_points().empty(), "cancellation discards captured points");
  check(machine.state() == GestureState::cancelled, "cancellation enters cancelled state");
  (void)machine.cancellation_finished();
  check(machine.state() == GestureState::idle, "cancelled interaction returns to idle");
}

void no_match_and_invalid_transitions() {
  GestureStateMachine machine;
  check(!machine.button_up({0, 0}).recognition_requested, "unexpected release is ignored");
  (void)machine.button_down({0, 0});
  (void)machine.pointer_moved({20, 0});
  (void)machine.button_up({20, 0});
  (void)machine.recognition_finished(false);
  check(machine.state() == GestureState::idle, "unrecognized gesture returns to idle");
}

void session_context_is_preserved() {
  GestureStateMachine machine;
  const auto timestamp = input::GestureClock::now();
  input::GestureStart start;
  start.activation_button = input::ActivationButton::x_button_1;
  start.position = {-120, 450};
  start.timestamp = timestamp;
  start.modifiers = {.control = true, .shift = false, .alt = true, .windows = false};
  start.application = {42, 99, "chrome.exe", "Original title", "ChromeClass"};

  (void)machine.button_down(start);
  check(machine.session().has_value(), "button down creates a gesture session");
  check(machine.session()->activation_button == input::ActivationButton::x_button_1,
        "session records activation button");
  check(machine.session()->start_position == gestures::Point{-120, 450},
        "session records negative-capable start coordinates");
  check(machine.session()->start_timestamp == timestamp, "session records start time");
  check(machine.session()->modifiers.control && machine.session()->modifiers.alt,
        "session records modifier state");
  check(machine.session()->application.executable_name == "chrome.exe",
        "session records original application context");

  (void)machine.pointer_moved({-100, 460});
  (void)machine.button_up({-90, 470});
  check(machine.session()->application.window_title == "Original title",
        "application snapshot survives through recognition");
  check(machine.session()->current_position == gestures::Point{-90, 470},
        "session tracks final pointer position");
  (void)machine.recognition_finished(true);
  check(machine.session()->current_state == GestureState::executing,
        "session follows state through action execution");
  (void)machine.execution_finished();
  check(!machine.session(), "completed interaction releases its session");
}

void rapid_repeated_gestures() {
  GestureStateMachine machine;
  for (int attempt = 0; attempt < 100; ++attempt) {
    (void)machine.button_down({0, 0});
    (void)machine.pointer_moved({20, 0});
    check(machine.button_up({40, 0}).recognition_requested,
          "rapid repeated gesture reaches recognition");
    (void)machine.recognition_finished(false);
    check(machine.state() == GestureState::idle, "rapid repeated gesture returns to idle");
  }
}

}  // namespace

void run_state_machine_tests() {
  click_without_movement();
  capture_and_complete();
  cancellation();
  no_match_and_invalid_transitions();
  session_context_is_preserved();
  rapid_repeated_gestures();
}

}  // namespace strokes::tests
