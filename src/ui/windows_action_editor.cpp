#include "ui/windows_action_editor.h"

#include <commdlg.h>
#include <shlobj.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <utility>

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
};

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
  window_ = ::CreateWindowExW(WS_EX_DLGMODALFRAME, class_name, L"Configure Action",
                              WS_CAPTION | WS_SYSMENU | WS_POPUP, CW_USEDEFAULT, CW_USEDEFAULT,
                              ::MulDiv(570, static_cast<int>(dpi_), 96),
                              ::MulDiv(430, static_cast<int>(dpi_), 96), owner, nullptr, instance,
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
  ::SetFocus(::GetDlgItem(window_, type_id));
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
      finish(true);
    }
  }
  return 0;
}

void WindowsActionEditor::create_controls() {
  using detail::control;
  using detail::text;
  text(window_, 0, L"Action type", 20, 22, 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, type_id, 150, 18, 360, 220);
  reset_combo(window_, type_id,
              {L"Keyboard Shortcut", L"Launch Program", L"Open URL / URI", L"Mouse", L"Window",
               L"Media", L"Volume", L"Virtual Desktop"});
  text(window_, operation_label_id, L"Operation", 20, 62, 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, operation_id, 150, 58, 360, 220);
  text(window_, option_label_id, L"Button / target", 20, 102, 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, option_id, 150, 98, 360, 180);
  text(window_, position_label_id, L"Position", 20, 142, 120);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, position_id, 150, 138, 360, 180);
  text(window_, first_label_id, L"Value", 20, 182, 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, first_id, 150, 178, 290, 24);
  control(window_, L"BUTTON", L"Browse...", BS_PUSHBUTTON, browse_file_id, 444, 178, 66, 24);
  text(window_, second_label_id, L"Value", 20, 222, 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, second_id, 150, 218, 360, 24);
  text(window_, third_label_id, L"Value", 20, 262, 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, third_id, 150, 258, 290, 24);
  control(window_, L"BUTTON", L"Browse...", BS_PUSHBUTTON, browse_directory_id, 444, 258, 66, 24);
  text(window_, fourth_label_id, L"Value", 20, 302, 120);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, fourth_id, 150, 298, 360, 24);
  control(window_, L"BUTTON", L"Remove mapping", BS_PUSHBUTTON, remove_id, 20, 350, 120, 28);
  control(window_, L"BUTTON", L"Save", BS_DEFPUSHBUTTON, save_action_id, 350, 350, 76, 28);
  control(window_, L"BUTTON", L"Cancel", BS_PUSHBUTTON, cancel_action_id, 434, 350, 76, 28);
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
  }
  bool operation = false, option = false, position = false;
  bool first = false, second = false, third = false, fourth = false;
  bool browse_file = false, browse_directory = false;
  if (type == 0) {
    first = true;
    label(window_, first_label_id, L"Shortcut");
  } else if (type == 1) {
    operation = first = second = third = browse_file = browse_directory = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Launch"});
    label(window_, first_label_id, L"Executable");
    label(window_, second_label_id, L"Arguments");
    label(window_, third_label_id, L"Working directory");
  } else if (type == 2) {
    first = true;
    label(window_, first_label_id, L"URL or URI");
  } else if (type == 3) {
    operation = option = position = true;
    if (reset_choices) {
      reset_combo(window_, operation_id, {L"Click", L"Double click", L"Button down", L"Button up", L"Move"});
      reset_combo(window_, option_id, {L"Left", L"Right", L"Middle", L"XButton1", L"XButton2"});
      reset_combo(window_, position_id, {L"Current cursor", L"Gesture start", L"Gesture end", L"Absolute"});
    }
    label(window_, option_label_id, L"Mouse button");
    option = selection(window_, operation_id) != 4;
    first = second = selection(window_, position_id) == 3;
    label(window_, first_label_id, L"X coordinate");
    label(window_, second_label_id, L"Y coordinate");
  } else if (type == 4) {
    operation = option = true;
    if (reset_choices) {
      reset_combo(window_, operation_id, {L"Close", L"Minimize", L"Maximize", L"Restore", L"Activate", L"Move", L"Resize", L"Move and resize", L"Maximize / Restore", L"Center"});
      reset_combo(window_, option_id, {L"Gesture window", L"Foreground window", L"Window at gesture start"});
    }
    label(window_, option_label_id, L"Window target");
    const int selected = selection(window_, operation_id);
    first = second = selected == 5 || selected == 7;
    third = fourth = selected == 6 || selected == 7;
    label(window_, first_label_id, L"X coordinate");
    label(window_, second_label_id, L"Y coordinate");
    label(window_, third_label_id, L"Width");
    label(window_, fourth_label_id, L"Height");
  } else if (type == 5) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Play / Pause", L"Next track", L"Previous track", L"Stop"});
  } else if (type == 6) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Increase", L"Decrease", L"Mute toggle"});
    first = selection(window_, operation_id) != 2;
    label(window_, first_label_id, L"Amount (optional %)");
  } else if (type == 7) {
    operation = true;
    if (reset_choices) reset_combo(window_, operation_id, {L"Next desktop", L"Previous desktop", L"Create desktop", L"Close desktop"});
  }
  show(window_, operation_label_id, operation); show(window_, operation_id, operation);
  show(window_, option_label_id, option); show(window_, option_id, option);
  show(window_, position_label_id, position); show(window_, position_id, position);
  show(window_, first_label_id, first); show(window_, first_id, first);
  show(window_, second_label_id, second); show(window_, second_id, second);
  show(window_, third_label_id, third); show(window_, third_id, third);
  show(window_, fourth_label_id, fourth); show(window_, fourth_id, fourth);
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
  }
  refresh(false);
}

std::optional<actions::ActionDefinition> WindowsActionEditor::read() const {
  const int type = selection(window_, type_id);
  const int operation = selection(window_, operation_id);
  const int option = selection(window_, option_id);
  const int position = selection(window_, position_id);
  if (type == 0)
    return actions::ActionDefinition::keyboard(detail::read_utf8(window_, first_id));
  if (type == 1)
    return actions::ActionDefinition{
        1, actions::ActionType::process,
        actions::ProcessParameters{actions::ProcessOperation::launch,
                                   detail::read_utf8(window_, first_id),
                                   detail::read_utf8(window_, second_id),
                                   detail::read_utf8(window_, third_id)}};
  if (type == 2)
    return actions::ActionDefinition{1, actions::ActionType::url,
                                     actions::UrlParameters{detail::read_utf8(window_, first_id)}};
  if (type == 3 && operation >= 0 && position >= 0) {
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
  if (type == 4 && operation >= 0 && option >= 0) {
    actions::WindowParameters parameters{static_cast<actions::WindowOperation>(operation),
                                         static_cast<actions::WindowTarget>(option)};
    if (operation == 5 || operation == 7) {
      parameters.x = integer(window_, first_id);
      parameters.y = integer(window_, second_id);
      if (!parameters.x || !parameters.y) return std::nullopt;
    }
    if (operation == 6 || operation == 7) {
      parameters.width = integer(window_, third_id);
      parameters.height = integer(window_, fourth_id);
      if (!parameters.width || !parameters.height) return std::nullopt;
    }
    return actions::ActionDefinition{1, actions::ActionType::window, parameters};
  }
  if (type == 5 && operation >= 0)
    return actions::ActionDefinition{1, actions::ActionType::media,
                                     actions::MediaParameters{
                                         static_cast<actions::MediaOperation>(operation)}};
  if (type == 6 && operation >= 0) {
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
  if (type == 7 && operation >= 0)
    return actions::ActionDefinition{
        1, actions::ActionType::virtual_desktop,
        actions::VirtualDesktopParameters{
            static_cast<actions::VirtualDesktopOperation>(operation)}};
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
