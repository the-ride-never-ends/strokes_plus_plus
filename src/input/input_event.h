#pragma once

#include "gestures/point.h"
#include "input/gesture_session.h"

#include <chrono>

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
};

}  // namespace strokes::input
