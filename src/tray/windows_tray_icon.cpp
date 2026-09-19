#include "tray/windows_tray_icon.h"

#include <shellapi.h>

#include <cstdint>

#include "../../resources/resource.h"

namespace strokes::tray {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusTrayWindow";
constexpr UINT callback_message = WM_APP + 1;
constexpr UINT icon_id = 1;
constexpr UINT retry_timer_id = 1;
constexpr UINT enable_id = 1001, disable_id = 1002, settings_id = 1003, exit_id = 1004,
               reload_id = 1005;

HICON load_logo(HINSTANCE instance, bool& owned) noexcept {
  HICON icon = static_cast<HICON>(::LoadImageW(
      instance, MAKEINTRESOURCEW(IDI_STROKES_PLUS_PLUS), IMAGE_ICON,
      ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
  owned = icon != nullptr;
  if (icon == nullptr) icon = ::LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
  return icon;
}

HICON make_disabled_icon(HICON source) noexcept {
  const int width = ::GetSystemMetrics(SM_CXSMICON);
  const int height = ::GetSystemMetrics(SM_CYSMICON);
  BITMAPV5HEADER header{};
  header.bV5Size = sizeof(header);
  header.bV5Width = width;
  header.bV5Height = -height;
  header.bV5Planes = 1;
  header.bV5BitCount = 32;
  header.bV5Compression = BI_BITFIELDS;
  header.bV5RedMask = 0x00FF0000;
  header.bV5GreenMask = 0x0000FF00;
  header.bV5BlueMask = 0x000000FF;
  header.bV5AlphaMask = 0xFF000000;
  void* pixels = nullptr;
  HDC screen = ::GetDC(nullptr);
  HBITMAP color = ::CreateDIBSection(screen, reinterpret_cast<BITMAPINFO*>(&header),
                                     DIB_RGB_COLORS, &pixels, nullptr, 0);
  HBITMAP mask = ::CreateBitmap(width, height, 1, 1, nullptr);
  HDC memory = color != nullptr ? ::CreateCompatibleDC(screen) : nullptr;
  HGDIOBJ previous = memory != nullptr ? ::SelectObject(memory, color) : nullptr;
  const bool drawn = memory != nullptr &&
                     ::DrawIconEx(memory, 0, 0, source, width, height, 0, nullptr, DI_NORMAL);
  if (drawn) {
    auto* values = static_cast<std::uint32_t*>(pixels);
    for (int i = 0; i < width * height; ++i) {
      const auto pixel = values[i];
      const auto gray = static_cast<std::uint32_t>(
          (((pixel >> 16) & 0xFF) * 30 + ((pixel >> 8) & 0xFF) * 59 + (pixel & 0xFF) * 11) /
          100);
      values[i] = (pixel & 0xFF000000) | (gray << 16) | (gray << 8) | gray;
    }
  }
  if (previous != nullptr) ::SelectObject(memory, previous);
  if (memory != nullptr) ::DeleteDC(memory);
  ::ReleaseDC(nullptr, screen);
  ICONINFO info{};
  info.fIcon = TRUE;
  info.hbmColor = color;
  info.hbmMask = mask;
  HICON result = drawn ? ::CreateIconIndirect(&info) : nullptr;
  if (color != nullptr) ::DeleteObject(color);
  if (mask != nullptr) ::DeleteObject(mask);
  return result;
}
}  // namespace

WindowsTrayIcon::~WindowsTrayIcon() { destroy(); }

bool WindowsTrayIcon::create(HINSTANCE instance, Handler handler, void* context) {
  if (window_ != nullptr || instance == nullptr || handler == nullptr) return false;
  instance_ = instance;
  handler_ = handler;
  context_ = context;
  logo_icon_ = load_logo(instance_, owns_logo_icon_);
  disabled_icon_ = make_disabled_icon(logo_icon_);
  taskbar_created_ = ::RegisterWindowMessageW(L"TaskbarCreated");
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  wc.hIcon = logo_icon_;
  wc.hIconSm = logo_icon_;
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
  if (owns_logo_icon_ && logo_icon_) ::DestroyIcon(logo_icon_);
  if (disabled_icon_) ::DestroyIcon(disabled_icon_);
  logo_icon_ = nullptr;
  disabled_icon_ = nullptr;
  owns_logo_icon_ = false;
}

void WindowsTrayIcon::set_enabled(bool enabled) noexcept {
  enabled_ = enabled;
  if (window_ == nullptr) return;
  NOTIFYICONDATAW icon{sizeof(icon)};
  icon.hWnd = window_;
  icon.uID = icon_id;
  icon.uFlags = NIF_ICON | NIF_TIP;
  icon.hIcon = enabled_ || disabled_icon_ == nullptr ? logo_icon_ : disabled_icon_;
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

std::optional<TrayClick> WindowsTrayIcon::click_for_callback(UINT event) noexcept {
  if (event == WM_LBUTTONUP) return TrayClick::toggle;
  if (event == WM_RBUTTONUP) return TrayClick::show_menu;
  return std::nullopt;
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
    const auto click = click_for_callback(LOWORD(lp));
    if (click == TrayClick::toggle)
      handler_(TrayCommand::toggle, context_);
    else if (click == TrayClick::show_menu)
      show_menu();
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
      case reload_id:
        handler_(TrayCommand::reload_scripts, context_);
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
  icon.hIcon = enabled_ || disabled_icon_ == nullptr ? logo_icon_ : disabled_icon_;
  ::lstrcpynW(icon.szTip, enabled_ ? L"Strokes++ - Enabled" : L"Strokes++ - Disabled", 128);
  if (::Shell_NotifyIconW(NIM_ADD, &icon)) {
    ::KillTimer(window_, retry_timer_id);
    // Version 3 reports the physical button messages directly. Version 4
    // synthesizes NIN_SELECT/WM_CONTEXTMENU notifications, which makes the
    // intentionally reversed left-menu/right-toggle behavior ambiguous.
    icon.uVersion = NOTIFYICON_VERSION;
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
  ::AppendMenuW(menu, MF_STRING, reload_id, L"Reload Lua Scripts");
  ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  ::AppendMenuW(menu, MF_STRING, exit_id, L"Exit");
  POINT cursor{};
  ::GetCursorPos(&cursor);
  ::SetForegroundWindow(window_);
  const UINT command = ::TrackPopupMenu(
      menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursor.x, cursor.y, 0,
      window_, nullptr);
  ::PostMessageW(window_, WM_NULL, 0, 0);
  ::DestroyMenu(menu);
  switch (command) {
    case enable_id:
      handler_(TrayCommand::enable, context_);
      break;
    case disable_id:
      handler_(TrayCommand::disable, context_);
      break;
    case settings_id:
      handler_(TrayCommand::settings, context_);
      break;
    case reload_id:
      handler_(TrayCommand::reload_scripts, context_);
      break;
    case exit_id:
      remove_icon();
      handler_(TrayCommand::exit, context_);
      break;
    default:
      break;
  }
}
}  // namespace strokes::tray
