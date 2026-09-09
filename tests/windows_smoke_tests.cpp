#include <Windows.h>

#include <iostream>

#include "context/windows_application_context.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "overlay/windows_gesture_overlay.h"

namespace {
int failures = 0;
void check(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    ++failures;
  }
}
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

  return failures == 0 ? 0 : 1;
}
