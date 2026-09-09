#pragma once

#include <functional>

#include "input/input_event.h"

namespace strokes::input {

struct MouseRouteResult {
  bool suppress_input{};
  bool event_delivered{};
};

/// Makes synchronous pass-through and suppression decisions for low-level mouse events.
class MouseInputRouter {
 public:
  struct Options {
    ActivationButton activation_button{ActivationButton::right};
    double movement_threshold{8.0};
    std::function<double()> threshold_scale_provider;
  };

  using EventSink = std::function<bool(const MouseInputEvent&)>;

  explicit MouseInputRouter(EventSink sink);
  MouseInputRouter(Options options, EventSink sink);

  [[nodiscard]] MouseRouteResult route(const MouseInputEvent& event);
  void configure(Options options);
  void set_enabled(bool enabled) noexcept;
  void set_button(ActivationButton button) noexcept;
  [[nodiscard]] bool cancel_interaction() noexcept;
  [[nodiscard]] bool enabled() const noexcept { return enabled_; }
  [[nodiscard]] bool interaction_active() const noexcept { return interaction_active_; }

 private:
  [[nodiscard]] bool deliver(const MouseInputEvent& event);
  void clear_interaction() noexcept;

  Options options_;
  EventSink sink_;
  bool enabled_{true};
  bool interaction_active_{};
  bool cancelled_release_pending_{};
  ActivationButton cancelled_button_{ActivationButton::right};
  gestures::Point start_position_{};
  double active_movement_threshold_{8.0};
};

}  // namespace strokes::input
