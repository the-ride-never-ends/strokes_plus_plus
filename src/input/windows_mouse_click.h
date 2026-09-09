#pragma once

#include "input/mouse_click.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>

namespace strokes::input {

/// Replays balanced Windows mouse input while preserving cursor position.
class WindowsMouseClick final : public IMouseClick {
 public:
  [[nodiscard]] bool click(ActivationButton button, gestures::Point position) override;
  [[nodiscard]] static std::array<INPUT, 4> make_inputs(ActivationButton button,
                                                        gestures::Point position,
                                                        gestures::Point restore_position,
                                                        RECT virtual_screen) noexcept;
};

}  // namespace strokes::input
