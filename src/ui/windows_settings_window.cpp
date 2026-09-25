#include "ui/windows_settings_window.h"

#include <algorithm>
#include <array>
#include <CommCtrl.h>
#include <Richedit.h>
#include <optional>
#include <string>
#include <vector>

#include "ui/settings_controls.h"
#include "ui/settings_ids.h"

namespace strokes::ui {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusSettingsWindow";
constexpr wchar_t tooltip_property[] = L"StrokesPlusPlus.SettingsTooltip";
constexpr wchar_t gestures_enabled_help[] =
    L"Turns gesture recognition on or off. Ordinary mouse input should continue to work while "
    L"this is off.";
constexpr wchar_t activation_button_help[] =
    L"The mouse button you hold while drawing a gesture. A short press without enough movement "
    L"remains an ordinary click.";
constexpr wchar_t overlay_enabled_help[] = L"Shows the line you draw while capturing a gesture.";
constexpr wchar_t overlay_width_help[] =
    L"The thickness of the on-screen gesture line, in pixels.";
constexpr wchar_t overlay_opacity_help[] =
    L"The visibility of the gesture line from 0 to 1. Lower values are more transparent.";
constexpr wchar_t movement_threshold_help[] =
    L"How far, in pixels at 100% display scaling, the pointer must move before a held activation "
    L"button becomes a gesture. Higher values make accidental gestures less likely.";
constexpr wchar_t point_distance_help[] =
    L"The minimum distance between recorded stroke points. Lower values capture more detail; "
    L"higher values produce simpler strokes.";
constexpr wchar_t maximum_points_help[] =
    L"The largest number of points retained for one gesture. The default is suitable for normal "
    L"gestures.";
constexpr wchar_t recognition_threshold_help[] =
    L"The required similarity from 0 to 1. Higher values are stricter; lower values accept more "
    L"variation but can increase false matches.";

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
  advanced_expanded_ = false;
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
  inventory_.reset();
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
  if (message == WM_NOTIFY) {
    const auto* notification = reinterpret_cast<const NMHDR*>(lp);
    if (notification && notification->idFrom == editor_tabs_id &&
        notification->code == TCN_SELCHANGE) {
      select_editor_tab();
      return 0;
    }
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
  if (command == advanced_section_label_id && notification == BN_CLICKED) {
    advanced_expanded_ = !advanced_expanded_;
    show_advanced_settings(true);
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
  INITCOMMONCONTROLSEX common_controls{sizeof(common_controls), ICC_WIN95_CLASSES};
  (void)::InitCommonControlsEx(&common_controls);
  tooltip_ = ::CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr,
                               WS_POPUP | TTS_ALWAYSTIP | TTS_NOPREFIX, CW_USEDEFAULT,
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, window_, nullptr,
                               instance_, nullptr);
  if (tooltip_) {
    (void)::SetPropW(window_, tooltip_property, tooltip_);
    ::SendMessageW(tooltip_, TTM_SETMAXTIPWIDTH, 0, 420);
    ::SetWindowPos(tooltip_, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
  }
  editor_tabs_ = control(window_, WC_TABCONTROLW, L"", WS_CLIPSIBLINGS, editor_tabs_id, 10, 10,
                         650, 650);
  TCITEMW tab{};
  tab.mask = TCIF_TEXT;
  tab.pszText = const_cast<wchar_t*>(L"Options");
  (void)::SendMessageW(editor_tabs_, TCM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&tab));
  tab.pszText = const_cast<wchar_t*>(L"Global Actions");
  (void)::SendMessageW(editor_tabs_, TCM_INSERTITEMW, 1, reinterpret_cast<LPARAM>(&tab));
  tab.pszText = const_cast<wchar_t*>(L"Applications");
  (void)::SendMessageW(editor_tabs_, TCM_INSERTITEMW, 2, reinterpret_cast<LPARAM>(&tab));
  tab.pszText = const_cast<wchar_t*>(L"Gestures");
  (void)::SendMessageW(editor_tabs_, TCM_INSERTITEMW, 3, reinterpret_cast<LPARAM>(&tab));
  tab.pszText = const_cast<wchar_t*>(L"Help");
  (void)::SendMessageW(editor_tabs_, TCM_INSERTITEMW, 4, reinterpret_cast<LPARAM>(&tab));
  (void)::SendMessageW(editor_tabs_, TCM_SETCURSEL, 0, 0);

  text(window_, settings_section_label_id, L"Simple settings", 25, 50, 220);
  auto* enabled = control(window_, L"BUTTON", L"Gestures enabled", BS_AUTOCHECKBOX, enabled_id,
                          30, 82, 180, 24);
  add_tooltip(enabled, gestures_enabled_help);
  add_tooltip(text(window_, activation_label_id, L"Activation button", 30, 116),
              activation_button_help);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, button_id, 210, 112, 180, 180);
  for (auto* value : {L"Right", L"Middle", L"XButton1", L"XButton2"})
    ::SendDlgItemMessageW(window_, button_id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
  auto* overlay = control(window_, L"BUTTON", L"Overlay enabled", BS_AUTOCHECKBOX, overlay_id,
                          30, 150, 180, 24);
  add_tooltip(overlay, overlay_enabled_help);
  add_tooltip(text(window_, overlay_width_label_id, L"Overlay line width", 30, 184),
              overlay_width_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, width_id, 210, 180, 100, 24);
  add_tooltip(text(window_, overlay_opacity_label_id, L"Overlay opacity (0-1)", 30, 218),
              overlay_opacity_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, opacity_id, 210, 214, 100, 24);

  control(window_, L"BUTTON", L"\x25B6 Advanced settings", BS_PUSHBUTTON,
          advanced_section_label_id, 25, 254, 180, 28);
  add_tooltip(text(window_, movement_label_id, L"Movement threshold", 30, 294),
              movement_threshold_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, move_id, 210, 290, 100, 24);
  add_tooltip(text(window_, distance_label_id, L"Point distance", 30, 328), point_distance_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, distance_id, 210, 324, 100, 24);
  add_tooltip(text(window_, maximum_label_id, L"Maximum points", 30, 362), maximum_points_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, max_id, 210, 358, 100, 24);
  add_tooltip(text(window_, recognition_label_id, L"Recognition threshold", 30, 396),
              recognition_threshold_help);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, threshold_id, 210, 392, 100, 24);

  gestures_.emplace(window_, instance_, working_);
  inventory_.emplace(window_, instance_, working_);
  profiles_.emplace(window_, instance_, working_);
  gestures_->create();
  inventory_->create();
  profiles_->create();
  (void)::LoadLibraryW(L"Msftedit.dll");
  control(window_, MSFTEDIT_CLASS, L"",
          ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL, help_text_id, 25, 50, 610,
          590);
  select_editor_tab();

  control(window_, L"BUTTON", L"Save", BS_DEFPUSHBUTTON, save_id, 450, 670, 90, 30);
  control(window_, L"BUTTON", L"Cancel", BS_PUSHBUTTON, cancel_id, 550, 670, 90, 30);
  ::EnumChildWindows(window_, apply_font,
                     reinterpret_cast<LPARAM>(::GetStockObject(DEFAULT_GUI_FONT)));
  load_help();
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

void WindowsSettingsWindow::select_editor_tab() const noexcept {
  if (!editor_tabs_ || !gestures_ || !inventory_ || !profiles_) return;
  const LRESULT selected = ::SendMessageW(editor_tabs_, TCM_GETCURSEL, 0, 0);
  show_settings(selected == 0);
  gestures_->set_visible(selected == 1);
  profiles_->set_visible(selected == 2);
  inventory_->set_visible(selected == 3);
  ::ShowWindow(::GetDlgItem(window_, help_text_id), selected == 4 ? SW_SHOW : SW_HIDE);
}

void WindowsSettingsWindow::show_settings(bool visible) const noexcept {
  for (const int id : {settings_section_label_id, advanced_section_label_id, activation_label_id,
                       overlay_width_label_id, overlay_opacity_label_id, enabled_id, button_id,
                       overlay_id, width_id, opacity_id})
    ::ShowWindow(::GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE);
  show_advanced_settings(visible);
}

void WindowsSettingsWindow::show_advanced_settings(bool settings_visible) const noexcept {
  HWND toggle = ::GetDlgItem(window_, advanced_section_label_id);
  if (toggle)
    ::SetWindowTextW(toggle,
                     advanced_expanded_ ? L"\x25BC Advanced settings"
                                        : L"\x25B6 Advanced settings");
  const bool visible = settings_visible && advanced_expanded_;
  for (const int id : {movement_label_id, distance_label_id, maximum_label_id,
                       recognition_label_id, move_id, distance_id, max_id, threshold_id})
    ::ShowWindow(::GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE);
}

void WindowsSettingsWindow::add_tooltip(HWND target, const wchar_t* description) const noexcept {
  if (!tooltip_ || !target) return;
  TTTOOLINFOW tool{TTTOOLINFOW_V1_SIZE};
  tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
  tool.hwnd = window_;
  tool.uId = reinterpret_cast<UINT_PTR>(target);
  tool.lpszText = const_cast<wchar_t*>(description);
  (void)::SendMessageW(tooltip_, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
}

void WindowsSettingsWindow::load_help() const noexcept {
  struct Span {
    LONG start;
    LONG label_end;
    LONG end;
    bool heading;
    bool shaded;
  };
  HWND help = ::GetDlgItem(window_, help_text_id);
  if (!help) return;
  std::wstring display;
  std::vector<Span> spans;
  const auto section = [&](const wchar_t* title) {
    // Rich Edit uses a single carriage return for an internal paragraph break.
    // Keeping that representation here makes the formatting character offsets exact.
    if (!display.empty()) display += L'\r';
    const LONG start = static_cast<LONG>(display.size());
    display += title;
    display += L"\r--------------------------------------------------------\r";
    const LONG end = static_cast<LONG>(display.size());
    spans.push_back({start, end, end, true, false});
  };
  bool shaded = false;
  const auto row = [&](const wchar_t* label, const wchar_t* description) {
    const LONG start = static_cast<LONG>(display.size());
    display += label;
    const LONG label_end = static_cast<LONG>(display.size());
    display += L'\t';
    display += description;
    display += L'\r';
    spans.push_back({start, label_end, static_cast<LONG>(display.size()), false, shaded});
    shaded = !shaded;
  };

  section(L"Options");
  row(L"Gestures enabled", gestures_enabled_help);
  row(L"Activation button", activation_button_help);
  row(L"Overlay enabled", overlay_enabled_help);
  row(L"Overlay line width", overlay_width_help);
  row(L"Overlay opacity", overlay_opacity_help);
  section(L"Advanced options");
  row(L"Movement threshold", movement_threshold_help);
  row(L"Point distance", point_distance_help);
  row(L"Maximum points", maximum_points_help);
  row(L"Recognition threshold", recognition_threshold_help);
  section(L"Global Actions");
  row(L"Pattern selection",
      L"Selects a gesture pattern for editing or action assignment. Pattern names describe how "
      L"the gesture is drawn, such as Down Right or Diagonal Up-Right; they do not describe the "
      L"attached action.");
  row(L"Pattern editing",
      L"Add creates a pattern, Rename changes its direction-based name, Delete removes it, Train "
      L"records another example, and Remove last sample removes its newest example. Enable / "
      L"Disable controls recognition independently of action assignment.");
  row(L"Global action",
      L"An optional action used when no matching application profile overrides it. Add Action "
      L"opens an editor with unassigned gestures and triggers; Edit Action changes the selected "
      L"mapping. Choose a keyboard shortcut, program, URI, mouse, window, media, volume, "
      L"virtual-desktop, or Lua script action. Deleting an action does not remove the pattern.");
  row(L"Assigned action",
      L"Shows the action's actual details, such as Ctrl+C, rather than only its action type. The "
      L"list at left uses shorter task labels such as Copy, Minimize, or Next Track.");
  row(L"Input-only actions",
      L"Wheel and rocker actions are triggered by mouse-button combinations instead of a drawn "
      L"pattern. Selecting one clears the gesture fields and displays No Gesture Assigned while "
      L"keeping its action available for editing.");
  row(L"Wheel triggers",
      L"Hold the right mouse button and scroll up or down to change volume. These actions do not "
      L"have gesture drawings.");
  row(L"Rocker triggers",
      L"Hold right and click left for Back. Hold left and click right for Forward. These actions "
      L"do not have gesture drawings.");
  section(L"Applications");
  row(L"Profiles",
      L"Let the same gesture perform different actions in different applications. Add, Rename, "
      L"Delete, and Enable / Disable manage the selected profile.");
  row(L"Matching",
      L"Match field chooses process name, window title, or window class. Match mode chooses Exact, "
      L"Contains, or Regular expression. Match value is the text or pattern to compare. Add, Update, "
      L"and Remove criterion manage the rules. All criteria in a profile must match.");
  row(L"Override action",
      L"Replaces the global action for the selected gesture when this profile matches. Configure "
      L"override opens the same action editor.");
  row(L"Built-in profiles",
      L"Chrome overrides Right Up for New Tab, Up Down for Reload, and Left Down for Reopen Closed "
      L"Tab. Excel overrides Right and Left for the next and previous worksheet.");
  section(L"Gestures");
  row(L"Inventory",
      L"Shows all 41 built-in direction and letter-shaped patterns, including patterns that have no global or "
      L"application action assigned.");
  row(L"Activated",
      L"Shows enabled patterns in blue. Activated means the pattern can be recognized; it does not "
      L"mean that an action is assigned.");
  row(L"Not Activated",
      L"Shows disabled patterns in gray. They remain in the catalog and can be enabled later.");
  row(L"Pattern names",
      L"Names record the stroke directions in drawing order. Diagonal names include both vertical "
      L"and horizontal direction; multi-part names list each successive direction. Letter-shaped "
      L"patterns are named M, P, e, and C for the shape that is drawn.");
  row(L"Default mappings",
      L"The built-in StrokesPlus.net-style defaults cover copy, paste, select all, screen capture, "
      L"delete, escape, media control, Explorer, window management, and tab navigation. Defaults "
      L"can be edited or removed without deleting their gesture patterns.");
  row(L"Direction arrows",
      L"The arrowhead shows drawing direction in the preview only. It is not stored as part of the "
      L"gesture sample.");
  row(L"Action assignments",
      L"Assign global behavior on the Global Actions tab and application-specific overrides on the "
      L"Applications tab. A pattern does not need an assignment to remain in this inventory.");
  section(L"Lua scripting");
  row(L"Lua Script action",
      L"Choose Lua Script in the action editor to write a script instead of picking a built-in "
      L"action. Scripts can branch on the gesture, the target application, and live keyboard "
      L"state, and they call the same automation the built-in actions use.");
  row(L"Validate and Test",
      L"Validate reports syntax errors without running the script. Test runs it immediately "
      L"against the real automation services, so it can move windows and send input. A tested "
      L"script was not produced by a gesture, so the gesture and application values are absent "
      L"and window actions report that the target is unavailable.");
  row(L"API Help",
      L"Lists every namespace and function with its parameters, return value, and description. "
      L"Automation functions return true and raise a catchable Lua error on failure.");
  row(L"Shared scripts",
      L"Functions defined in scripts\\init.lua, in the Strokes++ configuration folder under "
      L"Local AppData, are available to every Lua action. Modules placed in its scripts\\modules "
      L"folder load with require(\"name\").");
  row(L"Reload Lua Scripts",
      L"The notification-area menu reloads the initialization script and cached modules without "
      L"restarting Strokes++. Reloading clears values scripts stored in Lua globals.");
  row(L"Execution limits",
      L"A script that runs too long is interrupted and reported as a failed action, and a failed "
      L"script never disables gestures.");
  section(L"Saving changes");
  row(L"Save", L"Applies all changes and closes Settings.");
  row(L"Cancel", L"Closes Settings without applying the current changes.");

  ::SetWindowTextW(help, display.c_str());
  PARAFORMAT2 paragraph{};
  paragraph.cbSize = sizeof(paragraph);
  paragraph.dwMask = PFM_TABSTOPS | PFM_SPACEAFTER | PFM_LINESPACING;
  paragraph.cTabCount = 1;
  paragraph.rgxTabs[0] = 1900;
  paragraph.dySpaceAfter = 50;
  paragraph.bLineSpacingRule = 0;
  ::SendMessageW(help, EM_SETSEL, 0, -1);
  ::SendMessageW(help, EM_SETPARAFORMAT, 0, reinterpret_cast<LPARAM>(&paragraph));
  for (const auto& span : spans) {
    ::SendMessageW(help, EM_SETSEL, span.start, span.end);
    CHARFORMAT2W format{};
    format.cbSize = sizeof(format);
    format.dwMask = CFM_BOLD | CFM_BACKCOLOR | CFM_SIZE;
    format.dwEffects = span.heading ? CFE_BOLD : 0;
    format.yHeight = span.heading ? 220 : 180;
    format.crBackColor = span.shaded ? RGB(242, 242, 242) : RGB(255, 255, 255);
    ::SendMessageW(help, EM_SETCHARFORMAT, SCF_SELECTION, reinterpret_cast<LPARAM>(&format));
    if (!span.heading) {
      ::SendMessageW(help, EM_SETSEL, span.start, span.label_end);
      CHARFORMAT2W label_format{};
      label_format.cbSize = sizeof(label_format);
      label_format.dwMask = CFM_BOLD;
      label_format.dwEffects = CFE_BOLD;
      ::SendMessageW(help, EM_SETCHARFORMAT, SCF_SELECTION,
                     reinterpret_cast<LPARAM>(&label_format));
    }
  }
  ::SendMessageW(help, EM_SETSEL, 0, 0);
}

void WindowsSettingsWindow::rescale_children(UINT old_dpi, UINT new_dpi) noexcept {
  if (old_dpi == 0 || old_dpi == new_dpi) return;
  for (HWND child = ::GetWindow(window_, GW_CHILD); child != nullptr;
       child = ::GetWindow(child, GW_HWNDNEXT)) {
    RECT bounds{};
    ::GetWindowRect(child, &bounds);
    POINT corners[]{{bounds.left, bounds.top}, {bounds.right, bounds.bottom}};
    ::MapWindowPoints(HWND_DESKTOP, window_, corners, 2);
    const auto combo_height = reinterpret_cast<INT_PTR>(
        ::GetPropW(child, L"StrokesPlusPlus.ComboDropHeight"));
    const int height = combo_height > 0
                           ? ::MulDiv(static_cast<int>(combo_height), static_cast<int>(new_dpi), 96)
                           : ::MulDiv(corners[1].y - corners[0].y, static_cast<int>(new_dpi),
                                      static_cast<int>(old_dpi));
    ::SetWindowPos(
        child, nullptr,
        ::MulDiv(corners[0].x, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        ::MulDiv(corners[0].y, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        ::MulDiv(corners[1].x - corners[0].x, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
        height,
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
