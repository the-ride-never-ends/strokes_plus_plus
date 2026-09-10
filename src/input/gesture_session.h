#pragma once

#include <chrono>

#include "context/application_context.h"
#include "gestures/stroke.h"

namespace strokes::input {

enum class ActivationButton {
  right,
  middle,
  x_button_1,
  x_button_2,
  left,
};

enum class GestureState {
  idle,
  button_pending,
  capturing,
  recognizing,
  executing,
  cancelled,
};

struct ModifierState {
  bool control{};
  bool shift{};
  bool alt{};
  bool windows{};
};

using GestureClock = std::chrono::steady_clock;

struct GestureStart {
  ActivationButton activation_button{ActivationButton::right};
  gestures::Point position;
  GestureClock::time_point timestamp{GestureClock::now()};
  ModifierState modifiers;
  context::ApplicationContext application;
};

struct GestureSession {
  ActivationButton activation_button{ActivationButton::right};
  gestures::Point start_position;
  gestures::Point current_position;
  gestures::Stroke captured_points;
  GestureClock::time_point start_timestamp;
  GestureClock::time_point last_movement_timestamp;
  context::ApplicationContext application;
  ModifierState modifiers;
  GestureState current_state{GestureState::button_pending};
};

}  // namespace strokes::input
