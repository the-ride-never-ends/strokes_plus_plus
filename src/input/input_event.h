#pragma once

#include <chrono>
#include <cstdint>

#include "gestures/point.h"
#include "input/gesture_session.h"

namespace strokes::input {

enum class MouseEventType {
  pointer_moved,
  button_down,
  button_up,
  cancel,
};

struct MouseInputEvent {
  MouseEventType type{MouseEventType::pointer_moved};
  gestures::Point position;
  ActivationButton button{ActivationButton::right};
  std::chrono::milliseconds timestamp{};
  std::uintptr_t target_window{};
};

}  // namespace strokes::input
