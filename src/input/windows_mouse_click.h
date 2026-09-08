#pragma once

#include "input/mouse_click.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <array>

namespace strokes::input {

class WindowsMouseClick final : public IMouseClick {
public:
    [[nodiscard]] bool click(ActivationButton button) override;
    [[nodiscard]] static std::array<INPUT,2> make_input_sequence(ActivationButton button) noexcept;
};

}  // namespace strokes::input
