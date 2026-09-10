#include "input/windows_mouse_click.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cmath>

namespace strokes::input {

bool WindowsMouseClick::click(ActivationButton button, gestures::Point position) {
  POINT current{};
  if (!::GetCursorPos(&current)) return false;
  const RECT virtual_screen{
      ::GetSystemMetrics(SM_XVIRTUALSCREEN), ::GetSystemMetrics(SM_YVIRTUALSCREEN),
      ::GetSystemMetrics(SM_XVIRTUALSCREEN) + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
      ::GetSystemMetrics(SM_YVIRTUALSCREEN) + ::GetSystemMetrics(SM_CYVIRTUALSCREEN)};
  auto inputs =
      make_inputs(button, position,
                  {static_cast<double>(current.x), static_cast<double>(current.y)}, virtual_screen);
  const UINT sent = sender_(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
  if (sent == inputs.size()) return true;
  INPUT cleanup[2]{};
  UINT count = 0;
  if (sent >= 2 && sent < 3) cleanup[count++] = inputs[2];
  if (sent < 4) cleanup[count++] = inputs[3];
  if (count != 0) (void)sender_(count, cleanup, sizeof(INPUT));
  return false;
}

std::array<INPUT, 4> WindowsMouseClick::make_inputs(ActivationButton button,
                                                    gestures::Point position,
                                                    gestures::Point restore_position,
                                                    RECT virtual_screen) noexcept {
  DWORD down_flag = 0;
  DWORD up_flag = 0;
  DWORD mouse_data = 0;
  switch (button) {
    case ActivationButton::left:
      return {};
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

  const auto absolute = [&](gestures::Point point) {
    POINT result{};
    const auto width = static_cast<double>(virtual_screen.right - virtual_screen.left - 1);
    const auto height = static_cast<double>(virtual_screen.bottom - virtual_screen.top - 1);
    result.x =
        width > 0.0 ? static_cast<LONG>(std::lround((point.x - virtual_screen.left) * 65535.0 / width)) : 0;
    result.y =
        height > 0.0 ? static_cast<LONG>(std::lround((point.y - virtual_screen.top) * 65535.0 / height)) : 0;
    return result;
  };
  const auto click = absolute(position);
  const auto restore = absolute(restore_position);

  std::array<INPUT, 4> inputs{};
  inputs[0].type = INPUT_MOUSE;
  inputs[0].mi.dx = click.x;
  inputs[0].mi.dy = click.y;
  inputs[0].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  inputs[1].type = INPUT_MOUSE;
  inputs[1].mi.dwFlags = down_flag;
  inputs[1].mi.mouseData = mouse_data;
  inputs[2].type = INPUT_MOUSE;
  inputs[2].mi.dwFlags = up_flag;
  inputs[2].mi.mouseData = mouse_data;
  inputs[3].type = INPUT_MOUSE;
  inputs[3].mi.dx = restore.x;
  inputs[3].mi.dy = restore.y;
  inputs[3].mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  return inputs;
}

}  // namespace strokes::input
