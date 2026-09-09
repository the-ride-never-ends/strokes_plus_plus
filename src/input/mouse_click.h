#pragma once

#include "input/gesture_session.h"

namespace strokes::input {

/// Replays a suppressed activation-button click at its original position.
class IMouseClick {
 public:
  virtual ~IMouseClick() = default;
  [[nodiscard]] virtual bool click(ActivationButton button, gestures::Point position) = 0;
};

}  // namespace strokes::input
