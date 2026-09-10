#include "tray/windows_tray_icon.h"

#include <shellapi.h>

#include <cstdint>

namespace strokes::tray {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusTrayWindow";
constexpr UINT callback_message = WM_APP + 1;
constexpr UINT icon_id = 1;
constexpr UINT retry_timer_id = 1;
constexpr UINT enable_id = 1001, disable_id = 1002, settings_id = 1003, exit_id = 1004;

HICON create_status_icon(bool enabled) noexcept {
  constexpr int size = 32;
  BITMAPV5HEADER header{};
  header.bV5Size = sizeof(header);
  header.bV5Width = size;
  header.bV5Height = -size;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;
  void* bits = nullptr;
  HDC screen = ::GetDC(nullptr);
  HBITMAP color = ::CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header), DIB_RGB_COLORS,
                                     &bits, nullptr, 0);
  if (screen) ::ReleaseDC(nullptr, screen);
  if (!color || !bits) return nullptr;
  auto* pixels = static_cast<std::uint32_t*>(bits);
  const std::uint32_t fill = enabled ? 0xFF168B4B : 0xFF777777;
  const std::uint32_t accent = enabled ? 0xFFFFFFFF : 0xFFDDDDDD;
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const int dx = x - 15, dy = y - 15;
      if (dx * dx + dy * dy <= 14 * 14) pixels[y * size + x] = fill;
    }
  }
  // A compact, recognizable gesture stroke: down, right, then up.
  for (int i = 0; i < 3; ++i) {
    for (int y = 7; y <= 23; ++y) pixels[y * size + 9 + i] = accent;
    for (int x = 9; x <= 22; ++x) pixels[(23 - i) * size + x] = accent;
    for (int y = 15; y <= 23; ++y) pixels[y * size + 20 + i] = accent;
  }
  HBITMAP mask = ::CreateBitmap(size, size, 1, 1, nullptr);
  ICONINFO info{};
  info.fIcon = TRUE;
  info.hbmColor = color;
  info.hbmMask = mask;
  HICON icon = mask ? ::CreateIconIndirect(&info) : nullptr;
  if (mask) ::DeleteObject(mask);
  ::DeleteObject(color);
  return icon;
}
}  // namespace

WindowsTrayIcon::~WindowsTrayIcon() { destroy(); }

bool WindowsTrayIcon::create(HINSTANCE instance, Handler handler, void* context) {
  if (window_ != nullptr || instance == nullptr || handler == nullptr) return false;
  instance_ = instance;
  handler_ = handler;
  context_ = context;
  enabled_icon_ = create_status_icon(true);
  disabled_icon_ = create_status_icon(false);
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
  remove_icon();
  if (window_ != nullptr) {
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
  if (class_registered_ && instance_ != nullptr) ::UnregisterClassW(class_name, instance_);
  class_registered_ = false;
  instance_ = nullptr;
  if (enabled_icon_) ::DestroyIcon(enabled_icon_);
  if (disabled_icon_) ::DestroyIcon(disabled_icon_);
  enabled_icon_ = nullptr;
  disabled_icon_ = nullptr;
}

void WindowsTrayIcon::set_enabled(bool enabled) noexcept {
  enabled_ = enabled;
  if (window_ == nullptr) return;
  NOTIFYICONDATAW icon{sizeof(icon)};
  icon.hWnd = window_;
  icon.uID = icon_id;
  icon.uFlags = NIF_ICON | NIF_TIP;
  icon.hIcon = enabled ? enabled_icon_ : disabled_icon_;
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
  if (message == WM_TIMER && wp == retry_timer_id) {
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
        remove_icon();
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
  icon.hIcon = enabled_ ? enabled_icon_ : disabled_icon_;
  ::lstrcpynW(icon.szTip, enabled_ ? L"Strokes++ - Enabled" : L"Strokes++ - Disabled", 128);
  if (::Shell_NotifyIconW(NIM_ADD, &icon)) {
    ::KillTimer(window_, retry_timer_id);
    icon.uVersion = NOTIFYICON_VERSION_4;
    (void)::Shell_NotifyIconW(NIM_SETVERSION, &icon);
  } else
    ::SetTimer(window_, retry_timer_id, 2000, nullptr);
}

void WindowsTrayIcon::remove_icon() noexcept {
  if (window_ == nullptr) return;
  ::KillTimer(window_, retry_timer_id);
  NOTIFYICONDATAW icon{sizeof(icon)};
  icon.hWnd = window_;
  icon.uID = icon_id;
  (void)::Shell_NotifyIconW(NIM_DELETE, &icon);
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
