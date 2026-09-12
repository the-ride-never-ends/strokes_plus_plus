#include "actions/windows_mouse_service.h"

#include <cmath>

namespace strokes::actions {
namespace {

INPUT movement(gestures::Point point, RECT screen) {
  const double width = static_cast<double>(screen.right - screen.left - 1);
  const double height = static_cast<double>(screen.bottom - screen.top - 1);
  INPUT input{};
  input.type = INPUT_MOUSE;
  input.mi.dx = width > 0.0
                    ? static_cast<LONG>(std::lround((point.x - screen.left) * 65535.0 / width))
                    : 0;
  input.mi.dy = height > 0.0
                    ? static_cast<LONG>(std::lround((point.y - screen.top) * 65535.0 / height))
                    : 0;
  input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  return input;
}

std::pair<INPUT, INPUT> button_inputs(MouseButton button) {
  INPUT down{}, up{};
  down.type = up.type = INPUT_MOUSE;
  switch (button) {
    case MouseButton::left:
      down.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
      up.mi.dwFlags = MOUSEEVENTF_LEFTUP;
      break;
    case MouseButton::right:
      down.mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
      up.mi.dwFlags = MOUSEEVENTF_RIGHTUP;
      break;
    case MouseButton::middle:
      down.mi.dwFlags = MOUSEEVENTF_MIDDLEDOWN;
      up.mi.dwFlags = MOUSEEVENTF_MIDDLEUP;
      break;
    case MouseButton::x_button_1:
    case MouseButton::x_button_2:
      down.mi.dwFlags = MOUSEEVENTF_XDOWN;
      up.mi.dwFlags = MOUSEEVENTF_XUP;
      down.mi.mouseData = up.mi.mouseData =
          button == MouseButton::x_button_1 ? XBUTTON1 : XBUTTON2;
      break;
  }
  return {down, up};
}

}  // namespace

std::optional<gestures::Point> WindowsMouseService::current_position() const {
  POINT point{};
  if (!::GetCursorPos(&point)) return std::nullopt;
  return gestures::Point{static_cast<double>(point.x), static_cast<double>(point.y)};
}

std::vector<INPUT> WindowsMouseService::make_inputs(MouseOperation operation,
                                                    std::optional<MouseButton> button,
                                                    gestures::Point position,
                                                    RECT virtual_screen) {
  std::vector<INPUT> inputs{movement(position, virtual_screen)};
  if (operation == MouseOperation::move) return inputs;
  if (!button) return {};
  const auto [down, up] = button_inputs(*button);
  switch (operation) {
    case MouseOperation::click:
      inputs.push_back(down);
      inputs.push_back(up);
      break;
    case MouseOperation::double_click:
      inputs.push_back(down);
      inputs.push_back(up);
      inputs.push_back(down);
      inputs.push_back(up);
      break;
    case MouseOperation::button_down:
      inputs.push_back(down);
      break;
    case MouseOperation::button_up:
      inputs.push_back(up);
      break;
    case MouseOperation::move:
      break;
  }
  return inputs;
}

ActionResult WindowsMouseService::perform(MouseOperation operation,
                                          std::optional<MouseButton> button,
                                          gestures::Point position) {
  const RECT screen{::GetSystemMetrics(SM_XVIRTUALSCREEN), ::GetSystemMetrics(SM_YVIRTUALSCREEN),
                    ::GetSystemMetrics(SM_XVIRTUALSCREEN) + ::GetSystemMetrics(SM_CXVIRTUALSCREEN),
                    ::GetSystemMetrics(SM_YVIRTUALSCREEN) + ::GetSystemMetrics(SM_CYVIRTUALSCREEN)};
  auto inputs = make_inputs(operation, button, position, screen);
  if (inputs.empty())
    return ActionResult::failed(ActionError::invalid_definition, "invalid_mouse_action",
                                "The mouse operation is incomplete.");
  const UINT sent = sender_(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
  if (sent == inputs.size()) return ActionResult::succeeded();

  if (button && sent > 1) {
    bool logically_down = false;
    for (UINT index = 1; index < sent; ++index) {
      const DWORD flags = inputs[index].mi.dwFlags;
      if ((flags & (MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_MIDDLEDOWN |
                    MOUSEEVENTF_XDOWN)) != 0)
        logically_down = true;
      if ((flags & (MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP |
                    MOUSEEVENTF_XUP)) != 0)
        logically_down = false;
    }
    if (logically_down) {
      auto cleanup = button_inputs(*button).second;
      (void)sender_(1, &cleanup, sizeof(INPUT));
    }
  }
  return ActionResult::failed(ActionError::platform_failure, "mouse_injection_failed",
                              "Windows did not accept the complete mouse input sequence.");
}

}  // namespace strokes::actions
