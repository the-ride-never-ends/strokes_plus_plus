#include "overlay/windows_gesture_overlay.h"

#include <algorithm>

namespace strokes::overlay {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusGestureOverlay";
constexpr UINT refresh_message = WM_APP + 20;
constexpr UINT show_message = WM_APP + 21;
constexpr UINT hide_message = WM_APP + 22;
}  // namespace

WindowsGestureOverlay::~WindowsGestureOverlay() { destroy(); }

bool WindowsGestureOverlay::create(HINSTANCE instance, Options options) {
  if (window_ != nullptr || instance == nullptr || options.line_width <= 0) return false;
  instance_ = instance;
  options_ = options;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));  // OCR_NORMAL
  if (::RegisterClassExW(&wc) == 0) {
    if (::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  } else {
    class_registered_ = true;
  }

  origin_x_ = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
  origin_y_ = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
  width_ = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  height_ = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
  window_ = ::CreateWindowExW(
      WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
      class_name, L"", WS_POPUP, origin_x_, origin_y_, width_, height_, nullptr, nullptr, instance_,
      this);
  if (window_ == nullptr) {
    if (class_registered_) ::UnregisterClassW(class_name, instance_);
    class_registered_ = false;
    return false;
  }
  ::SetLayeredWindowAttributes(window_, RGB(0, 0, 0), options_.opacity, LWA_COLORKEY | LWA_ALPHA);
  create_buffer();
  if (memory_dc_ == nullptr) {
    destroy();
    return false;
  }
  return true;
}

void WindowsGestureOverlay::destroy() noexcept {
  destroy_buffer();
  if (window_ != nullptr) {
    ::DestroyWindow(window_);
    window_ = nullptr;
  }
  if (class_registered_ && instance_ != nullptr) ::UnregisterClassW(class_name, instance_);
  class_registered_ = false;
  instance_ = nullptr;
}

void WindowsGestureOverlay::show(const gestures::Stroke& points) {
  if (!options_.enabled || window_ == nullptr) return;
  update(points);
  ::PostMessageW(window_, show_message, 0, 0);
}

void WindowsGestureOverlay::update(const gestures::Stroke& points) {
  if (!options_.enabled || window_ == nullptr) return;
  {
    std::scoped_lock lock(points_mutex_);
    if (points.size() < points_.size()) {
      points_.clear();
      painted_points_ = 0;
    }
    points_.insert(points_.end(), points.begin() + static_cast<std::ptrdiff_t>(points_.size()),
                   points.end());
  }
  ::PostMessageW(window_, refresh_message, 0, 0);
}

void WindowsGestureOverlay::hide() noexcept {
  if (window_ != nullptr) ::PostMessageW(window_, hide_message, 0, 0);
}

void WindowsGestureOverlay::configure(Options options) noexcept {
  options_ = options;
  if (window_ == nullptr) return;
  ::SetLayeredWindowAttributes(window_, RGB(0, 0, 0), options_.opacity, LWA_COLORKEY | LWA_ALPHA);
  if (!options_.enabled)
    hide();
  else {
    std::scoped_lock lock(points_mutex_);
    painted_points_ = 0;
    clear_buffer();
    ::PostMessageW(window_, refresh_message, 0, 0);
  }
}

LRESULT CALLBACK WindowsGestureOverlay::window_proc(HWND window, UINT message, WPARAM wp,
                                                    LPARAM lp) {
  auto* self = reinterpret_cast<WindowsGestureOverlay*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self =
        static_cast<WindowsGestureOverlay*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsGestureOverlay::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == refresh_message) {
    draw_segments();
    return 0;
  }
  if (message == show_message) {
    ::ShowWindow(window_, SW_SHOWNOACTIVATE);
    ::InvalidateRect(window_, nullptr, FALSE);
    return 0;
  }
  if (message == hide_message) {
    ::ShowWindow(window_, SW_HIDE);
    std::scoped_lock lock(points_mutex_);
    points_.clear();
    painted_points_ = 0;
    clear_buffer();
    return 0;
  }
  if (message == WM_DISPLAYCHANGE) {
    resize_screen();
    return 0;
  }
  if (message == WM_PAINT) {
    paint();
    return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_NCHITTEST) return HTTRANSPARENT;
  if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  return ::DefWindowProcW(window_, message, wp, lp);
}

void WindowsGestureOverlay::paint() noexcept {
  PAINTSTRUCT paint_data{};
  HDC dc = ::BeginPaint(window_, &paint_data);
  if (memory_dc_ != nullptr) {
    const RECT& dirty = paint_data.rcPaint;
    ::BitBlt(dc, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top,
             memory_dc_, dirty.left, dirty.top, SRCCOPY);
  }
  ::EndPaint(window_, &paint_data);
}

void WindowsGestureOverlay::resize_screen() noexcept {
  origin_x_ = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
  origin_y_ = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
  width_ = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
  height_ = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
  ::SetWindowPos(window_, HWND_TOPMOST, origin_x_, origin_y_, width_, height_,
                 SWP_NOACTIVATE | SWP_NOOWNERZORDER);
  destroy_buffer();
  create_buffer();
  {
    std::scoped_lock lock(points_mutex_);
    painted_points_ = 0;
  }
  draw_segments();
  ::InvalidateRect(window_, nullptr, FALSE);
}

void WindowsGestureOverlay::create_buffer() noexcept {
  if (window_ == nullptr || width_ <= 0 || height_ <= 0) return;
  HDC window_dc = ::GetDC(window_);
  if (window_dc == nullptr) return;
  memory_dc_ = ::CreateCompatibleDC(window_dc);
  bitmap_ = ::CreateCompatibleBitmap(window_dc, width_, height_);
  ::ReleaseDC(window_, window_dc);
  if (memory_dc_ == nullptr || bitmap_ == nullptr) {
    destroy_buffer();
    return;
  }
  previous_bitmap_ = ::SelectObject(memory_dc_, bitmap_);
  clear_buffer();
}

void WindowsGestureOverlay::destroy_buffer() noexcept {
  if (memory_dc_ != nullptr && previous_bitmap_ != nullptr) {
    ::SelectObject(memory_dc_, previous_bitmap_);
  }
  previous_bitmap_ = nullptr;
  if (bitmap_ != nullptr) ::DeleteObject(bitmap_);
  bitmap_ = nullptr;
  if (memory_dc_ != nullptr) ::DeleteDC(memory_dc_);
  memory_dc_ = nullptr;
}

void WindowsGestureOverlay::clear_buffer() noexcept {
  if (memory_dc_ == nullptr) return;
  const RECT bounds{0, 0, width_, height_};
  ::FillRect(memory_dc_, &bounds, static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
}

void WindowsGestureOverlay::draw_segments() noexcept {
  if (memory_dc_ == nullptr) return;
  std::scoped_lock lock(points_mutex_);
  if (points_.size() < 2 || painted_points_ >= points_.size()) return;
  const std::size_t first = painted_points_ == 0 ? 1 : painted_points_;
  HPEN pen = ::CreatePen(PS_SOLID, options_.line_width, options_.color);
  if (pen == nullptr) return;
  HGDIOBJ previous = ::SelectObject(memory_dc_, pen);
  RECT dirty{width_, height_, 0, 0};
  for (std::size_t index = first; index < points_.size(); ++index) {
    const int x1 = static_cast<int>(points_[index - 1].x) - origin_x_;
    const int y1 = static_cast<int>(points_[index - 1].y) - origin_y_;
    const int x2 = static_cast<int>(points_[index].x) - origin_x_;
    const int y2 = static_cast<int>(points_[index].y) - origin_y_;
    ::MoveToEx(memory_dc_, x1, y1, nullptr);
    ::LineTo(memory_dc_, x2, y2);
    const int padding = options_.line_width + 1;
    dirty.left = (std::min)(dirty.left, static_cast<LONG>((std::min)(x1, x2) - padding));
    dirty.top = (std::min)(dirty.top, static_cast<LONG>((std::min)(y1, y2) - padding));
    dirty.right = (std::max)(dirty.right, static_cast<LONG>((std::max)(x1, x2) + padding));
    dirty.bottom = (std::max)(dirty.bottom, static_cast<LONG>((std::max)(y1, y2) + padding));
  }
  painted_points_ = points_.size();
  ::SelectObject(memory_dc_, previous);
  ::DeleteObject(pen);
  ::InvalidateRect(window_, &dirty, FALSE);
}
}  // namespace strokes::overlay
