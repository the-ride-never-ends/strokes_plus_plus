#include "input/gesture_state_machine.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace strokes::input {
namespace {

double distance(const gestures::Point& lhs, const gestures::Point& rhs) {
  return std::hypot(rhs.x - lhs.x, rhs.y - lhs.y);
}

bool valid(const GestureStateMachine::Options& options) {
  return std::isfinite(options.minimum_point_distance) && options.minimum_point_distance >= 0.0 &&
         options.maximum_points >= 2;
}

}  // namespace

GestureStateMachine::GestureStateMachine() : GestureStateMachine(Options{}) {}

GestureStateMachine::GestureStateMachine(Options options) : options_(options) {
  if (!valid(options)) {
    throw std::invalid_argument("invalid gesture state-machine options");
  }
}

GestureUpdate GestureStateMachine::button_down(gestures::Point position) {
  GestureStart start;
  start.position = position;
  return button_down(std::move(start));
}

GestureUpdate GestureStateMachine::button_down(GestureStart start) {
  if (state_ != GestureState::idle) {
    return {};
  }
  GestureSession session;
  session.activation_button = start.activation_button;
  session.start_position = start.position;
  session.current_position = start.position;
  session.start_timestamp = start.timestamp;
  session.last_movement_timestamp = start.timestamp;
  session.application = std::move(start.application);
  session.modifiers = start.modifiers;
  session_ = std::move(session);
  transition_to(GestureState::button_pending);
  return {.suppress_input = true};
}

GestureUpdate GestureStateMachine::pointer_moved(gestures::Point position) {
  if (session_) {
    session_->current_position = position;
    session_->last_movement_timestamp = GestureClock::now();
  }
  if (state_ == GestureState::button_pending) {
    transition_to(GestureState::capturing);
    session_->captured_points.reserve(options_.maximum_points);
    add_point(session_->start_position);
    add_point(position);
    return {.suppress_input = true, .capture_started = true};
  }
  if (state_ == GestureState::capturing) {
    add_point(position);
    return {.suppress_input = true};
  }
  return {};
}

GestureUpdate GestureStateMachine::button_up(gestures::Point position) {
  if (session_) {
    session_->current_position = position;
  }
  if (state_ == GestureState::button_pending) {
    reset();
    return {.suppress_input = true, .emulate_click = true};
  }
  if (state_ == GestureState::capturing) {
    add_point(position);
    transition_to(GestureState::recognizing);
    return {.suppress_input = true, .recognition_requested = true};
  }
  return {};
}

GestureUpdate GestureStateMachine::cancel() {
  if (state_ != GestureState::button_pending && state_ != GestureState::capturing) {
    return {};
  }
  transition_to(GestureState::cancelled);
  session_->captured_points.clear();
  return {.suppress_input = true, .capture_cancelled = true};
}

GestureUpdate GestureStateMachine::cancellation_finished() {
  if (state_ == GestureState::cancelled) {
    reset();
  }
  return {};
}

GestureUpdate GestureStateMachine::recognition_finished(bool has_action) {
  if (state_ != GestureState::recognizing) {
    return {};
  }
  if (has_action) {
    transition_to(GestureState::executing);
  } else {
    reset();
  }
  return {};
}

GestureUpdate GestureStateMachine::execution_finished() {
  if (state_ == GestureState::executing) {
    reset();
  }
  return {};
}

void GestureStateMachine::reset() noexcept {
  state_ = GestureState::idle;
  session_.reset();
}

void GestureStateMachine::transition_to(GestureState state) noexcept {
  state_ = state;
  if (session_) {
    session_->current_state = state;
  }
}

void GestureStateMachine::add_point(gestures::Point position) {
  auto& points = session_->captured_points;
  if (points.size() >= options_.maximum_points) {
    return;
  }
  if (points.empty() || distance(points.back(), position) >= options_.minimum_point_distance) {
    points.push_back(position);
  }
}

const gestures::Stroke& GestureStateMachine::captured_points() const noexcept {
  return session_ ? session_->captured_points : empty_stroke_;
}

}  // namespace strokes::input
