#include "actions/symbolic_resolver.h"

namespace strokes::actions {

std::optional<gestures::Point> resolve_position(const PositionDefinition& position,
                                                const ActionContext& context) {
  switch (position.target) {
    case PositionTarget::current_cursor:
      return context.current_cursor_position;
    case PositionTarget::gesture_start:
      return context.gesture.start_position;
    case PositionTarget::gesture_end:
      return context.gesture.current_position;
    case PositionTarget::absolute:
      return position.absolute;
  }
  return std::nullopt;
}

std::optional<std::uintptr_t> resolve_window(WindowTarget target, const ActionContext& context) {
  switch (target) {
    case WindowTarget::gesture_window:
      return context.gesture_window;
    case WindowTarget::foreground_window:
      return context.foreground_window;
    case WindowTarget::window_at_gesture_start:
      return context.window_at_gesture_start;
  }
  return std::nullopt;
}

}  // namespace strokes::actions
