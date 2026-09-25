#include "ui/windows_action_editor.h"

#include <commdlg.h>
#include <Richedit.h>
#include <shlobj.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <utility>

#include "actions/lua_runtime.h"
#include "ui/lua_editor_environment.h"
#include "ui/settings_controls.h"

namespace strokes::ui {
namespace {

constexpr wchar_t class_name[] = L"StrokesPlusPlusActionEditor";
enum : int {
  type_id = 5001,
  operation_id,
  option_id,
  position_id,
  first_id,
  second_id,
  third_id,
  fourth_id,
  first_label_id,
  second_label_id,
  third_label_id,
  fourth_label_id,
  operation_label_id,
  option_label_id,
  position_label_id,
  browse_file_id,
  browse_directory_id,
  remove_id,
  save_action_id,
  cancel_action_id,
  lua_script_id,
  lua_script_label_id,
  validate_lua_id,
  test_lua_id,
  lua_help_id,
  help_text_id,
  help_close_id,
  global_target_id,
  global_target_label_id,
};

/// Action-type combo indices. The combo strings, this list and ActionType share one order.
enum : int {
  keyboard_type = 0,
  process_type,
  url_type,
  mouse_type,
  window_type,
  media_type,
  volume_type,
  desktop_type,
  lua_type,
};

// Every combo in this editor is filled in enum order and read back by index, so the two
// orders must stay identical. These pin the positions the index arithmetic below relies on.
static_assert(static_cast<int>(actions::ActionType::keyboard_shortcut) == keyboard_type);
static_assert(static_cast<int>(actions::ActionType::lua) == lua_type);
static_assert(static_cast<int>(actions::MouseOperation::move) == 4);
static_assert(static_cast<int>(actions::MouseButton::x_button_2) == 4);
static_assert(static_cast<int>(actions::PositionTarget::absolute) == 3);
static_assert(static_cast<int>(actions::WindowOperation::move) == 5);
static_assert(static_cast<int>(actions::WindowOperation::resize) == 6);
static_assert(static_cast<int>(actions::WindowOperation::move_resize) == 7);
static_assert(static_cast<int>(actions::WindowOperation::center) == 9);
static_assert(static_cast<int>(actions::WindowTarget::window_at_gesture_start) == 2);
static_assert(static_cast<int>(actions::MediaOperation::stop) == 3);
static_assert(static_cast<int>(actions::VolumeOperation::mute_toggle) == 2);
static_assert(static_cast<int>(actions::VirtualDesktopOperation::close) == 3);

void show(HWND window, int id, bool visible) {
  ::ShowWindow(::GetDlgItem(window, id), visible ? SW_SHOW : SW_HIDE);
}

void label(HWND window, int id, const wchar_t* value) {
  ::SetDlgItemTextW(window, id, value);
}

void reset_combo(HWND window, int id, std::initializer_list<const wchar_t*> values) {
  ::SendDlgItemMessageW(window, id, CB_RESETCONTENT, 0, 0);
  for (const auto* value : values)
    ::SendDlgItemMessageW(window, id, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(value));
  ::SendDlgItemMessageW(window, id, CB_SETCURSEL, 0, 0);
}

int selection(HWND window, int id) {
  const LRESULT value = ::SendDlgItemMessageW(window, id, CB_GETCURSEL, 0, 0);
  return value == CB_ERR ? -1 : static_cast<int>(value);
}

std::optional<int> integer(HWND window, int id) {
  const std::string text = detail::read_utf8(window, id);
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  errno = 0;
  const long value = std::strtol(text.c_str(), &end, 10);
  if (errno != 0 || end == text.c_str() || *end != '\0' || value < INT_MIN || value > INT_MAX)
    return std::nullopt;
  return static_cast<int>(value);
}

struct ApiEntry {
  const wchar_t* group;
  const wchar_t* signature;
  const wchar_t* parameters;
  const wchar_t* returns;
  const wchar_t* description;
};

constexpr ApiEntry api_entries[] = {
    {L"gesture", L"gesture.id, gesture.name, gesture.score", L"None.",
     L"Strings for id and name, a number between 0 and 1 for score.",
     L"Identifies the recognized gesture that ran this script."},
    {L"gesture", L"gesture.start.x, gesture.start.y, gesture.finish.x, gesture.finish.y",
     L"None.", L"Numbers in virtual-screen coordinates, which may be negative.",
     L"Reports where the stroke began and where it ended."},
    {L"gesture", L"gesture.duration, gesture.point_count, gesture.distance", L"None.",
     L"Milliseconds, the captured point count, and the traveled distance in pixels.",
     L"Describes the shape and timing of the stroke."},
    {L"application",
     L"application.process, application.process_id, application.title, application.class, "
     L"application.executable_path",
     L"None.",
     L"Strings, except process_id which is a number; executable_path is absent when Windows "
     L"denies the query.",
     L"Describes the application captured when the gesture began. Both context objects are "
     L"read-only, and both are absent when a script is run from Test."},
    {L"window", L"window.close([target]), minimize, maximize, restore, activate",
     L"target: \"gesture\" (default) or \"foreground\".",
     L"true, or raises a Lua error when the window is unavailable or Windows refuses.",
     L"Performs one lifecycle operation on the target window."},
    {L"window", L"window.move(x, y [, target])",
     L"x, y: virtual-screen coordinates. target: \"gesture\" (default) or \"foreground\".",
     L"true, or raises a Lua error when the move fails.",
     L"Moves the target window to the given desktop position."},
    {L"window", L"window.resize(width, height [, target])",
     L"width, height: positive pixel dimensions. target as above.",
     L"true, or raises a Lua error; zero or negative dimensions are rejected.",
     L"Resizes the target window without moving it."},
    {L"window", L"window.move_resize(x, y, width, height [, target])",
     L"Position and positive dimensions, then the optional target.",
     L"true, or raises a Lua error when the operation fails.",
     L"Moves and resizes the target window in one step."},
    {L"window", L"window.bounds([target])", L"target as above.",
     L"A read-only table with x, y, width and height.",
     L"Reads the target window rectangle in virtual-screen coordinates."},
    {L"window", L"window.exists([target])", L"target as above.", L"true or false.",
     L"Reports whether the target window is still a live window."},
    {L"window", L"window.title([target]), window.class([target]), window.process([target])",
     L"target as above.",
     L"A string, or raises a Lua error when the window or the value is unavailable.",
     L"Reads window metadata live, rather than from the captured context."},
    {L"keyboard", L"keyboard.hotkey(key, ...)",
     L"One or more key names, such as \"CTRL\", \"SHIFT\", \"T\"; no name may contain + or ,.",
     L"true, or raises a Lua error when a name is invalid or Windows refuses the input.",
     L"Sends the keys as one chord and restores the modifiers you are physically holding."},
    {L"keyboard", L"keyboard.press(key)", L"One key name; chords belong to keyboard.hotkey.",
     L"true, or raises a Lua error when the name is invalid.",
     L"Sends one key down and up without touching modifier state."},
    {L"keyboard", L"keyboard.down(key), keyboard.up(key)", L"One key name.",
     L"true, or raises a Lua error when the name is invalid.",
     L"Sends a single key-down or key-up event, so a chord can be held across calls."},
    {L"keyboard", L"keyboard.is_down(key)", L"One key name; modifiers are always supported.",
     L"true or false, or raises a Lua error for an unknown key.",
     L"Reports whether the key is physically held, ignoring injected input."},
    {L"mouse", L"mouse.position()", L"None.", L"A read-only table with x and y.",
     L"Reads the pointer position in virtual-screen coordinates."},
    {L"mouse", L"mouse.move(x, y)", L"x, y: finite virtual-screen coordinates, possibly negative.",
     L"true, or raises a Lua error when the coordinates are invalid.",
     L"Moves the pointer to an absolute desktop position."},
    {L"mouse", L"mouse.click(button), double_click(button), down(button), up(button)",
     L"button: \"left\", \"right\", \"middle\", \"x1\" or \"x2\", in any case.",
     L"true, or raises a Lua error for an unsupported button or unavailable cursor.",
     L"Generates the button event at the current pointer position."},
    {L"process", L"process.launch(path [, arguments [, working_directory]])",
     L"path: the executable. arguments and working_directory are optional strings.",
     L"true, or raises a Lua error carrying the Windows failure.",
     L"Starts a program without waiting for it."},
    {L"shell", L"shell.open(uri)", L"uri: a URL or registered URI, such as ms-settings:display.",
     L"true, or raises a Lua error when no handler accepts it.",
     L"Opens the target with its registered Windows handler."},
    {L"media", L"media.play_pause(), media.next(), media.previous(), media.stop()", L"None.",
     L"true, or raises a Lua error when the command is refused.",
     L"Sends the system media command to the active player."},
    {L"volume", L"volume.increase(amount), volume.decrease(amount)",
     L"amount: greater than 0 and at most 100, on the same scale as volume.set.",
     L"true, or raises a Lua error when no output endpoint is available.",
     L"Changes the default output level by a relative amount."},
    {L"volume", L"volume.toggle_mute(), volume.get(), volume.set(value), volume.is_muted()",
     L"value: 0 to 100.",
     L"get returns 0 to 100, is_muted returns true or false, the others return true.",
     L"Reads and sets the default output level and mute state."},
    {L"desktop", L"desktop.next(), desktop.previous(), desktop.create(), desktop.close()",
     L"None.", L"true, or raises a Lua error when the operation is unsupported.",
     L"Drives Windows virtual desktops with the system shortcuts."},
    {L"ui", L"ui.message(text), ui.osd(text)", L"text: a non-empty string.",
     L"true, or raises a Lua error when the message window is unavailable.",
     L"Shows the text on screen and returns at once; both dismiss themselves."},
    {L"log", L"log.debug(text), log.info(text), log.warn(text), log.error(text)",
     L"text: the message to record.",
     L"true, or raises a Lua error when the log cannot be written.",
     L"Writes one structured record to the application log."},
    {L"log", L"print(...)", L"Any values, converted with tostring.",
     L"true, or raises a Lua error when the log cannot be written.",
     L"Writes the values to the application log at info level; there is no console."},
};

std::wstring documentation() {
  std::wstring text =
      L"Strokes++ Lua API\r\n\r\nNamespaces: gesture, application, window, keyboard, mouse, "
      L"process, shell, media, volume, desktop, ui, log.\r\n\r\nAutomation functions return "
      L"true and raise a catchable Lua error on failure. Query functions return their "
      L"documented value. Shared code is loaded from scripts\\init.lua in the configuration "
      L"directory, and require loads modules from its scripts\\modules folder.\r\n";
  const wchar_t* group = nullptr;
  for (const auto& entry : api_entries) {
    if (group == nullptr || ::lstrcmpW(group, entry.group) != 0) {
      group = entry.group;
      text += L"\r\n=== ";
      text += group;
      text += L" ===\r\n";
    }
    text += L"\r\n";
    text += entry.signature;
    text += L"\r\n    Parameters: ";
    text += entry.parameters;
    text += L"\r\n    Returns: ";
    text += entry.returns;
    text += L"\r\n    ";
    text += entry.description;
    text += L"\r\n";
  }
  return text;
}

LRESULT CALLBACK help_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_NCCREATE)
    ::SetWindowLongPtrW(
        window, GWLP_USERDATA,
        reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams));
  if (message == WM_COMMAND &&
      (LOWORD(wp) == help_close_id || LOWORD(wp) == IDOK || LOWORD(wp) == IDCANCEL)) {
    ::DestroyWindow(window);
    return 0;
  }
  if (message == WM_CLOSE) {
    ::DestroyWindow(window);
    return 0;
  }
  if (message == WM_DESTROY) {
    if (auto* finished = reinterpret_cast<bool*>(::GetWindowLongPtrW(window, GWLP_USERDATA)))
      *finished = true;
    return 0;
  }
  return ::DefWindowProcW(window, message, wp, lp);
}

std::optional<double> number(HWND window, int id) {
  const std::string text = detail::read_utf8(window, id);
  if (text.empty()) return std::nullopt;
  char* end = nullptr;
  errno = 0;
  const double value = std::strtod(text.c_str(), &end);
  if (errno != 0 || end == text.c_str() || *end != '\0') return std::nullopt;
  return value;
}

}  // namespace

ActionEditResult WindowsActionEditor::edit(HINSTANCE instance, HWND owner,
                                           const actions::ActionDefinition* existing) {
  global_targets_.clear();
  return edit_impl(instance, owner, existing);
}

ActionEditResult WindowsActionEditor::add_global(HINSTANCE instance, HWND owner,
                                                 std::vector<GlobalActionTarget> targets,
                                                 const std::string& preferred_id) {
  global_targets_ = std::move(targets);
  if (global_targets_.empty()) return {};
  preferred_target_ = 0;
  for (std::size_t index = 0; index < global_targets_.size(); ++index) {
    if (global_targets_[index].id == preferred_id) {
      preferred_target_ = static_cast<int>(index);
      break;
    }
  }
  return edit_impl(instance, owner, nullptr);
}

ActionEditResult WindowsActionEditor::edit_impl(HINSTANCE instance, HWND owner,
                                                const actions::ActionDefinition* existing) {
  instance_ = instance;
  owner_ = owner;
  finished_ = false;
  result_ = {};
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance;
  wc.lpszClassName = class_name;
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&wc);
  if (registered == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return {};
  dpi_ = owner ? ::GetDpiForWindow(owner) : ::GetDpiForSystem();
  if (dpi_ == 0) dpi_ = 96;
  window_ = ::CreateWindowExW(WS_EX_DLGMODALFRAME, class_name,
                              global_targets_.empty() ? L"Configure Action" : L"Add Global Action",
                              WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
                              ::MulDiv(570, static_cast<int>(dpi_), 96),
                              ::MulDiv(global_targets_.empty() ? 430 : 470,
                                       static_cast<int>(dpi_), 96), owner, nullptr, instance,
                              this);
  if (!window_) {
    if (registered != 0) ::UnregisterClassW(class_name, instance);
    return {};
  }
  create_controls();
  rescale_children(96, dpi_);
  load(existing);
  ::EnableWindow(owner, FALSE);
  ::ShowWindow(window_, SW_SHOW);
  ::SetFocus(::GetDlgItem(window_, global_targets_.empty() ? type_id : global_target_id));
  ::SetForegroundWindow(window_);
  MSG message{};
  while (!finished_ && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!::IsDialogMessageW(window_, &message)) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
  }
  ::EnableWindow(owner, TRUE);
  ::SetForegroundWindow(owner);
  if (window_) ::DestroyWindow(window_);
  if (registered != 0) ::UnregisterClassW(class_name, instance);
  return std::move(result_);
}

LRESULT CALLBACK WindowsActionEditor::window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<WindowsActionEditor*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<WindowsActionEditor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsActionEditor::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_DPICHANGED) {
    const UINT new_dpi = HIWORD(wp);
    const auto* suggested = reinterpret_cast<const RECT*>(lp);
    if (suggested)
      ::SetWindowPos(window_, nullptr, suggested->left, suggested->top,
                     suggested->right - suggested->left, suggested->bottom - suggested->top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
    rescale_children(dpi_, new_dpi);
    dpi_ = new_dpi;
    return 0;
  }
  if (message == WM_CLOSE) {
    finish(false);
    return 0;
  }
  if (message == WM_DESTROY) {
    window_ = nullptr;
    finished_ = true;
    return 0;
  }
  if (message != WM_COMMAND) return ::DefWindowProcW(window_, message, wp, lp);
  const int raw = LOWORD(wp);
  const int command = raw == IDOK ? save_action_id : raw == IDCANCEL ? cancel_action_id : raw;
  if (command == type_id && HIWORD(wp) == CBN_SELCHANGE) refresh(true);
  else if (command == operation_id && HIWORD(wp) == CBN_SELCHANGE) refresh(false);
  else if (command == position_id && HIWORD(wp) == CBN_SELCHANGE) refresh(false);
  else if (command == browse_file_id) browse_executable();
  else if (command == browse_directory_id) browse_directory();
  else if (command == validate_lua_id) validate_lua();
  else if (command == test_lua_id) test_lua();
  else if (command == lua_help_id) show_lua_help();
  else if (command == remove_id) finish(true, true);
  else if (command == cancel_action_id) finish(false);
  else if (command == save_action_id) {
    auto action = read();
    const auto validation = action ? actions::validate(*action)
                                   : actions::ActionValidationResult{false, "invalid_fields",
                                                                     "Complete all required fields."};
    if (!validation.valid) {
      ::MessageBoxW(window_, detail::wide(validation.message).c_str(), L"Invalid Action",
                    MB_OK | MB_ICONERROR);
    } else {
      result_ = {true, std::move(action)};
      if (!global_targets_.empty()) {
        const int target = selection(window_, global_target_id);
        if (target < 0 || static_cast<std::size_t>(target) >= global_targets_.size()) {
          ::MessageBoxW(window_, L"Select a gesture for the action.", L"Invalid Action",
                        MB_OK | MB_ICONERROR);
          return 0;
        }
        result_.gesture_id = global_targets_[static_cast<std::size_t>(target)].id;
      }
      finish(true);
    }
  }
  return 0;
}

void WindowsActionEditor::create_controls() {
  using detail::control;
  using detail::text;
  const auto y = [this](int top) { return top + (global_targets_.empty() ? 0 : 40); };
  if (!global_targets_.empty()) {
    text(window_, global_target_label_id, L"Gesture", 20, 22, 120);
    control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, global_target_id, 150, 18, 360, 220);
    for (const auto& target : global_targets_) {
      const auto name = detail::wide(target.name);
      ::SendDlgItemMessageW(window_, global_target_id, CB_ADDSTRING, 0,
                            reinterpret_cast<LPARAM>(name.c_str()));
    }
    ::SendDlgItemMessageW(window_, global_target_id, CB_SETCURSEL, preferred_target_, 0);
  }
  text(window_, 0, L"Action type", 20, y(22), 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, type_id, 150, y(18), 360, 220);
  reset_combo(window_, type_id,
              {L"Keyboard Shortcut", L"Launch Program", L"Open URL / URI", L"Mouse", L"Window",
               L"Media", L"Volume", L"Virtual Desktop", L"Lua Script"});
  text(window_, operation_label_id, L"Operation", 20, y(62), 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, operation_id, 150, y(58), 360, 220);
  text(window_, option_label_id, L"Button / target", 20, y(102), 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, option_id, 150, y(98), 360, 180);
  text(window_, position_label_id, L"Position", 20, y(142), 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, position_id, 150, y(138), 360, 180);
  text(window_, first_label_id, L"Value", 20, y(182), 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, first_id, 150, y(178), 290, 24);
  control(window_, L"BUTTON", L"Browse...", BS_PUSHBUTTON, browse_file_id, 444, y(178), 66, 24);
  text(window_, second_label_id, L"Value", 20, y(222), 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, second_id, 150, y(218), 360, 24);
  text(window_, third_label_id, L"Value", 20, y(262), 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, third_id, 150, y(258), 290, 24);
  control(window_, L"BUTTON", L"Browse...", BS_PUSHBUTTON, browse_directory_id, 444, y(258), 66, 24);
  text(window_, fourth_label_id, L"Value", 20, y(302), 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, fourth_id, 150, y(298), 360, 24);
  text(window_, lua_script_label_id, L"Lua script", 20, y(62), 120);
  control(window_, L"BUTTON", L"API Help", BS_PUSHBUTTON, lua_help_id, 420, y(54), 90, 26);
  (void)::LoadLibraryW(L"Msftedit.dll");
  control(window_, MSFTEDIT_CLASS, L"",
          ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
          lua_script_id, 20, y(88), 490, 242);
  control(window_, L"BUTTON", L"Remove mapping", BS_PUSHBUTTON, remove_id, 20, y(350), 120, 28);
  if (!global_targets_.empty()) show(window_, remove_id, false);
  control(window_, L"BUTTON", L"Validate", BS_PUSHBUTTON, validate_lua_id, 150, y(350), 86, 28);
  control(window_, L"BUTTON", L"Test", BS_PUSHBUTTON, test_lua_id, 244, y(350), 72, 28);
  control(window_, L"BUTTON", L"Save", BS_DEFPUSHBUTTON, save_action_id, 350, y(350), 76, 28);
  control(window_, L"BUTTON", L"Cancel", BS_PUSHBUTTON, cancel_action_id, 434, y(350), 76, 28);
  ::EnumChildWindows(window_, [](HWND child, LPARAM font) -> BOOL {
    ::SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
  }, reinterpret_cast<LPARAM>(::GetStockObject(DEFAULT_GUI_FONT)));
}

void WindowsActionEditor::refresh(bool reset_choices) {
  const int type = selection(window_, type_id);
  if (reset_choices) {
    ::SetDlgItemTextW(window_, first_id, L"");
    ::SetDlgItemTextW(window_, second_id, L"");
    ::SetDlgItemTextW(window_, third_id, L"");
    ::SetDlgItemTextW(window_, fourth_id, L"");
    ::SetDlgItemTextW(window_, lua_script_id, L"");
  }
  bool operation = false, option = false, position = false;
  bool first = false, second = false, third = false, fourth = false;
  bool lua_script = false;
  bool browse_file = false, browse_directory = false;
  if (type == keyboard_type) {
    first = true;
    label(window_, first_label_id, L"Shortcut");
  } else if (type == process_type) {
    operation = first = second = third = browse_file = browse_directory = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Launch"});
    label(window_, first_label_id, L"Executable");
    label(window_, second_label_id, L"Arguments");
    label(window_, third_label_id, L"Working directory");
  } else if (type == url_type) {
    first = true;
    label(window_, first_label_id, L"URL or URI");
  } else if (type == mouse_type) {
    operation = option = position = true;
    if (reset_choices) {
      reset_combo(window_, operation_id, {L"Click", L"Double click", L"Button down", L"Button up", L"Move"});
      reset_combo(window_, option_id, {L"Left", L"Right", L"Middle", L"XButton1", L"XButton2"});
      reset_combo(window_, position_id, {L"Current cursor", L"Gesture start", L"Gesture end", L"Absolute"});
    }
    label(window_, option_label_id, L"Mouse button");
    option = selection(window_, operation_id) != static_cast<int>(actions::MouseOperation::move);
    first = second =
        selection(window_, position_id) == static_cast<int>(actions::PositionTarget::absolute);
    label(window_, first_label_id, L"X coordinate");
    label(window_, second_label_id, L"Y coordinate");
  } else if (type == window_type) {
    operation = option = true;
    if (reset_choices) {
      reset_combo(window_, operation_id, {L"Close", L"Minimize", L"Maximize", L"Restore", L"Activate", L"Move", L"Resize", L"Move and resize", L"Maximize / Restore", L"Center"});
      reset_combo(window_, option_id, {L"Gesture window", L"Foreground window", L"Window at gesture start"});
    }
    label(window_, option_label_id, L"Window target");
    const int selected = selection(window_, operation_id);
    const bool moves = selected == static_cast<int>(actions::WindowOperation::move) ||
                       selected == static_cast<int>(actions::WindowOperation::move_resize);
    const bool resizes = selected == static_cast<int>(actions::WindowOperation::resize) ||
                         selected == static_cast<int>(actions::WindowOperation::move_resize);
    first = second = moves;
    third = fourth = resizes;
    label(window_, first_label_id, L"X coordinate");
    label(window_, second_label_id, L"Y coordinate");
    label(window_, third_label_id, L"Width");
    label(window_, fourth_label_id, L"Height");
  } else if (type == media_type) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Play / Pause", L"Next track", L"Previous track", L"Stop"});
  } else if (type == volume_type) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Increase", L"Decrease", L"Mute toggle"});
    first = selection(window_, operation_id) !=
            static_cast<int>(actions::VolumeOperation::mute_toggle);
    label(window_, first_label_id, L"Amount (optional %)");
  } else if (type == desktop_type) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Next desktop", L"Previous desktop", L"Create desktop", L"Close desktop"});
  } else if (type == lua_type) {
    lua_script = true;
  }
  show(window_, operation_label_id, operation); show(window_, operation_id, operation);
  show(window_, option_label_id, option); show(window_, option_id, option);
  show(window_, position_label_id, position); show(window_, position_id, position);
  show(window_, first_label_id, first); show(window_, first_id, first);
  show(window_, second_label_id, second); show(window_, second_id, second);
  show(window_, third_label_id, third); show(window_, third_id, third);
  show(window_, fourth_label_id, fourth); show(window_, fourth_id, fourth);
  show(window_, lua_script_label_id, lua_script);
  show(window_, lua_script_id, lua_script);
  show(window_, validate_lua_id, lua_script);
  show(window_, test_lua_id, lua_script);
  show(window_, lua_help_id, lua_script);
  show(window_, browse_file_id, browse_file); show(window_, browse_directory_id, browse_directory);
}

void WindowsActionEditor::load(const actions::ActionDefinition* existing) {
  const int type = existing ? static_cast<int>(existing->type) : 0;
  ::SendDlgItemMessageW(window_, type_id, CB_SETCURSEL, type, 0);
  refresh(true);
  if (!existing) return;
  if (const auto* keyboard = std::get_if<actions::KeyboardParameters>(&existing->parameters)) {
    ::SetDlgItemTextW(window_, first_id, detail::wide(keyboard->shortcut).c_str());
  } else if (const auto* process = std::get_if<actions::ProcessParameters>(&existing->parameters)) {
    ::SetDlgItemTextW(window_, first_id, detail::wide(process->path).c_str());
    ::SetDlgItemTextW(window_, second_id, detail::wide(process->arguments).c_str());
    ::SetDlgItemTextW(window_, third_id, detail::wide(process->working_directory).c_str());
  } else if (const auto* url = std::get_if<actions::UrlParameters>(&existing->parameters)) {
    ::SetDlgItemTextW(window_, first_id, detail::wide(url->uri).c_str());
  } else if (const auto* mouse = std::get_if<actions::MouseParameters>(&existing->parameters)) {
    ::SendDlgItemMessageW(window_, operation_id, CB_SETCURSEL,
                          static_cast<int>(mouse->operation), 0);
    if (mouse->button)
      ::SendDlgItemMessageW(window_, option_id, CB_SETCURSEL, static_cast<int>(*mouse->button), 0);
    ::SendDlgItemMessageW(window_, position_id, CB_SETCURSEL,
                          static_cast<int>(mouse->position.target), 0);
    if (mouse->position.absolute) {
      ::SetDlgItemTextW(window_, first_id, detail::number(mouse->position.absolute->x).c_str());
      ::SetDlgItemTextW(window_, second_id, detail::number(mouse->position.absolute->y).c_str());
    }
  } else if (const auto* window = std::get_if<actions::WindowParameters>(&existing->parameters)) {
    ::SendDlgItemMessageW(window_, operation_id, CB_SETCURSEL,
                          static_cast<int>(window->operation), 0);
    ::SendDlgItemMessageW(window_, option_id, CB_SETCURSEL, static_cast<int>(window->target), 0);
    if (window->x) ::SetDlgItemTextW(window_, first_id, std::to_wstring(*window->x).c_str());
    if (window->y) ::SetDlgItemTextW(window_, second_id, std::to_wstring(*window->y).c_str());
    if (window->width) ::SetDlgItemTextW(window_, third_id, std::to_wstring(*window->width).c_str());
    if (window->height) ::SetDlgItemTextW(window_, fourth_id, std::to_wstring(*window->height).c_str());
  } else if (const auto* media = std::get_if<actions::MediaParameters>(&existing->parameters)) {
    ::SendDlgItemMessageW(window_, operation_id, CB_SETCURSEL,
                          static_cast<int>(media->operation), 0);
  } else if (const auto* volume = std::get_if<actions::VolumeParameters>(&existing->parameters)) {
    ::SendDlgItemMessageW(window_, operation_id, CB_SETCURSEL,
                          static_cast<int>(volume->operation), 0);
    if (volume->amount)
      ::SetDlgItemTextW(window_, first_id, detail::number(*volume->amount).c_str());
  } else if (const auto* desktop =
                 std::get_if<actions::VirtualDesktopParameters>(&existing->parameters)) {
    ::SendDlgItemMessageW(window_, operation_id, CB_SETCURSEL,
                          static_cast<int>(desktop->operation), 0);
  } else if (const auto* lua = std::get_if<actions::LuaParameters>(&existing->parameters)) {
    ::SetDlgItemTextW(window_, lua_script_id, detail::wide(lua->script).c_str());
  }
  refresh(false);
}

std::optional<actions::ActionDefinition> WindowsActionEditor::read() const {
  const int type = selection(window_, type_id);
  const int operation = selection(window_, operation_id);
  const int option = selection(window_, option_id);
  const int position = selection(window_, position_id);
  if (type == keyboard_type)
    return actions::ActionDefinition::keyboard(detail::read_utf8(window_, first_id));
  if (type == process_type)
    return actions::ActionDefinition{
        1, actions::ActionType::process,
        actions::ProcessParameters{actions::ProcessOperation::launch,
                                   detail::read_utf8(window_, first_id),
                                   detail::read_utf8(window_, second_id),
                                   detail::read_utf8(window_, third_id)}};
  if (type == url_type)
    return actions::ActionDefinition{1, actions::ActionType::url,
                                     actions::UrlParameters{detail::read_utf8(window_, first_id)}};
  if (type == mouse_type && operation >= 0 && position >= 0) {
    actions::PositionDefinition target{static_cast<actions::PositionTarget>(position), std::nullopt};
    if (target.target == actions::PositionTarget::absolute) {
      const auto x = number(window_, first_id), y = number(window_, second_id);
      if (!x || !y) return std::nullopt;
      target.absolute = gestures::Point{*x, *y};
    }
    std::optional<actions::MouseButton> button;
    if (operation != static_cast<int>(actions::MouseOperation::move)) {
      if (option < 0) return std::nullopt;
      button = static_cast<actions::MouseButton>(option);
    }
    return actions::ActionDefinition{
        1, actions::ActionType::mouse,
        actions::MouseParameters{static_cast<actions::MouseOperation>(operation), button, target}};
  }
  if (type == window_type && operation >= 0 && option >= 0) {
    actions::WindowParameters parameters{static_cast<actions::WindowOperation>(operation),
                                         static_cast<actions::WindowTarget>(option)};
    if (operation == static_cast<int>(actions::WindowOperation::move) ||
        operation == static_cast<int>(actions::WindowOperation::move_resize)) {
      parameters.x = integer(window_, first_id);
      parameters.y = integer(window_, second_id);
      if (!parameters.x || !parameters.y) return std::nullopt;
    }
    if (operation == static_cast<int>(actions::WindowOperation::resize) ||
        operation == static_cast<int>(actions::WindowOperation::move_resize)) {
      parameters.width = integer(window_, third_id);
      parameters.height = integer(window_, fourth_id);
      if (!parameters.width || !parameters.height) return std::nullopt;
    }
    return actions::ActionDefinition{1, actions::ActionType::window, parameters};
  }
  if (type == media_type && operation >= 0)
    return actions::ActionDefinition{1, actions::ActionType::media,
                                     actions::MediaParameters{
                                         static_cast<actions::MediaOperation>(operation)}};
  if (type == volume_type && operation >= 0) {
    std::optional<double> amount;
    if (operation != static_cast<int>(actions::VolumeOperation::mute_toggle)) {
      const auto text = detail::read_utf8(window_, first_id);
      if (!text.empty()) {
        amount = number(window_, first_id);
        if (!amount) return std::nullopt;
      }
    }
    return actions::ActionDefinition{
        1, actions::ActionType::volume,
        actions::VolumeParameters{static_cast<actions::VolumeOperation>(operation), amount}};
  }
  if (type == desktop_type && operation >= 0)
    return actions::ActionDefinition{
        1, actions::ActionType::virtual_desktop,
        actions::VirtualDesktopParameters{
            static_cast<actions::VirtualDesktopOperation>(operation)}};
  if (type == lua_type)
    return actions::ActionDefinition::lua(detail::read_utf8(window_, lua_script_id));
  return std::nullopt;
}

void WindowsActionEditor::browse_executable() {
  std::array<wchar_t, 32768> path{};
  OPENFILENAMEW dialog{sizeof(dialog)};
  dialog.hwndOwner = window_;
  dialog.lpstrFilter = L"Programs (*.exe)\0*.exe\0All files\0*.*\0\0";
  dialog.lpstrFile = path.data();
  dialog.nMaxFile = static_cast<DWORD>(path.size());
  dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
  if (::GetOpenFileNameW(&dialog)) ::SetDlgItemTextW(window_, first_id, path.data());
}

void WindowsActionEditor::browse_directory() {
  BROWSEINFOW dialog{};
  dialog.hwndOwner = window_;
  dialog.lpszTitle = L"Select working directory";
  dialog.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  if (PIDLIST_ABSOLUTE item = ::SHBrowseForFolderW(&dialog)) {
    std::array<wchar_t, MAX_PATH> path{};
    if (::SHGetPathFromIDListW(item, path.data())) ::SetDlgItemTextW(window_, third_id, path.data());
    ::CoTaskMemFree(item);
  }
}

void WindowsActionEditor::validate_lua() {
  actions::LuaRuntime runtime;
  const auto result = runtime.validate_script(detail::read_utf8(window_, lua_script_id));
  const auto message =
      detail::wide(result.success ? "Lua syntax is valid." : result.message);
  ::MessageBoxW(window_, message.c_str(),
                result.success ? L"Valid Lua Script" : L"Invalid Lua Script",
                MB_OK | (result.success ? MB_ICONINFORMATION : MB_ICONERROR));
}

void WindowsActionEditor::test_lua() {
  actions::LuaRuntime runtime;
  if (lua_environment != nullptr) {
    runtime.set_services(lua_environment->services);
    if (!lua_environment->module_directory.empty())
      runtime.set_module_directory(lua_environment->module_directory);
    if (const auto initialized = runtime.initialize(lua_environment->initialization_script);
        !initialized.success) {
      ::MessageBoxW(
          window_,
          detail::wide("The shared initialization script failed: " + initialized.message).c_str(),
          L"Lua Test Failed", MB_OK | MB_ICONERROR);
      return;
    }
  }
  // A tested script was not produced by a gesture, so it runs against the real automation
  // services with no gesture and no application context.
  actions::ActionContext context;
  context.captured = false;
  const auto result = runtime.execute(detail::read_utf8(window_, lua_script_id), context);
  ::MessageBoxW(window_,
                detail::wide(result.success ? "Lua script completed successfully."
                                            : result.message)
                    .c_str(),
                result.success ? L"Lua Test Succeeded" : L"Lua Test Failed",
                MB_OK | (result.success ? MB_ICONINFORMATION : MB_ICONERROR));
}

void WindowsActionEditor::show_lua_help() {
  constexpr wchar_t help_class[] = L"StrokesPlusPlusApiHelp";
  WNDCLASSEXW description{sizeof(description)};
  description.lpfnWndProc = help_proc;
  description.hInstance = instance_;
  description.lpszClassName = help_class;
  description.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  description.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&description);
  if (registered == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
  const auto scale = [this](int value) { return ::MulDiv(value, static_cast<int>(dpi_), 96); };
  bool finished = false;
  HWND help = ::CreateWindowExW(WS_EX_DLGMODALFRAME, help_class, L"Lua API Help",
                                WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
                                scale(640), scale(540), window_, nullptr, instance_, &finished);
  if (help == nullptr) {
    if (registered != 0) ::UnregisterClassW(help_class, instance_);
    return;
  }
  const auto text = documentation();
  (void)::CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", text.c_str(),
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
                              ES_READONLY | ES_AUTOVSCROLL,
                          scale(12), scale(12), scale(600), scale(440), help,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(help_text_id)), instance_,
                          nullptr);
  (void)::CreateWindowExW(0, L"BUTTON", L"Close",
                          WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON, scale(524),
                          scale(464), scale(88), scale(28), help,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(help_close_id)), instance_,
                          nullptr);
  ::EnumChildWindows(help, [](HWND child, LPARAM font) -> BOOL {
    ::SendMessageW(child, WM_SETFONT, static_cast<WPARAM>(font), TRUE);
    return TRUE;
  }, reinterpret_cast<LPARAM>(::GetStockObject(DEFAULT_GUI_FONT)));
  ::EnableWindow(window_, FALSE);
  ::ShowWindow(help, SW_SHOW);
  ::SetFocus(::GetDlgItem(help, help_text_id));
  MSG message{};
  while (!finished && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!::IsDialogMessageW(help, &message)) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
  }
  ::EnableWindow(window_, TRUE);
  ::SetForegroundWindow(window_);
  if (registered != 0) ::UnregisterClassW(help_class, instance_);
}

void WindowsActionEditor::rescale_children(UINT old_dpi, UINT new_dpi) noexcept {
  if (old_dpi == 0 || new_dpi == 0 || old_dpi == new_dpi) return;
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
    ::SetWindowPos(child, nullptr,
                   ::MulDiv(corners[0].x, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
                   ::MulDiv(corners[0].y, static_cast<int>(new_dpi), static_cast<int>(old_dpi)),
                   ::MulDiv(corners[1].x - corners[0].x, static_cast<int>(new_dpi),
                            static_cast<int>(old_dpi)),
                   height,
                   SWP_NOACTIVATE | SWP_NOZORDER);
  }
}

void WindowsActionEditor::finish(bool accepted, bool remove) {
  if (remove) result_ = {true, std::nullopt};
  if (!accepted) result_ = {};
  finished_ = true;
  if (window_) ::DestroyWindow(window_);
}

}  // namespace strokes::ui
