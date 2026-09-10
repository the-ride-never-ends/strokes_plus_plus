#include <Windows.h>

#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "context/windows_application_context.h"
#include "actions/windows_keyboard_input.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "overlay/windows_gesture_overlay.h"
#include "test_support.h"
#include "tray/windows_tray_icon.h"
#include "ui/settings_ids.h"
#include "ui/windows_settings_window.h"

namespace {
using strokes::tests::check;
bool pass_mouse(const strokes::input::MouseInputEvent&, void*) noexcept { return false; }
bool pass_escape(void*) noexcept { return false; }
struct ChildBoundsCheck {
  HWND parent{};
  RECT client{};
  bool contained{true};
};
BOOL CALLBACK check_child_bounds(HWND child, LPARAM context) {
  auto& check = *reinterpret_cast<ChildBoundsCheck*>(context);
  if (!::IsWindowVisible(child)) return TRUE;
  RECT bounds{};
  ::GetWindowRect(child, &bounds);
  POINT corners[]{{bounds.left, bounds.top}, {bounds.right, bounds.bottom}};
  ::MapWindowPoints(HWND_DESKTOP, check.parent, corners, 2);
  check.contained = check.contained && corners[0].x >= check.client.left &&
                    corners[0].y >= check.client.top && corners[1].x <= check.client.right &&
                    corners[1].y <= check.client.bottom;
  return TRUE;
}
}  // namespace

int main() {
  using namespace strokes;
  check(tray::WindowsTrayIcon::click_for_callback(WM_LBUTTONUP) == tray::TrayClick::toggle &&
            tray::WindowsTrayIcon::click_for_callback(WM_RBUTTONUP) ==
                tray::TrayClick::show_menu,
        "tray callback messages retain the shell mapping required for physical left-menu and right-toggle behavior");

  auto settings_configuration = config::ConfigurationStore::defaults();
  std::atomic<bool> settings_returned{false};
  std::jthread settings_thread([&] {
    ui::WindowsSettingsWindow settings;
    (void)settings.show(::GetModuleHandleW(nullptr), settings_configuration);
    settings_returned.store(true, std::memory_order_release);
  });
  HWND settings_window = nullptr;
  for (int attempt = 0; attempt < 100; ++attempt) {
    settings_window = ::FindWindowW(L"StrokesPlusPlusSettingsWindow", L"Strokes++ Settings");
    if (settings_window != nullptr && ::IsWindowVisible(settings_window)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  check(settings_window != nullptr && ::IsWindowVisible(settings_window),
        "settings command creates a visible top-level window");
  ChildBoundsCheck bounds{settings_window};
  if (settings_window != nullptr) {
    ::GetClientRect(settings_window, &bounds.client);
    ::EnumChildWindows(settings_window, check_child_bounds, reinterpret_cast<LPARAM>(&bounds));
  }
  check(settings_window != nullptr && bounds.contained,
        "every visible settings control fits inside the client area");
  check(settings_window != nullptr &&
            ::FindWindowExW(settings_window, nullptr, L"STATIC", L"Simple settings") != nullptr &&
            ::FindWindowExW(settings_window, nullptr, L"STATIC", L"Advanced settings") != nullptr,
        "settings visibly separates simple controls from implementation-level advanced controls");
  check(settings_window != nullptr && ::GetDlgItem(settings_window, ui::help_id) != nullptr,
        "settings window contains a Help button");
  if (settings_window != nullptr)
    ::PostMessageW(settings_window, WM_COMMAND, MAKEWPARAM(ui::help_id, BN_CLICKED), 0);
  HWND help_window = nullptr;
  for (int attempt = 0; attempt < 100; ++attempt) {
    help_window = ::FindWindowW(L"#32770", L"Strokes++ Settings Help");
    if (help_window != nullptr && ::IsWindowVisible(help_window)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  check(help_window != nullptr && ::IsWindowVisible(help_window),
        "Help button opens a visible settings explanation dialog");
  if (help_window != nullptr) ::PostMessageW(help_window, WM_CLOSE, 0, 0);
  if (settings_window != nullptr) ::PostMessageW(settings_window, WM_CLOSE, 0, 0);
  settings_thread.join();
  check(settings_returned.load(std::memory_order_acquire),
        "settings window responds to close and leaves its modal loop");

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

  actions::PhysicalKeyState keys;
  keys.update(actions::VirtualKey::left_shift, true);
  std::vector<INPUT> keyboard_inputs;
  actions::WindowsKeyboardInput keyboard(keys, [&](UINT count, INPUT* inputs, int) {
    keyboard_inputs.assign(inputs, inputs + count);
    return count - 1;
  });
  const std::array key_events{actions::KeyEvent{actions::VirtualKey::control, true},
                              actions::KeyEvent{actions::VirtualKey::control, false}};
  check(!keyboard.send(key_events) && keyboard_inputs.size() == 2 &&
            keyboard_inputs[0].ki.dwFlags == 0 &&
            keyboard_inputs[1].ki.dwFlags == KEYEVENTF_KEYUP,
        "Windows keyboard backend translates events and reports a partial SendInput result");
  const std::array shift_release{actions::KeyEvent{actions::VirtualKey::left_shift, false}};
  (void)keyboard.send(shift_release);
  check(keyboard.is_key_down(actions::VirtualKey::left_shift),
        "injecting a modifier key-up does not change the reported physical state");

  bool suppress_up = false;
  KBDLLHOOKSTRUCT escape{};
  escape.vkCode = VK_ESCAPE;
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, true, suppress_up) &&
            suppress_up,
        "handled Escape down is suppressed");
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYUP, escape, false, suppress_up) &&
            !suppress_up,
        "matching Escape up is suppressed and balanced");
  check(input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, true, suppress_up) &&
            suppress_up,
        "a later cancellation re-arms the key-up latch");
  check(!input::WindowsKeyboardHook::filter_escape(WM_KEYDOWN, escape, false, suppress_up) &&
            !suppress_up,
        "a declined Escape press clears a stale latch instead of swallowing the key");
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
  check(keyboard_hook.start(pass_escape, nullptr, keys), "low-level keyboard hook installs");
  keyboard_hook.stop();
  mouse_hook.stop();
  check(!mouse_hook.running() && !keyboard_hook.running(), "low-level hooks uninstall cleanly");

  return strokes::tests::failures == 0 ? 0 : 1;
}
