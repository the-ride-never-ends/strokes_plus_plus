#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "actions/action_resolver.h"
#include "context/application_profile.h"
#include "gestures/recognizer.h"
#include "input/gesture_session.h"

namespace strokes::config {

struct OverlayOptions {
  bool enabled{true};
  int line_width{4};
  double opacity{0.85};
  std::uint32_t color{0x00A0FF};
};

struct GlobalOptions {
  static constexpr int current_version = 1;
  static constexpr double maximum_movement_threshold = 1000.0;
  static constexpr double maximum_point_distance = 1000.0;
  static constexpr std::size_t maximum_point_limit = 1'000'000;
  static constexpr int maximum_overlay_line_width = 100;
  // Overlay alpha is an 8-bit channel; anything below one step rounds to a
  // fully transparent window, which is the state the color guard also rejects.
  static constexpr double minimum_overlay_opacity = 1.0 / 255.0;
  int version{current_version};
  bool gestures_enabled{true};
  input::ActivationButton gesture_button{input::ActivationButton::right};
  double movement_threshold{8.0};
  double minimum_point_distance{2.0};
  std::size_t maximum_points{4096};
  double recognition_threshold{0.75}; // NOTE: Set by human developer. Don't touch
  OverlayOptions overlay;
};

struct GestureFile {
  static constexpr int current_version = 3;
  int version{current_version};
  std::vector<gestures::GestureDefinition> gestures;
};

struct ProfileFile {
  static constexpr int current_version = 2;
  int version{current_version};
  std::vector<context::ApplicationProfile> profiles;
  actions::ActionResolver::GlobalActions global_actions;
};

}  // namespace strokes::config
