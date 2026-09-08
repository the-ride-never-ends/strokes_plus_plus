#include "input/windows_mouse_click.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::input {

bool WindowsMouseClick::click(ActivationButton button) {
    auto inputs=make_input_sequence(button);
    return ::SendInput(static_cast<UINT>(inputs.size()),inputs.data(),static_cast<int>(sizeof(INPUT)))==inputs.size();
}

std::array<INPUT,2> WindowsMouseClick::make_input_sequence(ActivationButton button) noexcept {
    DWORD down_flag = 0;
    DWORD up_flag = 0;
    DWORD mouse_data = 0;
    switch (button) {
    case ActivationButton::right:
        down_flag = MOUSEEVENTF_RIGHTDOWN;
        up_flag = MOUSEEVENTF_RIGHTUP;
        break;
    case ActivationButton::middle:
        down_flag = MOUSEEVENTF_MIDDLEDOWN;
        up_flag = MOUSEEVENTF_MIDDLEUP;
        break;
    case ActivationButton::x_button_1:
        down_flag = MOUSEEVENTF_XDOWN;
        up_flag = MOUSEEVENTF_XUP;
        mouse_data = XBUTTON1;
        break;
    case ActivationButton::x_button_2:
        down_flag = MOUSEEVENTF_XDOWN;
        up_flag = MOUSEEVENTF_XUP;
        mouse_data = XBUTTON2;
        break;
    }

    std::array<INPUT,2> inputs{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = down_flag;
    inputs[0].mi.mouseData = mouse_data;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = up_flag;
    inputs[1].mi.mouseData = mouse_data;
    return inputs;
}

}  // namespace strokes::input
