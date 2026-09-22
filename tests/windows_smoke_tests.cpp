#include <Windows.h>
#include <CommCtrl.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <string_view>
#include <thread>
#include <vector>

#include "context/windows_application_context.h"
#include "context/windows_shell_surface.h"
#include "actions/windows_keyboard_input.h"
#include "actions/windows_audio_service.h"
#include "actions/windows_mouse_service.h"
#include "actions/windows_media_service.h"
#include "actions/windows_process_service.h"
#include "actions/windows_shell_service.h"
#include "actions/windows_window_service.h"
#include "actions/windows_virtual_desktop_service.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "overlay/windows_gesture_overlay.h"
#include "test_support.h"
#include "tray/windows_tray_icon.h"
#include "ui/settings_ids.h"
#include "ui/windows_action_editor.h"
#include "ui/windows_settings_window.h"

namespace {
using strokes::tests::check;
bool pass_mouse(const strokes::input::MouseInputEvent&, void*) noexcept { return false; }
bool pass_escape(void*) noexcept { return false; }
bool combo_exposes(HWND window, int id, LRESULT choices) {
  HWND combo = ::GetDlgItem(window, id);
  RECT dropped{};
  const LRESULT item_height = ::SendMessageW(combo, CB_GETITEMHEIGHT, 0, 0);
  return combo != nullptr && ::SendMessageW(combo, CB_GETCOUNT, 0, 0) == choices &&
         ::SendMessageW(combo, CB_GETDROPPEDCONTROLRECT, 0,
                        reinterpret_cast<LPARAM>(&dropped)) != CB_ERR &&
         dropped.bottom - dropped.top >= item_height * 2;
}
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
  for (const auto class_name : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
                                L"NotifyIconOverflowWindow",
                                L"TopLevelWindowForOverflowXamlIsland"}) {
    check(context::is_protected_shell_class(class_name),
          "taskbar and notification-overflow window classes are protected from gestures");
  }
  check(!context::is_protected_shell_class(L"ApplicationFrameWindow") &&
            !context::is_protected_shell_class(L"Windows.UI.Core.CoreWindow"),
        "generic application and XAML windows remain valid gesture targets");
  check(actions::WindowsAudioService::adjusted_level(
            0.50F, actions::VolumeOperation::increase, 7.5) == 0.575F,
        "volume increase honors its configured percentage");
  check(actions::WindowsAudioService::adjusted_level(
            0.50F, actions::VolumeOperation::decrease, std::nullopt) == 0.48F,
        "volume decrease uses the default two-percent step");
  check(actions::WindowsAudioService::adjusted_level(
            0.98F, actions::VolumeOperation::increase, 10.0) == 1.0F &&
            actions::WindowsAudioService::adjusted_level(
                0.02F, actions::VolumeOperation::decrease, 10.0) == 0.0F,
        "volume adjustments clamp to the endpoint limits");
  actions::WindowsProcessService process_service;
  const auto missing_process = process_service.launch(
      {actions::ProcessOperation::launch, "Z:\\missing\\strokes-plus-plus-test.exe", {}, {}});
  check(!missing_process.success &&
            missing_process.error == actions::ActionError::invalid_runtime_target &&
            missing_process.code == "executable_not_found",
        "Windows process service reports a missing executable without launching anything");
  check(process_service
            .launch({actions::ProcessOperation::launch, "cmd.exe", "/D /C exit 0",
                     std::filesystem::current_path().string()})
            .success,
        "Windows process service launches an existing executable with arguments and a working directory");
  const auto invalid_process_text = process_service.launch(
      {actions::ProcessOperation::launch, std::string("cmd.exe\xFF", 8), {}, {}});
  check(!invalid_process_text.success && invalid_process_text.code == "invalid_process_text",
        "Windows process service rejects invalid UTF-8 launch parameters");
  const auto invalid_working_directory = process_service.launch(
      {actions::ProcessOperation::launch, "cmd.exe", "/D /C exit 0",
       "Z:\\missing\\strokes-plus-plus-directory"});
  check(!invalid_working_directory.success &&
            invalid_working_directory.error == actions::ActionError::platform_failure &&
            invalid_working_directory.code == "process_launch_failed",
        "Windows process service returns structured launch failures");
  std::vector<std::wstring> opened_uris;
  actions::WindowsShellService shell([&](std::wstring_view uri) {
    opened_uris.emplace_back(uri);
    return std::intptr_t{33};
  });
  for (const auto* uri : {"https://example.com", "http://example.com", "mailto:test@example.com",
                          "ms-settings:display", "custom-scheme:value"}) {
    check(shell.open_uri(uri).success, "registered URI schemes delegate to the Windows shell");
  }
  check(opened_uris.size() == 5 && opened_uris.back() == L"custom-scheme:value",
        "shell service preserves HTTP and arbitrary registered URI values");
  actions::WindowsShellService failing_shell([](std::wstring_view) { return std::intptr_t{31}; });
  const auto shell_failure = failing_shell.open_uri("https://example.com");
  check(!shell_failure.success && shell_failure.error == actions::ActionError::platform_failure,
        "shell handler failures return a structured platform error");
  const RECT negative_screen{-1920, 0, 1920, 1080};
  const auto generic_mouse = actions::WindowsMouseService::make_inputs(
      actions::MouseOperation::double_click, actions::MouseButton::x_button_2,
      {-1000, 200}, negative_screen);
  check(generic_mouse.size() == 5 &&
            generic_mouse[0].mi.dwFlags ==
                (MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK) &&
            generic_mouse[1].mi.dwFlags == MOUSEEVENTF_XDOWN &&
            generic_mouse[1].mi.mouseData == XBUTTON2 &&
            generic_mouse[4].mi.dwFlags == MOUSEEVENTF_XUP,
        "generic mouse service builds normalized cross-monitor XButton double clicks");
  for (const auto button : {actions::MouseButton::left, actions::MouseButton::right,
                            actions::MouseButton::middle, actions::MouseButton::x_button_1,
                            actions::MouseButton::x_button_2}) {
    for (const auto operation : {actions::MouseOperation::click,
                                 actions::MouseOperation::double_click,
                                 actions::MouseOperation::button_down,
                                 actions::MouseOperation::button_up}) {
      const auto inputs = actions::WindowsMouseService::make_inputs(
          operation, button, {-1000, 200}, negative_screen);
      const std::size_t expected = operation == actions::MouseOperation::double_click ? 5 :
                                   operation == actions::MouseOperation::click ? 3 : 2;
      check(inputs.size() == expected,
            "every mouse button supports click, double-click, down, and up construction");
    }
  }
  const auto move_inputs = actions::WindowsMouseService::make_inputs(
      actions::MouseOperation::move, std::nullopt, {-1920, 0}, negative_screen);
  check(move_inputs.size() == 1 && move_inputs[0].mi.dx == 0 && move_inputs[0].mi.dy == 0,
        "mouse movement normalizes the negative virtual-desktop origin");
  std::vector<std::vector<INPUT>> generic_mouse_batches;
  actions::WindowsMouseService failing_mouse([&](UINT count, INPUT* inputs, int) {
    generic_mouse_batches.emplace_back(inputs, inputs + count);
    return generic_mouse_batches.size() == 1 ? 2U : count;
  });
  const auto failed_mouse = failing_mouse.perform(actions::MouseOperation::click,
                                                   actions::MouseButton::left, {10, 10});
  check(!failed_mouse.success && generic_mouse_batches.size() == 2 &&
            generic_mouse_batches.back().size() == 1 &&
            generic_mouse_batches.back()[0].mi.dwFlags == MOUSEEVENTF_LEFTUP,
        "partial generic mouse injection releases a synthetically pressed button");
  const auto media_inputs =
      actions::WindowsMediaService::make_inputs(actions::MediaOperation::play_pause);
  check(media_inputs.size() == 2 && media_inputs[0].ki.wVk == VK_MEDIA_PLAY_PAUSE &&
            media_inputs[1].ki.dwFlags == KEYEVENTF_KEYUP,
        "media service builds a balanced media-key command");
  for (const auto operation : {actions::MediaOperation::play_pause,
                               actions::MediaOperation::next_track,
                               actions::MediaOperation::previous_track,
                               actions::MediaOperation::stop}) {
    const auto inputs = actions::WindowsMediaService::make_inputs(operation);
    check(inputs.size() == 2 && inputs[1].ki.dwFlags == KEYEVENTF_KEYUP,
          "every media operation builds a balanced key command");
  }
  int media_batches = 0;
  actions::WindowsMediaService failing_media([&](UINT count, INPUT*, int) {
    return ++media_batches == 1 ? 1U : count;
  });
  check(!failing_media.perform(actions::MediaOperation::stop).success && media_batches == 2,
        "partial media input attempts a balancing key release");
  const auto desktop_inputs = actions::WindowsVirtualDesktopService::make_inputs(
      actions::VirtualDesktopOperation::previous);
  check(desktop_inputs.size() == 6 && desktop_inputs[0].ki.wVk == VK_LWIN &&
            desktop_inputs[1].ki.wVk == VK_LCONTROL && desktop_inputs[2].ki.wVk == VK_LEFT &&
            desktop_inputs[5].ki.dwFlags == KEYEVENTF_KEYUP,
        "virtual desktop service builds a balanced Windows shortcut");
  for (const auto operation : {actions::VirtualDesktopOperation::next,
                               actions::VirtualDesktopOperation::previous,
                               actions::VirtualDesktopOperation::create,
                               actions::VirtualDesktopOperation::close}) {
    const auto inputs = actions::WindowsVirtualDesktopService::make_inputs(operation);
    check(inputs.size() == 6 && inputs[0].ki.wVk == VK_LWIN &&
              inputs[4].ki.dwFlags == KEYEVENTF_KEYUP &&
              inputs[5].ki.dwFlags == KEYEVENTF_KEYUP,
          "every virtual desktop operation builds a balanced Windows shortcut");
  }
  int desktop_batches = 0;
  actions::WindowsVirtualDesktopService failing_desktop([&](UINT count, INPUT*, int) {
    return ++desktop_batches == 1 ? 2U : count;
  });
  check(!failing_desktop.perform(actions::VirtualDesktopOperation::next).success &&
            desktop_batches == 2,
        "partial virtual desktop input attempts modifier and command cleanup");
  check(tray::WindowsTrayIcon::click_for_callback(WM_LBUTTONUP) == tray::TrayClick::toggle &&
            tray::WindowsTrayIcon::click_for_callback(WM_RBUTTONUP) ==
                tray::TrayClick::show_menu,
        "tray callback messages retain the shell mapping required for physical left-menu and right-toggle behavior");

  ui::ActionEditResult editor_result;
  std::jthread editor_thread([&] {
    ui::WindowsActionEditor editor;
    const auto existing = actions::ActionDefinition::keyboard("CTRL+W");
    editor_result = editor.edit(::GetModuleHandleW(nullptr), nullptr, &existing);
  });
  HWND action_window = nullptr;
  for (int attempt = 0; attempt < 100; ++attempt) {
    action_window = ::FindWindowW(L"StrokesPlusPlusActionEditor", L"Configure Action");
    if (action_window != nullptr && ::IsWindowVisible(action_window)) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  check(action_window != nullptr && ::IsWindowVisible(action_window),
        "generic action editor creates a visible modal window");
  check(action_window != nullptr && combo_exposes(action_window, 5001, 9),
        "action editor exposes every supported action type in its expanded dropdown");
  check(action_window != nullptr &&
            (::GetWindowLongPtrW(::GetDlgItem(action_window, 5001), GWL_STYLE) & WS_TABSTOP) != 0 &&
            (::GetWindowLongPtrW(::GetDlgItem(action_window, 5019), GWL_STYLE) & BS_DEFPUSHBUTTON) != 0,
        "action editor preserves dialog tab navigation and default Save semantics");
  if (action_window) {
    actions::WindowsWindowService window_service;
    const auto handle = reinterpret_cast<std::uintptr_t>(action_window);
    check(window_service.perform(actions::WindowOperation::minimize, handle, {}).success,
          "window service minimizes a live target");
    check(window_service.perform(actions::WindowOperation::restore, handle, {}).success,
          "window service restores a minimized target");
    check(window_service.perform(actions::WindowOperation::maximize, handle, {}).success,
          "window service maximizes a live target");
    check(window_service.perform(actions::WindowOperation::restore, handle, {}).success,
          "window service restores a maximized target");
    const auto activation =
        window_service.perform(actions::WindowOperation::activate, handle, {});
    check((activation.success && ::GetForegroundWindow() == action_window) ||
              (!activation.success && activation.code == "window_activation_denied"),
          "window service activates a live target or reports Windows foreground-policy denial");
    const auto before_move = window_service.bounds(handle);
    check(window_service
              .perform(actions::WindowOperation::move, handle,
                       {actions::WindowOperation::move, actions::WindowTarget::gesture_window,
                        80, 80, 1, 1})
              .success,
          "window service moves a live target");
    const auto after_move = window_service.bounds(handle);
    check(before_move && after_move && after_move->width == before_move->width &&
              after_move->height == before_move->height,
          "move ignores stored dimensions and preserves the existing size");
    const auto before_resize = window_service.bounds(handle);
    check(window_service
              .perform(actions::WindowOperation::resize, handle,
                       {actions::WindowOperation::resize, actions::WindowTarget::gesture_window,
                        -500, -500, 760, 660})
              .success,
          "window service resizes a live target");
    const auto after_resize = window_service.bounds(handle);
    check(before_resize && after_resize && after_resize->left == before_resize->left &&
              after_resize->top == before_resize->top,
          "resize ignores stored coordinates and preserves the existing origin");
    check(window_service
              .perform(actions::WindowOperation::move_resize, handle,
                       {actions::WindowOperation::move_resize,
                        actions::WindowTarget::gesture_window, 100, 100, 780, 680})
              .success,
          "window service moves and resizes a live target");
    ::SendDlgItemMessageW(action_window, 5001, CB_SETCURSEL, 3, 0);
    ::SendMessageW(action_window, WM_COMMAND, MAKEWPARAM(5001, CBN_SELCHANGE), 0);
    check(::IsWindowVisible(::GetDlgItem(action_window, 5004)) &&
              !::IsWindowVisible(::GetDlgItem(action_window, 5005)),
          "mouse action initially shows its position target without absolute coordinates");
    ::SendDlgItemMessageW(action_window, 5004, CB_SETCURSEL, 3, 0);
    ::SendMessageW(action_window, WM_COMMAND, MAKEWPARAM(5004, CBN_SELCHANGE), 0);
    check(::IsWindowVisible(::GetDlgItem(action_window, 5005)) &&
              ::IsWindowVisible(::GetDlgItem(action_window, 5006)),
          "absolute mouse position reveals coordinate controls");
    ::SendDlgItemMessageW(action_window, 5001, CB_SETCURSEL, 1, 0);
    ::SendMessageW(action_window, WM_COMMAND, MAKEWPARAM(5001, CBN_SELCHANGE), 0);
    check(::IsWindowVisible(::GetDlgItem(action_window, 5016)) &&
              ::IsWindowVisible(::GetDlgItem(action_window, 5017)),
          "process action exposes executable and working-directory browsers");
    ::SendDlgItemMessageW(action_window, 5001, CB_SETCURSEL, 8, 0);
    ::SendMessageW(action_window, WM_COMMAND, MAKEWPARAM(5001, CBN_SELCHANGE), 0);
    const LONG_PTR lua_style = ::GetWindowLongPtrW(::GetDlgItem(action_window, 5021), GWL_STYLE);
    check(::IsWindowVisible(::GetDlgItem(action_window, 5021)) &&
              (lua_style & ES_MULTILINE) != 0 && (lua_style & ES_WANTRETURN) != 0,
          "Lua actions expose a multiline script editor with native editing semantics");
    check(::IsWindowVisible(::GetDlgItem(action_window, 5023)) &&
              ::IsWindowVisible(::GetDlgItem(action_window, 5024)) &&
              ::IsWindowVisible(::GetDlgItem(action_window, 5025)),
          "Lua actions expose syntax validation and protected test execution controls");
  }
  if (action_window) {
    actions::WindowsWindowService window_service;
    check(window_service
              .perform(actions::WindowOperation::close,
                       reinterpret_cast<std::uintptr_t>(action_window), {})
              .success,
          "window service posts a close request to a live target");
  }
  editor_thread.join();
  check(!editor_result.accepted, "action editor Cancel leaves the mapping unchanged");

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
  HWND editor_tabs = ::GetDlgItem(settings_window, ui::editor_tabs_id);
  check(editor_tabs != nullptr && ::SendMessageW(editor_tabs, TCM_GETITEMCOUNT, 0, 0) == 5 &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::enabled_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::gestures_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::profiles_id)),
        "settings opens on a separate Options tab");
  (void)::SendMessageW(editor_tabs, TCM_SETCURSEL, 1, 0);
  NMHDR tab_change{editor_tabs, static_cast<UINT_PTR>(ui::editor_tabs_id), TCN_SELCHANGE};
  (void)::SendMessageW(settings_window, WM_NOTIFY, ui::editor_tabs_id,
                       reinterpret_cast<LPARAM>(&tab_change));
  check(!::IsWindowVisible(::GetDlgItem(settings_window, ui::enabled_id)) &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::gestures_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::profiles_id)) &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::gesture_preview_id)),
        "selecting Global Actions hides options and shows the gesture editor and gesture preview");
  check(::SendDlgItemMessageW(settings_window, ui::gesture_select_id, CB_GETCOUNT, 0, 0) == 41 &&
            ::SendDlgItemMessageW(settings_window, ui::gestures_id, LB_GETCOUNT, 0, 0) == 22 &&
            ::GetDlgItem(settings_window, ui::global_remove_id) != nullptr,
        "Global Actions separates the full gesture selector from assigned action entries");
  (void)::SendDlgItemMessageW(settings_window, ui::gestures_id, LB_SETCURSEL, 21, 0);
  (void)::SendMessageW(settings_window, WM_COMMAND,
                       MAKEWPARAM(ui::gestures_id, LBN_SELCHANGE),
                       reinterpret_cast<LPARAM>(::GetDlgItem(settings_window, ui::gestures_id)));
  wchar_t preview_text[64]{};
  wchar_t assigned_text[64]{};
  ::GetWindowTextW(::GetDlgItem(settings_window, ui::gesture_preview_id), preview_text, 64);
  ::GetDlgItemTextW(settings_window, ui::shortcut_id, assigned_text, 64);
  check(::SendDlgItemMessageW(settings_window, ui::gesture_select_id, CB_GETCURSEL, 0, 0) ==
                CB_ERR &&
            ::GetWindowTextLengthW(::GetDlgItem(settings_window, ui::gesture_name_id)) == 0 &&
            std::wstring_view(preview_text) == L"No Gesture Assigned" &&
            std::wstring_view(assigned_text) == L"ALT+RIGHT",
        "non-drawn triggers clear gesture fields while retaining their assigned action");
  (void)::SendMessageW(editor_tabs, TCM_SETCURSEL, 2, 0);
  (void)::SendMessageW(settings_window, WM_NOTIFY, ui::editor_tabs_id,
                       reinterpret_cast<LPARAM>(&tab_change));
  check(!::IsWindowVisible(::GetDlgItem(settings_window, ui::enabled_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::gestures_id)) &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::profiles_id)) &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::profile_criteria_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::gesture_preview_id)),
        "selecting Applications hides global-action controls and shows the profile editor");
  (void)::SendMessageW(editor_tabs, TCM_SETCURSEL, 3, 0);
  (void)::SendMessageW(settings_window, WM_NOTIFY, ui::editor_tabs_id,
                       reinterpret_cast<LPARAM>(&tab_change));
  check(::IsWindowVisible(::GetDlgItem(settings_window, ui::gesture_inventory_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::gestures_id)) &&
            !::IsWindowVisible(::GetDlgItem(settings_window, ui::profiles_id)),
        "selecting Gestures shows the independent gesture inventory");
  (void)::SendMessageW(editor_tabs, TCM_SETCURSEL, 4, 0);
  (void)::SendMessageW(settings_window, WM_NOTIFY, ui::editor_tabs_id,
                       reinterpret_cast<LPARAM>(&tab_change));
  HWND help_text = ::GetDlgItem(settings_window, ui::help_text_id);
  check(help_text != nullptr && ::IsWindowVisible(help_text) &&
            (::GetWindowLongPtrW(help_text, GWL_STYLE) & ES_READONLY) != 0 &&
            ::GetWindowTextLengthW(help_text) > 500,
        "selecting Help shows the complete explanation in a read-only tab");
  check(settings_window != nullptr && combo_exposes(settings_window, ui::button_id, 4),
        "activation-button dropdown expands to expose all configured choices");
  actions::WindowsWindowService window_service;
  const auto invalid_window = window_service.perform(
      actions::WindowOperation::maximize, 0x1,
      {actions::WindowOperation::maximize, actions::WindowTarget::gesture_window});
  check(!invalid_window.success &&
            invalid_window.error == actions::ActionError::invalid_runtime_target,
        "window service rejects a destroyed or invalid HWND before dispatch");
  const auto window_bounds = window_service.bounds(reinterpret_cast<std::uintptr_t>(settings_window));
  const auto monitor = window_service.monitor(reinterpret_cast<std::uintptr_t>(settings_window));
  check(window_bounds && window_bounds->width > 0 && window_bounds->height > 0,
        "window service exposes target bounds");
  check(monitor && !monitor->identifier.empty() && monitor->bounds.width > 0 &&
            monitor->work_area.height > 0,
        "window service exposes monitor identity, bounds, and work area");
  (void)::SendMessageW(editor_tabs, TCM_SETCURSEL, 0, 0);
  (void)::SendMessageW(settings_window, WM_NOTIFY, ui::editor_tabs_id,
                       reinterpret_cast<LPARAM>(&tab_change));
  ChildBoundsCheck bounds{settings_window};
  if (settings_window != nullptr) {
    ::GetClientRect(settings_window, &bounds.client);
    ::EnumChildWindows(settings_window, check_child_bounds, reinterpret_cast<LPARAM>(&bounds));
  }
  check(settings_window != nullptr && bounds.contained,
        "every visible settings control fits inside the client area");
  HWND advanced_toggle = ::GetDlgItem(settings_window, ui::advanced_section_label_id);
  check(settings_window != nullptr &&
            ::FindWindowExW(settings_window, nullptr, L"STATIC", L"Simple settings") != nullptr &&
            advanced_toggle != nullptr && !::IsWindowVisible(::GetDlgItem(settings_window, ui::move_id)),
        "settings collapses implementation-level advanced controls behind a caret by default");
  if (advanced_toggle != nullptr)
    ::SendMessageW(settings_window, WM_COMMAND,
                   MAKEWPARAM(ui::advanced_section_label_id, BN_CLICKED),
                   reinterpret_cast<LPARAM>(advanced_toggle));
  check(::IsWindowVisible(::GetDlgItem(settings_window, ui::move_id)) &&
            ::IsWindowVisible(::GetDlgItem(settings_window, ui::threshold_id)),
        "advanced-settings caret expands the advanced controls");
  const auto tooltip = reinterpret_cast<HWND>(
      ::GetPropW(settings_window, L"StrokesPlusPlus.SettingsTooltip"));
  check(tooltip != nullptr, "settings creates its mouse-over tooltip control");
  const LRESULT tooltip_count =
      tooltip != nullptr ? ::SendMessageW(tooltip, TTM_GETTOOLCOUNT, 0, 0) : 0;
  check(tooltip_count >= 9,
        "settings labels register their help descriptions as mouse-over tooltips (count " +
            std::to_string(tooltip_count) + ")");
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
  input::WindowsMouseClick partial_mouse(
      [&](UINT count, INPUT* inputs, int) {
        mouse_batches.emplace_back(inputs, inputs + count);
        return mouse_batches.size() == 1 ? 2U : count;
      },
      [](POINT* point) {
        *point = {30, 40};
        return true;
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
