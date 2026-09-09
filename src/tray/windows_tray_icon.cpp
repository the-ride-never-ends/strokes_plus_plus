#include "tray/windows_tray_icon.h"

#include <shellapi.h>

namespace strokes::tray {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusTrayWindow";
constexpr UINT callback_message = WM_APP + 1;
constexpr UINT icon_id = 1;
constexpr UINT enable_id = 1001, disable_id = 1002, settings_id = 1003, exit_id = 1004;

HICON status_icon(bool enabled) noexcept {
  const WORD resource = enabled ? 32512 : 32515;  // OIC_APPLICATION / OIC_WARNING
  return static_cast<HICON>(
      ::LoadImageW(nullptr, MAKEINTRESOURCEW(resource), IMAGE_ICON, 16, 16, LR_SHARED));
}
}  // namespace

WindowsTrayIcon::~WindowsTrayIcon() { destroy(); }

bool WindowsTrayIcon::create(HINSTANCE instance, Handler handler, void* context) {
  if (window_ != nullptr || instance == nullptr || handler == nullptr) return false;
  instance_ = instance;
  handler_ = handler;
  context_ = context;
  taskbar_created_ = ::RegisterWindowMessageW(L"TaskbarCreated");
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  if (::RegisterClassExW(&wc) == 0) {
    if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  } else {
    class_registered_ = true;
  }
  window_ = ::CreateWindowExW(0, class_name, L"Strokes++", 0, 0, 0, 0, 0, nullptr, nullptr,
                              instance_, this);
  if (window_ == nullptr) {
    if (class_registered_) ::UnregisterClassW(class_name, instance_);
    class_registered_ = false;
    return false;
  }
  add_icon();
  return true;
}

void WindowsTrayIcon::destroy() noexcept {
  if (window_ != nullptr) {
    NOTIFYICONDATAW icon{sizeof(icon)};
    icon.hWnd = window_;
    icon.uID = icon_id;
    ::Shell_NotifyIconW(NIM_DELETE, &icon);
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
  if (class_registered_ && instance_ != nullptr) ::UnregisterClassW(class_name, instance_);
  class_registered_ = false;
  instance_ = nullptr;
}

void WindowsTrayIcon::set_enabled(bool enabled) noexcept {
  enabled_ = enabled;
  if (window_ == nullptr) return;
  NOTIFYICONDATAW icon{sizeof(icon)};
  icon.hWnd = window_;
  icon.uID = icon_id;
  icon.uFlags = NIF_ICON | NIF_TIP;
  icon.hIcon = status_icon(enabled);
  ::lstrcpynW(icon.szTip, enabled ? L"Strokes++ - Enabled" : L"Strokes++ - Disabled", 128);
  (void)::Shell_NotifyIconW(NIM_MODIFY, &icon);
}

LRESULT CALLBACK WindowsTrayIcon::window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<WindowsTrayIcon*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<WindowsTrayIcon*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsTrayIcon::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == taskbar_created_) {
    add_icon();
    return 0;
  }
  if (message == WM_POWERBROADCAST) {
    if (wp == PBT_APMSUSPEND)
      handler_(TrayCommand::suspend, context_);
    else if (wp == PBT_APMRESUMEAUTOMATIC || wp == PBT_APMRESUMESUSPEND)
      handler_(TrayCommand::resume, context_);
    return TRUE;
  }
  if (message == callback_message) {
    const UINT event = LOWORD(lp);
    if (event == WM_CONTEXTMENU || event == WM_RBUTTONUP)
      show_menu();
    else if (event == WM_LBUTTONDBLCLK)
      handler_(TrayCommand::settings, context_);
    return 0;
  }
  if (message == WM_COMMAND) {
    switch (LOWORD(wp)) {
      case enable_id:
        handler_(TrayCommand::enable, context_);
        break;
      case disable_id:
        handler_(TrayCommand::disable, context_);
        break;
      case settings_id:
        handler_(TrayCommand::settings, context_);
        break;
      case exit_id:
        handler_(TrayCommand::exit, context_);
        break;
      default:
        break;
    }
    return 0;
  }
  return ::DefWindowProcW(window_, message, wp, lp);
}

void WindowsTrayIcon::add_icon() noexcept {
  NOTIFYICONDATAW icon{sizeof(icon)};
  icon.hWnd = window_;
  icon.uID = icon_id;
  icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  icon.uCallbackMessage = callback_message;
  icon.hIcon = status_icon(enabled_);
  ::lstrcpynW(icon.szTip, enabled_ ? L"Strokes++ - Enabled" : L"Strokes++ - Disabled", 128);
  if (::Shell_NotifyIconW(NIM_ADD, &icon)) {
    icon.uVersion = NOTIFYICON_VERSION_4;
    (void)::Shell_NotifyIconW(NIM_SETVERSION, &icon);
  }
}

void WindowsTrayIcon::show_menu() noexcept {
  HMENU menu = ::CreatePopupMenu();
  if (menu == nullptr) return;
  ::AppendMenuW(menu, MF_STRING | (enabled_ ? MF_GRAYED : 0), enable_id, L"Enable Gestures");
  ::AppendMenuW(menu, MF_STRING | (!enabled_ ? MF_GRAYED : 0), disable_id, L"Disable Gestures");
  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu, MF_STRING, settings_id, L"Settings");
  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu, MF_STRING, exit_id, L"Exit");
  POINT cursor{};
  ::GetCursorPos(&cursor);
  ::SetForegroundWindow(window_);
  ::TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0,
                   window_, nullptr);
  ::PostMessageW(window_, WM_NULL, 0, 0);
  ::DestroyMenu(menu);
}
}  // namespace strokes::tray
