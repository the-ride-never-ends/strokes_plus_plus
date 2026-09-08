#include "overlay/windows_gesture_overlay.h"

#include <algorithm>

namespace strokes::overlay {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusGestureOverlay";
constexpr UINT refresh_message = WM_APP + 20;
constexpr UINT show_message = WM_APP + 21;
constexpr UINT hide_message = WM_APP + 22;
}

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
    if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

    const int x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int height = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    window_ = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        class_name, L"", WS_POPUP, x, y, width, height, nullptr, nullptr, instance_, this);
    if (window_ == nullptr) return false;
    ::SetLayeredWindowAttributes(window_, RGB(0, 0, 0), options_.opacity, LWA_COLORKEY | LWA_ALPHA);
    return true;
}

void WindowsGestureOverlay::destroy() noexcept {
    if (window_ != nullptr) {
        ::DestroyWindow(window_);
        window_ = nullptr;
    }
    if (instance_ != nullptr) ::UnregisterClassW(class_name, instance_);
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
        points_ = points;
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
    if (!options_.enabled) hide();
    else ::InvalidateRect(window_, nullptr, TRUE);
}

LRESULT CALLBACK WindowsGestureOverlay::window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
    auto* self = reinterpret_cast<WindowsGestureOverlay*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        self = static_cast<WindowsGestureOverlay*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        self->window_ = window;
        ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT WindowsGestureOverlay::handle_message(UINT message, WPARAM wp, LPARAM lp) {
    if (message == refresh_message) { ::InvalidateRect(window_, nullptr, TRUE); return 0; }
    if (message == show_message) {
        ::ShowWindow(window_, SW_SHOWNOACTIVATE);
        ::InvalidateRect(window_, nullptr, TRUE);
        return 0;
    }
    if (message == hide_message) {
        ::ShowWindow(window_, SW_HIDE);
        std::scoped_lock lock(points_mutex_);
        points_.clear();
        return 0;
    }
    if (message == WM_PAINT) { paint(); return 0; }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_NCHITTEST) return HTTRANSPARENT;
    if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    return ::DefWindowProcW(window_, message, wp, lp);
}

void WindowsGestureOverlay::paint() noexcept {
    PAINTSTRUCT paint_data{};
    HDC dc = ::BeginPaint(window_, &paint_data);
    RECT client{};
    ::GetClientRect(window_, &client);
    ::FillRect(dc, &client, static_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
    gestures::Stroke points;
    {
        std::scoped_lock lock(points_mutex_);
        points = points_;
    }
    if (points.size() >= 2) {
        HPEN pen = ::CreatePen(PS_SOLID, options_.line_width, options_.color);
        HGDIOBJ previous = ::SelectObject(dc, pen);
        const int origin_x = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int origin_y = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
        ::MoveToEx(dc, static_cast<int>(points.front().x) - origin_x,
                   static_cast<int>(points.front().y) - origin_y, nullptr);
        for (const auto& point : points) {
            ::LineTo(dc, static_cast<int>(point.x) - origin_x, static_cast<int>(point.y) - origin_y);
        }
        ::SelectObject(dc, previous);
        ::DeleteObject(pen);
    }
    ::EndPaint(window_, &paint_data);
}
}  // namespace strokes::overlay
