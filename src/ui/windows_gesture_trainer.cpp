#include "ui/windows_gesture_trainer.h"

#include <windowsx.h>

#include <cmath>

namespace strokes::ui {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusGestureTrainer";
}

std::optional<gestures::Stroke> WindowsGestureTrainer::capture(HINSTANCE instance,
                                                               double minimum_point_distance) {
  if (!std::isfinite(minimum_point_distance) || minimum_point_distance < 0.0) return {};
  instance_ = instance;
  minimum_point_distance_ = minimum_point_distance;
  points_.clear();
  drawing_ = false;
  finished_ = false;
  accepted_ = false;
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return {};
  window_ = ::CreateWindowExW(WS_EX_APPWINDOW, class_name, L"Draw Gesture - Escape to Cancel",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, CW_USEDEFAULT, CW_USEDEFAULT,
                              600, 460, nullptr, nullptr, instance_, this);
  if (!window_) return {};
  ::SendMessageW(window_, WM_SETFONT, reinterpret_cast<WPARAM>(::GetStockObject(DEFAULT_GUI_FONT)),
                 TRUE);
  ::ShowWindow(window_, SW_SHOW);
  ::UpdateWindow(window_);
  MSG message{};
  while (!finished_ && ::GetMessageW(&message, nullptr, 0, 0) > 0) {
    ::TranslateMessage(&message);
    ::DispatchMessageW(&message);
  }
  if (message.message == WM_QUIT) ::PostQuitMessage(static_cast<int>(message.wParam));
  return accepted_ ? std::optional{std::move(points_)} : std::nullopt;
}

LRESULT CALLBACK WindowsGestureTrainer::window_proc(HWND window, UINT message, WPARAM wp,
                                                    LPARAM lp) {
  auto* self = reinterpret_cast<WindowsGestureTrainer*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self =
        static_cast<WindowsGestureTrainer*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsGestureTrainer::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_DPICHANGED) {
    const auto* bounds = reinterpret_cast<const RECT*>(lp);
    ::SetWindowPos(window_, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
                   bounds->bottom - bounds->top, SWP_NOACTIVATE | SWP_NOZORDER);
    return 0;
  }
  if (message == WM_LBUTTONDOWN) {
    points_.clear();
    drawing_ = true;
    ::SetCapture(window_);
    points_.push_back(
        {static_cast<double>(GET_X_LPARAM(lp)), static_cast<double>(GET_Y_LPARAM(lp))});
    ::InvalidateRect(window_, nullptr, TRUE);
    return 0;
  }
  if (message == WM_MOUSEMOVE && drawing_) {
    gestures::Point p{static_cast<double>(GET_X_LPARAM(lp)), static_cast<double>(GET_Y_LPARAM(lp))};
    if (std::hypot(p.x - points_.back().x, p.y - points_.back().y) >= minimum_point_distance_) {
      points_.push_back(p);
      ::InvalidateRect(window_, nullptr, FALSE);
    }
    return 0;
  }
  if (message == WM_LBUTTONUP && drawing_) {
    drawing_ = false;
    ::ReleaseCapture();
    if (points_.size() >= 2) {
      accepted_ = true;
      ::DestroyWindow(window_);
    }
    return 0;
  }
  if (message == WM_KEYDOWN && wp == VK_ESCAPE) {
    ::DestroyWindow(window_);
    return 0;
  }
  if (message == WM_PAINT) {
    paint();
    return 0;
  }
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

void WindowsGestureTrainer::paint() noexcept {
  PAINTSTRUCT ps{};
  HDC dc = ::BeginPaint(window_, &ps);
  RECT area{};
  ::GetClientRect(window_, &area);
  HGDIOBJ previous_font = ::SelectObject(dc, ::GetStockObject(DEFAULT_GUI_FONT));
  ::FillRect(dc, &area, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
  ::SetBkMode(dc, TRANSPARENT);
  ::DrawTextW(dc, L"Hold the left mouse button and draw one continuous stroke.", -1, &area,
              DT_CENTER | DT_TOP | DT_SINGLELINE);
  if (points_.size() >= 2) {
    HPEN pen = ::CreatePen(PS_SOLID, 4, RGB(0, 120, 215));
    auto old = ::SelectObject(dc, pen);
    ::MoveToEx(dc, static_cast<int>(points_[0].x), static_cast<int>(points_[0].y), nullptr);
    for (auto& p : points_) ::LineTo(dc, static_cast<int>(p.x), static_cast<int>(p.y));
    ::SelectObject(dc, old);
    ::DeleteObject(pen);
  }
  ::SelectObject(dc, previous_font);
  ::EndPaint(window_, &ps);
}
}  // namespace strokes::ui
