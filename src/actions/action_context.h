#pragma once

#include <cstdint>
#include <optional>

#include "context/application_context.h"
#include "gestures/point.h"
#include "gestures/recognizer.h"
#include "input/gesture_session.h"

namespace strokes::actions {

/// Immutable runtime values captured for, or resolved immediately before, an action.
struct ActionContext {
  /// False when no gesture produced this context, such as a script tested from the
  /// settings editor. Scripts then see no gesture and no application values at all.
  bool captured{true};
  input::GestureSession gesture;
  context::ApplicationContext application;
  std::optional<gestures::RecognitionResult> recognition;
  std::optional<gestures::Point> current_cursor_position;
  std::optional<std::uintptr_t> gesture_window;
  std::optional<std::uintptr_t> foreground_window;
  std::optional<std::uintptr_t> window_at_gesture_start;
  std::optional<std::uint32_t> target_process_id;
};

}  // namespace strokes::actions
