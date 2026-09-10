#pragma once
#include "gestures/stroke.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <optional>

namespace strokes::ui {
/// Captures and previews one training stroke in a DPI-aware native window.
class WindowsGestureTrainer final {
 public:
  [[nodiscard]] std::optional<gestures::Stroke> capture(HINSTANCE instance,
                                                        double minimum_point_distance,
                                                        HWND owner = nullptr);

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  void paint() noexcept;
  HWND window_{};
  HINSTANCE instance_{};
  gestures::Stroke points_;
  bool drawing_{};
  bool finished_{};
  bool accepted_{};
  double minimum_point_distance_{2.0};
};
}  // namespace strokes::ui
