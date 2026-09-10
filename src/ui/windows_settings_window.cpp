#include "ui/windows_settings_window.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

#include "ui/settings_controls.h"
#include "ui/settings_ids.h"

namespace strokes::ui {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusSettingsWindow";

constexpr std::array<input::ActivationButton, 4> configurable_buttons{
    input::ActivationButton::right, input::ActivationButton::middle,
    input::ActivationButton::x_button_1, input::ActivationButton::x_button_2};

std::optional<std::size_t> button_index(input::ActivationButton button) {
  const auto found = std::ranges::find(configurable_buttons, button);
  if (found == configurable_buttons.end()) return std::nullopt;
  return static_cast<std::size_t>(found - configurable_buttons.begin());
}

BOOL CALLBACK apply_font(HWND window, LPARAM font) {
  ::SendMessageW(window, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
  return TRUE;
}
}  // namespace

using detail::control;
using detail::integer;
using detail::number;
using detail::read_double;
using detail::read_integer;
using detail::text;

bool WindowsSettingsWindow::show(HINSTANCE instance, config::ConfigurationBundle& configuration) {
  instance_ = instance;
  destination_ = &configuration;
  working_ = configuration;
  accepted_ = false;
  finished_ = false;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&wc);
  if (registered == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  POINT cursor{};
  ::GetCursorPos(&cursor);
  MONITORINFO monitor{sizeof(monitor)};
  ::GetMonitorInfoW(::MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
  constexpr int width = 680;
  constexpr int height = 760;
  const int x = monitor.rcWork.left + (monitor.rcWork.right - monitor.rcWork.left - width) / 2;
  const int y = monitor.rcWork.top + (monitor.rcWork.bottom - monitor.rcWork.top - height) / 2;
  window_ = ::CreateWindowExW(WS_EX_APPWINDOW, class_name, L"Strokes++ Settings",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, x, y,
                              width, height, nullptr, nullptr, instance_, this);
  if (!window_) {
    if (registered != 0) ::UnregisterClassW(class_name, instance_);
    return false;
  }
  ::SetWindowPos(window_, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
  ::SetForegroundWindow(window_);
  ::SetActiveWindow(window_);
  ::SetWindowPos(window_, HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
  ::UpdateWindow(window_);
  MSG message{};
  while (!finished_ && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!::IsDialogMessageW(window_, &message)) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
  }
  if (message.message == WM_QUIT) ::PostQuitMessage(static_cast<int>(message.wParam));
  if (accepted_) *destination_ = std::move(working_);
  gestures_.reset();
  profiles_.reset();
  if (registered != 0) ::UnregisterClassW(class_name, instance_);
  return accepted_;
}

LRESULT CALLBACK WindowsSettingsWindow::window_proc(HWND window, UINT message, WPARAM wp,
                                                    LPARAM lp) {
  auto* self = reinterpret_cast<WindowsSettingsWindow*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self =
        static_cast<WindowsSettingsWindow*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsSettingsWindow::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_CREATE) {
    create_controls();
    load_values();
    return 0;
  }
  if (message == WM_DPICHANGED) {
    const UINT dpi = HIWORD(wp);
    rescale_children(current_dpi_, dpi);
    current_dpi_ = dpi;
    const auto* bounds = reinterpret_cast<const RECT*>(lp);
    ::SetWindowPos(window_, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
                   bounds->bottom - bounds->top, SWP_NOACTIVATE | SWP_NOZORDER);
    return 0;
  }
  if (message == WM_COMMAND) return handle_command(wp);
  if (message == WM_CLOSE) {
    ::DestroyWindow(window_);
    return 0;
  }
  if (message == WM_DESTROY) {
    window_ = nullptr;
    finished_ = true;
    return 0;
  }
  return ::DefWindowProcW(window_, message, wp, lp);
}

LRESULT WindowsSettingsWindow::handle_command(WPARAM wp) {
  // The dialog manager reports Enter and Escape as IDOK and IDCANCEL, which are
  // the only identifiers it can know about for a plain window.
  const int raw = LOWORD(wp);
  const int command = raw == IDOK ? save_id : raw == IDCANCEL ? cancel_id : raw;
  const int notification = HIWORD(wp);
  if (command == save_id) {
    if (save_values()) {
      accepted_ = true;
      ::DestroyWindow(window_);
    }
    return 0;
  }
  if (command == cancel_id) {
    ::DestroyWindow(window_);
    return 0;
  }
  if (command == help_id) {
    show_help();
    return 0;
  }
  if (!gestures_ || !profiles_) return 0;
  const std::string previous = gestures_->selected();
  if (gestures_->handle(command, notification)) {
    const std::string current = gestures_->selected();
    if (current != previous) profiles_->load(current);
    return 0;
  }
  (void)profiles_->handle(command, notification, gestures_->selected());
  return 0;
}

void WindowsSettingsWindow::create_controls() {
  text(window_, 0, L"Simple settings", 16, 12, 220);
  control(window_, L"BUTTON", L"Gestures enabled", BS_AUTOCHECKBOX, enabled_id, 20, 42, 180, 24);
  text(window_, 0, L"Activation button", 20, 76);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, button_id, 200, 72, 180, 180);
  for (auto* value : {L"Right", L"Middle", L"XButton1", L"XButton2"})
    ::SendDlgItemMessageW(window_, button_id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
  control(window_, L"BUTTON", L"Overlay enabled", BS_AUTOCHECKBOX, overlay_id, 20, 110, 180, 24);
  text(window_, 0, L"Overlay line width", 20, 144);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, width_id, 200, 140, 100, 24);
  text(window_, 0, L"Overlay opacity (0-1)", 20, 178);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, opacity_id, 200, 174, 100, 24);

  text(window_, 0, L"Advanced settings", 16, 212, 220);
  text(window_, 0, L"Movement threshold", 20, 242);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, move_id, 200, 238, 100, 24);
  text(window_, 0, L"Point distance", 20, 276);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, distance_id, 200, 272, 100, 24);
  text(window_, 0, L"Maximum points", 20, 310);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, max_id, 200, 306, 100, 24);
  text(window_, 0, L"Recognition threshold", 20, 344);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, threshold_id, 200, 340, 100, 24);

  gestures_.emplace(window_, instance_, working_);
  profiles_.emplace(window_, working_);
  gestures_->create();
  profiles_->create();

  control(window_, L"BUTTON", L"Help", BS_PUSHBUTTON, help_id, 350, 670, 90, 30);
  control(window_, L"BUTTON", L"Save", BS_DEFPUSHBUTTON, save_id, 450, 670, 90, 30);
  control(window_, L"BUTTON", L"Cancel", BS_PUSHBUTTON, cancel_id, 550, 670, 90, 30);
  ::EnumChildWindows(window_, apply_font,
                     reinterpret_cast<LPARAM>(::GetStockObject(DEFAULT_GUI_FONT)));
  current_dpi_ = ::GetDpiForWindow(window_);
  if (current_dpi_ != 96) {
    rescale_children(96, current_dpi_);
    RECT bounds{};
    ::GetWindowRect(window_, &bounds);
    ::SetWindowPos(window_, nullptr, 0, 0,
                   ::MulDiv(bounds.right - bounds.left, static_cast<int>(current_dpi_), 96),
                   ::MulDiv(bounds.bottom - bounds.top, static_cast<int>(current_dpi_), 96),
                   SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
  }
}

void WindowsSettingsWindow::show_help() const noexcept {
  constexpr wchar_t help[] =
      L"SIMPLE SETTINGS\n\n"
      L"Gestures enabled\nTurns gesture recognition on or off. Ordinary mouse input should "
      L"continue to work while this is off.\n\n"
      L"Activation button\nThe mouse button you hold while drawing a gesture. A short press "
      L"without enough movement remains an ordinary click.\n\n"
      L"Overlay enabled\nShows the line you draw while capturing a gesture.\n\n"
      L"Overlay line width\nThe thickness of the on-screen gesture line, in pixels.\n\n"
      L"Overlay opacity\nThe visibility of the gesture line from 0 to 1. Lower values are more "
      L"transparent.\n\n"
      L"ADVANCED SETTINGS\n\n"
      L"Movement threshold\nHow far, in pixels at 100% display scaling, the pointer must move "
      L"before a held activation button becomes a gesture. Higher values make accidental gestures "
      L"less likely.\n\n"
      L"Point distance\nThe minimum distance between recorded stroke points. Lower values capture "
      L"more detail; higher values produce simpler strokes.\n\n"
      L"Maximum points\nThe largest number of points retained for one gesture. The default is "
      L"suitable for normal gestures.\n\n"
      L"Recognition threshold\nThe required similarity from 0 to 1. Higher values are stricter; "
      L"lower values accept more variation but can increase false matches.\n\n"
      L"GESTURES AND SHORTCUTS\n\n"
      L"Gestures lists the shapes you have created and their sample counts. Add creates one, "
      L"Rename changes its name, Delete removes it, Train records another example, and Remove last "
      L"sample removes its newest example. Enable / Disable controls whether that gesture can be "
      L"recognized.\n\n"
      L"Selected gesture global shortcut is the keyboard shortcut used when no matching application "
      L"profile overrides it. Enter a shortcut such as CTRL+W or ALT+LEFT, or a sequence such as "
      L"ALT+SPACE,N, then click Assign.\n\n"
      L"APPLICATION PROFILES\n\n"
      L"Profiles let the same gesture perform different shortcuts in different applications. Add, "
      L"Rename, Delete, and Enable / Disable manage the selected profile.\n\n"
      L"Match field chooses the application property: process name, window title, or window class. "
      L"Match mode chooses Exact, Contains, or Regular expression. Match value is the text or pattern "
      L"to compare. Add criterion adds it to the selected profile; Update criterion edits the selected "
      L"criterion; Remove criterion deletes it. All criteria in a profile must match.\n\n"
      L"Override shortcut replaces the global shortcut for the selected gesture when this profile "
      L"matches. Enter the shortcut and click Assign override.\n\n"
      L"Save applies all changes. Cancel closes Settings without applying them.";
  ::MessageBoxW(window_, help, L"Strokes++ Settings Help", MB_OK | MB_ICONINFORMATION);
}

void WindowsSettingsWindow::rescale_children(UINT old_dpi, UINT new_dpi) noexcept {
  if (old_dpi == 0 || old_dpi == new_dpi) return;
  for (HWND child = ::GetWindow(window_, GW_CHILD); child != nullptr;
       child = ::GetWindow(child, GW_HWNDNEXT)) {
    RECT bounds{};
    ::GetWindowRect(child, &bounds);
    POINT corners[]{{bounds.left, bounds.top}, {bounds.right, bounds.bottom}};
    ::MapWindowPoints(HWND_DESKTOP, window_, corners, 2);
    ::SetWindowPos(
        child, nullptr,
        ::MulDiv(corners[0].x, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        ::MulDiv(corners[0].y, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        ::MulDiv(corners[1].x - corners[0].x, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        ::MulDiv(corners[1].y - corners[0].y, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        SWP_NOACTIVATE | SWP_NOZORDER);
  }
}

void WindowsSettingsWindow::load_values() {
  auto& c = working_.global;
  ::CheckDlgButton(window_, enabled_id, c.gestures_enabled ? BST_CHECKED : BST_UNCHECKED);
  const auto selected_button = button_index(c.gesture_button).value_or(0);
  ::SendDlgItemMessageW(window_, button_id, CB_SETCURSEL, selected_button, 0);
  ::SetDlgItemTextW(window_, move_id, number(c.movement_threshold).c_str());
  ::SetDlgItemTextW(window_, distance_id, number(c.minimum_point_distance).c_str());
  ::SetDlgItemTextW(window_, max_id, integer(c.maximum_points).c_str());
  ::SetDlgItemTextW(window_, threshold_id, number(c.recognition_threshold).c_str());
  ::CheckDlgButton(window_, overlay_id, c.overlay.enabled ? BST_CHECKED : BST_UNCHECKED);
  ::SetDlgItemTextW(window_, width_id, integer(c.overlay.line_width).c_str());
  ::SetDlgItemTextW(window_, opacity_id, number(c.overlay.opacity).c_str());
  gestures_->refresh();
  profiles_->refresh();
  if (!working_.gestures.gestures.empty()) {
    ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, 0, 0);
    gestures_->load();
    profiles_->load(gestures_->selected());
  }
}

bool WindowsSettingsWindow::save_values() {
  auto copy = working_;
  double movement = 0, distance = 0, threshold = 0, opacity = 0;
  long maximum = 0, width = 0;
  if (!read_double(window_, move_id, 0, config::GlobalOptions::maximum_movement_threshold,
                   movement) ||
      !read_double(window_, distance_id, 0, config::GlobalOptions::maximum_point_distance,
                   distance) ||
      !read_integer(window_, max_id, 2,
                    static_cast<long>(config::GlobalOptions::maximum_point_limit), maximum) ||
      !read_double(window_, threshold_id, 0, 1, threshold) ||
      !read_integer(window_, width_id, 1, config::GlobalOptions::maximum_overlay_line_width,
                    width) ||
      !read_double(window_, opacity_id, config::GlobalOptions::minimum_overlay_opacity, 1,
                   opacity)) {
    ::MessageBoxW(window_, L"One or more numeric settings are invalid.", L"Strokes++",
                  MB_OK | MB_ICONERROR);
    return false;
  }
  if (distance > movement) distance = movement;
  if (!gestures_->save(copy)) return false;
  const auto button_selected = detail::selected_combo(
      window_, button_id, static_cast<LRESULT>(configurable_buttons.size()));
  if (!button_selected) {
    ::MessageBoxW(window_, L"Select a valid activation button.", L"Strokes++",
                  MB_OK | MB_ICONERROR);
    return false;
  }
  copy.global.gestures_enabled = ::IsDlgButtonChecked(window_, enabled_id) == BST_CHECKED;
  copy.global.gesture_button = configurable_buttons[static_cast<std::size_t>(*button_selected)];
  copy.global.movement_threshold = movement;
  copy.global.minimum_point_distance = distance;
  copy.global.maximum_points = static_cast<std::size_t>(maximum);
  copy.global.recognition_threshold = threshold;
  copy.global.overlay.enabled = ::IsDlgButtonChecked(window_, overlay_id) == BST_CHECKED;
  copy.global.overlay.line_width = static_cast<int>(width);
  copy.global.overlay.opacity = opacity;
  working_ = std::move(copy);
  return true;
}
}  // namespace strokes::ui
