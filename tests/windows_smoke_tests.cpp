#include <Windows.h>

#include <array>
#include <vector>

#include "context/windows_application_context.h"
#include "actions/windows_keyboard_input.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "overlay/windows_gesture_overlay.h"
#include "test_support.h"

namespace {
using strokes::tests::check;
bool pass_mouse(const strokes::input::MouseInputEvent&, void*) noexcept { return false; }
bool pass_escape(void*) noexcept { return false; }
}  // namespace

int main() {
  using namespace strokes;
  const auto right = input::WindowsMouseClick::make_inputs(input::ActivationButton::right, {10, 20},
                                                           {30, 40}, {0, 0, 1920, 1080});
  check(right[0].type == INPUT_MOUSE && (right[0].mi.dwFlags & MOUSEEVENTF_MOVE) != 0,
        "right click first positions the cursor at the press point");
  check(right[1].mi.dwFlags == MOUSEEVENTF_RIGHTDOWN && right[2].mi.dwFlags == MOUSEEVENTF_RIGHTUP,
        "right click emits balanced native button input");
  check((right[3].mi.dwFlags & MOUSEEVENTF_MOVE) != 0,
        "right click restores the current cursor position");
  std::vector<std::vector<INPUT>> mouse_batches;
  input::WindowsMouseClick partial_mouse([&](UINT count, INPUT* inputs, int) {
    mouse_batches.emplace_back(inputs, inputs + count);
    return mouse_batches.size() == 1 ? 2U : count;
  });
  check(!partial_mouse.click(input::ActivationButton::right, {10, 20}) &&
            mouse_batches.size() == 2 && mouse_batches.back().size() == 2 &&
            mouse_batches.back()[0].mi.dwFlags == MOUSEEVENTF_RIGHTUP,
        "partial mouse injection releases the pressed button and restores the cursor");

  std::vector<INPUT> keyboard_inputs;
  actions::WindowsKeyboardInput keyboard([&](UINT count, INPUT* inputs, int) {
    keyboard_inputs.assign(inputs, inputs + count);
    return count - 1;
  });
  const std::array key_events{actions::KeyEvent{actions::VirtualKey::control, true},
                              actions::KeyEvent{actions::VirtualKey::control, false}};
  check(!keyboard.send(key_events) && keyboard_inputs.size() == 2 &&
            keyboard_inputs[0].ki.dwFlags == 0 &&
            keyboard_inputs[1].ki.dwFlags == KEYEVENTF_KEYUP,
        "Windows keyboard backend translates events and reports a partial SendInput result");

  bool suppress_up = false;
  KBDLLHOOKSTRUCT escape{};
  escape.vkCode = VK_ESCAPE;
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, true, suppress_up) &&
            suppress_up,
        "handled Escape down is suppressed");
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, false, suppress_up) &&
            suppress_up,
        "repeated Escape down remains suppressed without repeating cancellation");
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYUP, escape, false, suppress_up) &&
            !suppress_up,
        "matching Escape up is suppressed and balanced");
  escape.flags = LLKHF_INJECTED;
  check(!input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, true, suppress_up),
        "injected Escape is ignored");

  overlay::WindowsGestureOverlay overlay;
  check(overlay.create(::GetModuleHandleW(nullptr), {false, 4, 217, RGB(0, 160, 255)}),
        "disabled Win32 overlay can be created without showing UI");
  overlay.destroy();
  context::WindowsApplicationContextProvider context;
  if (const auto foreground = context.foreground_application()) {
    check(foreground->process_id != 0, "available foreground application context has a process ID");
  }

  input::WindowsMouseHook mouse_hook;
  input::WindowsKeyboardHook keyboard_hook;
  check(mouse_hook.start(pass_mouse, nullptr), "low-level mouse hook installs");
  check(keyboard_hook.start(pass_escape, nullptr), "low-level keyboard hook installs");
  keyboard_hook.stop();
  mouse_hook.stop();
  check(!mouse_hook.running() && !keyboard_hook.running(), "low-level hooks uninstall cleanly");

  return strokes::tests::failures == 0 ? 0 : 1;
}
