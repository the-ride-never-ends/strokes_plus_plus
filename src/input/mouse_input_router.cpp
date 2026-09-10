#include "input/mouse_input_router.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace strokes::input {

MouseInputRouter::MouseInputRouter(EventSink sink) : MouseInputRouter(Options{}, std::move(sink)) {}

MouseInputRouter::MouseInputRouter(Options options, EventSink sink) : sink_(std::move(sink)) {
  if (!sink_) {
    throw std::invalid_argument("mouse input router requires an event sink");
  }
  configure(std::move(options));
}

void MouseInputRouter::configure(Options options) {
  if (!std::isfinite(options.movement_threshold) || options.movement_threshold < 0.0) {
    throw std::invalid_argument("mouse movement threshold must be non-negative");
  }
  cancelled_release_pending_ = interaction_active_;
  cancelled_button_ = options_.activation_button;
  options_ = std::move(options);
  clear_interaction();
}

MouseRouteResult MouseInputRouter::route(const MouseInputEvent& event) {
  if (!enabled_) {
    return {};
  }

  if (event.type == MouseEventType::button_down && !interaction_active_) {
    if (event.button != options_.activation_button) {
      return {};
    }
    if (!deliver(event)) {
      return {};
    }
    interaction_active_ = true;
    cancelled_release_pending_ = false;
    start_position_ = event.position;
    const double scale =
        options_.threshold_scale_provider ? options_.threshold_scale_provider() : 1.0;
    active_movement_threshold_ =
        options_.movement_threshold * (std::isfinite(scale) && scale > 0.0 ? scale : 1.0);
    return {.suppress_input = true, .event_delivered = true};
  }

  if (!interaction_active_) {
    if (cancelled_release_pending_ && event.type == MouseEventType::button_up &&
        event.button == cancelled_button_) {
      cancelled_release_pending_ = false;
      return {.suppress_input = true};
    }
    return {};
  }

  if (event.type == MouseEventType::pointer_moved) {
    if (!capturing_) {
      const double movement =
          std::hypot(event.position.x - start_position_.x, event.position.y - start_position_.y);
      if (movement < active_movement_threshold_) return {};
      capturing_ = true;
    }
    const bool delivered = deliver(event);
    return {.suppress_input = false, .event_delivered = delivered};
  }

  if (event.type == MouseEventType::button_up && event.button == options_.activation_button) {
    const bool delivered = deliver(event);
    clear_interaction();
    // The corresponding button-down was suppressed. The release must also
    // remain suppressed even if a saturated queue cannot accept it.
    return {.suppress_input = true, .event_delivered = delivered};
  }

  if (event.type == MouseEventType::button_down) {
    MouseInputEvent cancellation = event;
    cancellation.type = MouseEventType::cancel;
    const bool delivered = deliver(cancellation);
    cancelled_release_pending_ = true;
    cancelled_button_ = options_.activation_button;
    clear_interaction();
    return {.suppress_input = false, .event_delivered = delivered};
  }

  return {};
}

void MouseInputRouter::set_enabled(bool enabled) noexcept {
  enabled_ = enabled;
  if (!enabled_) {
    cancelled_release_pending_ = interaction_active_;
    cancelled_button_ = options_.activation_button;
    clear_interaction();
  }
}

void MouseInputRouter::set_button(ActivationButton button) noexcept {
  cancelled_release_pending_ = interaction_active_;
  cancelled_button_ = options_.activation_button;
  options_.activation_button = button;
  clear_interaction();
}

bool MouseInputRouter::cancel_interaction() noexcept {
  const bool was_active = interaction_active_;
  cancelled_release_pending_ = was_active;
  cancelled_button_ = options_.activation_button;
  clear_interaction();
  return was_active;
}

bool MouseInputRouter::deliver(const MouseInputEvent& event) { return sink_(event); }

void MouseInputRouter::clear_interaction() noexcept {
  interaction_active_ = false;
  capturing_ = false;
}

}  // namespace strokes::input
