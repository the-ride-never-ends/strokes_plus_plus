#include "actions/windows_user_feedback_service.h"

#include <string>
#include <utility>

#include "ui/settings_controls.h"

namespace strokes::actions {
namespace {

constexpr wchar_t class_name[] = L"StrokesPlusPlusFeedback";
constexpr UINT show_message = WM_APP + 1;
constexpr UINT_PTR hide_timer = 1;
constexpr UINT message_duration = 4000;
constexpr UINT osd_duration = 1500;
constexpr int horizontal_padding = 20;
constexpr int vertical_padding = 14;
constexpr COLORREF background_color = RGB(24, 24, 24);
constexpr COLORREF text_color = RGB(245, 245, 245);
constexpr BYTE window_opacity = 230;

std::wstring* stored_text(HWND window) {
  return reinterpret_cast<std::wstring*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
}

RECT work_area() {
  POINT cursor{};
  MONITORINFO information{sizeof(information)};
  if (::GetCursorPos(&cursor) &&
      ::GetMonitorInfoW(::MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY), &information))
    return information.rcWork;
  RECT primary{};
  (void)::SystemParametersInfoW(SPI_GETWORKAREA, 0, &primary, 0);
  return primary;
}

void paint(HWND window) {
  PAINTSTRUCT painting{};
  HDC device = ::BeginPaint(window, &painting);
  RECT client{};
  ::GetClientRect(window, &client);
  HBRUSH background = ::CreateSolidBrush(background_color);
  ::FillRect(device, &client, background);
  ::DeleteObject(background);
  if (const auto* text = stored_text(window)) {
    HGDIOBJ previous = ::SelectObject(device, ::GetStockObject(DEFAULT_GUI_FONT));
    ::SetBkMode(device, TRANSPARENT);
    ::SetTextColor(device, text_color);
    RECT bounds{client.left + horizontal_padding, client.top + vertical_padding,
                client.right - horizontal_padding, client.bottom - vertical_padding};
    ::DrawTextW(device, text->c_str(), static_cast<int>(text->size()), &bounds,
                DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
    ::SelectObject(device, previous);
  }
  ::EndPaint(window, &painting);
}

void display(HWND window, UINT duration) {
  const auto* text = stored_text(window);
  if (!text) return;
  const RECT area = work_area();
  HDC device = ::GetDC(window);
  HGDIOBJ previous = ::SelectObject(device, ::GetStockObject(DEFAULT_GUI_FONT));
  RECT measured{0, 0, (area.right - area.left) / 2, 0};
  ::DrawTextW(device, text->c_str(), static_cast<int>(text->size()), &measured,
              DT_CALCRECT | DT_CENTER | DT_WORDBREAK | DT_NOPREFIX);
  ::SelectObject(device, previous);
  ::ReleaseDC(window, device);
  const int width = measured.right - measured.left + horizontal_padding * 2;
  const int height = measured.bottom - measured.top + vertical_padding * 2;
  const int left = area.left + ((area.right - area.left) - width) / 2;
  const int top = area.bottom - height - (area.bottom - area.top) / 8;
  ::SetWindowPos(window, HWND_TOPMOST, left, top, width, height, SWP_NOACTIVATE);
  ::ShowWindow(window, SW_SHOWNOACTIVATE);
  ::InvalidateRect(window, nullptr, TRUE);
  (void)::SetTimer(window, hide_timer, duration, nullptr);
}

LRESULT CALLBACK feedback_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  if (message == show_message) {
    delete stored_text(window);
    ::SetWindowLongPtrW(window, GWLP_USERDATA, static_cast<LONG_PTR>(lp));
    display(window, static_cast<UINT>(wp));
    return 0;
  }
  if (message == WM_TIMER && wp == hide_timer) {
    ::KillTimer(window, hide_timer);
    ::ShowWindow(window, SW_HIDE);
    return 0;
  }
  if (message == WM_PAINT) {
    paint(window);
    return 0;
  }
  if (message == WM_DESTROY) {
    ::KillTimer(window, hide_timer);
    delete stored_text(window);
    ::SetWindowLongPtrW(window, GWLP_USERDATA, 0);
    ::PostQuitMessage(0);
    return 0;
  }
  return ::DefWindowProcW(window, message, wp, lp);
}

}  // namespace

WindowsUserFeedbackService::WindowsUserFeedbackService() {
  std::promise<void> ready;
  auto started = ready.get_future();
  worker_ = std::thread([this, &ready] { run(ready); });
  started.wait();
}

WindowsUserFeedbackService::~WindowsUserFeedbackService() {
  if (HWND window = window_.load(std::memory_order_acquire))
    (void)::PostMessageW(window, WM_CLOSE, 0, 0);
  if (worker_.joinable()) worker_.join();
}

void WindowsUserFeedbackService::run(std::promise<void>& ready) noexcept {
  const HINSTANCE instance = ::GetModuleHandleW(nullptr);
  WNDCLASSEXW description{sizeof(description)};
  description.lpfnWndProc = feedback_proc;
  description.hInstance = instance;
  description.lpszClassName = class_name;
  const ATOM registered = ::RegisterClassExW(&description);
  if (registered != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
    HWND window = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        class_name, L"", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr, instance, nullptr);
    if (window != nullptr) {
      (void)::SetLayeredWindowAttributes(window, 0, window_opacity, LWA_ALPHA);
      window_.store(window, std::memory_order_release);
    }
  }
  ready.set_value();
  if (window_.load(std::memory_order_acquire) == nullptr) {
    if (registered != 0) ::UnregisterClassW(class_name, instance);
    return;
  }
  MSG message{};
  while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&message);
    ::DispatchMessageW(&message);
  }
  window_.store(nullptr, std::memory_order_release);
  if (registered != 0) ::UnregisterClassW(class_name, instance);
}

ActionResult WindowsUserFeedbackService::show(std::string_view text, UINT duration) {
  if (text.empty())
    return ActionResult::failed(ActionError::invalid_definition, "empty_message",
                                "The message text must not be empty.");
  HWND window = window_.load(std::memory_order_acquire);
  if (window == nullptr)
    return ActionResult::failed(ActionError::unsupported_operation, "feedback_unavailable",
                                "The on-screen message window is unavailable.");
  auto* payload = new std::wstring(ui::detail::wide(text));
  if (!::PostMessageW(window, show_message, duration, reinterpret_cast<LPARAM>(payload))) {
    delete payload;
    return ActionResult::failed(ActionError::platform_failure, "feedback_post_failed",
                                "Windows did not accept the on-screen message.");
  }
  return ActionResult::succeeded();
}

ActionResult WindowsUserFeedbackService::message(std::string_view text) {
  return show(text, message_duration);
}

ActionResult WindowsUserFeedbackService::osd(std::string_view text) {
  return show(text, osd_duration);
}

}  // namespace strokes::actions
