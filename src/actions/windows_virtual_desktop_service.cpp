#include "actions/windows_virtual_desktop_service.h"

namespace strokes::actions {
namespace {

INPUT key(WORD value, bool down) {
  INPUT input{};
  input.type = INPUT_KEYBOARD;
  input.ki.wVk = value;
  input.ki.dwFlags = down ? 0 : KEYEVENTF_KEYUP;
  return input;
}

}  // namespace

std::vector<INPUT> WindowsVirtualDesktopService::make_inputs(VirtualDesktopOperation operation) {
  WORD command{};
  switch (operation) {
    case VirtualDesktopOperation::next: command = VK_RIGHT; break;
    case VirtualDesktopOperation::previous: command = VK_LEFT; break;
    case VirtualDesktopOperation::create: command = 'D'; break;
    case VirtualDesktopOperation::close: command = VK_F4; break;
  }
  if (command == 0) return {};
  return {key(VK_LWIN, true), key(VK_LCONTROL, true), key(command, true), key(command, false),
          key(VK_LCONTROL, false), key(VK_LWIN, false)};
}

ActionResult WindowsVirtualDesktopService::perform(VirtualDesktopOperation operation) {
  auto inputs = make_inputs(operation);
  if (inputs.empty())
    return ActionResult::failed(ActionError::unsupported_operation, "unknown_desktop_operation",
                                "The virtual desktop operation is unsupported.");
  const UINT sent = sender_(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
  if (sent == inputs.size()) return ActionResult::succeeded();

  INPUT cleanup[3]{key(inputs[2].ki.wVk, false), key(VK_LCONTROL, false), key(VK_LWIN, false)};
  (void)sender_(3, cleanup, sizeof(INPUT));
  return ActionResult::failed(ActionError::platform_failure, "desktop_input_failed",
                              "Windows did not accept the virtual desktop command.");
}

}  // namespace strokes::actions
