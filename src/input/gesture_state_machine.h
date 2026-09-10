#pragma once

#include <cstddef>
#include <optional>

#include "input/gesture_session.h"

namespace strokes::input {

struct GestureUpdate {
  bool suppress_input{};
  bool emulate_click{};
  bool capture_started{};
  bool recognition_requested{};
  bool capture_cancelled{};
};

/// Owns the lifecycle and bounded point collection for one gesture interaction.
class GestureStateMachine {
 public:
  struct Options {
    double minimum_point_distance{2.0};
    std::size_t maximum_points{4096};
  };

  GestureStateMachine();
  explicit GestureStateMachine(Options options);

  /// Begins a session from a point using default context.
  [[nodiscard]] GestureUpdate button_down(gestures::Point position);
  /// Begins a session from a complete activation snapshot.
  ///
  /// Args:
  ///   start: Immutable activation-time position, modifiers, and application context.
  /// Returns:
  ///   Flags describing the resulting state transition.
  [[nodiscard]] GestureUpdate button_down(GestureStart start);
  /// Records a qualifying point and enters capture when needed.
  [[nodiscard]] GestureUpdate pointer_moved(gestures::Point position);
  /// Completes a pending click or requests recognition of a captured stroke.
  [[nodiscard]] GestureUpdate button_up(gestures::Point position);
  /// Cancels the active interaction without executing an action.
  [[nodiscard]] GestureUpdate cancel();
  /// Completes cancellation and restores the idle state.
  [[nodiscard]] GestureUpdate cancellation_finished();
  /// Completes recognition and selects executing or idle state.
  [[nodiscard]] GestureUpdate recognition_finished(bool has_action);
  /// Completes action execution and restores the idle state.
  [[nodiscard]] GestureUpdate execution_finished();

  [[nodiscard]] GestureState state() const noexcept { return state_; }
  [[nodiscard]] const gestures::Stroke& captured_points() const noexcept;
  [[nodiscard]] const std::optional<GestureSession>& session() const noexcept { return session_; }

 private:
  void reset() noexcept;
  void transition_to(GestureState state) noexcept;
  void add_point(gestures::Point position);

  Options options_;
  GestureState state_{GestureState::idle};
  std::optional<GestureSession> session_;
  gestures::Stroke empty_stroke_;
};

}  // namespace strokes::input
