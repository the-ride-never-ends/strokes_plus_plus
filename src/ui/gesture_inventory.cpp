#include "ui/gesture_inventory.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "ui/settings_controls.h"
#include "ui/settings_ids.h"

namespace strokes::ui {
namespace {
constexpr wchar_t class_name[] = L"StrokesPlusPlusGestureInventory";
constexpr int tile_height = 118;

void draw_stroke(HDC target, const RECT& bounds, const gestures::GestureDefinition& gesture,
                 COLORREF color, UINT dpi) {
  if (gesture.templates.empty() || gesture.templates.front().points.empty()) return;
  const auto& stroke = gesture.templates.front().points;
  double left = std::numeric_limits<double>::max();
  double top = std::numeric_limits<double>::max();
  double right = std::numeric_limits<double>::lowest();
  double bottom = std::numeric_limits<double>::lowest();
  for (const auto& point : stroke) {
    left = std::min(left, point.x);
    top = std::min(top, point.y);
    right = std::max(right, point.x);
    bottom = std::max(bottom, point.y);
  }
  const double width = right - left;
  const double height = bottom - top;
  const double available_width = std::max(1L, bounds.right - bounds.left);
  const double available_height = std::max(1L, bounds.bottom - bounds.top);
  const double scale = std::min(width > 0.0 ? available_width / width : available_width,
                                height > 0.0 ? available_height / height : available_height);
  const double x_offset = bounds.left + (available_width - width * scale) / 2.0;
  const double y_offset = bounds.top + (available_height - height * scale) / 2.0;
  std::vector<POINT> points;
  points.reserve(stroke.size());
  for (const auto& point : stroke)
    points.push_back({static_cast<LONG>(std::lround(x_offset + (point.x - left) * scale)),
                      static_cast<LONG>(std::lround(y_offset + (point.y - top) * scale))});

  const int pen_width = std::max(2, ::MulDiv(3, static_cast<int>(dpi), 96));
  HPEN pen = ::CreatePen(PS_SOLID, pen_width, color);
  HGDIOBJ old_pen = ::SelectObject(target, pen);
  if (points.size() == 1) {
    (void)::Ellipse(target, points[0].x - pen_width, points[0].y - pen_width,
                    points[0].x + pen_width + 1, points[0].y + pen_width + 1);
  } else {
    (void)::Polyline(target, points.data(), static_cast<int>(points.size()));
    const POINT tip = points.back();
    auto prior = points.end() - 2;
    while (prior != points.begin() && prior->x == tip.x && prior->y == tip.y) --prior;
    const double dx = static_cast<double>(tip.x - prior->x);
    const double dy = static_cast<double>(tip.y - prior->y);
    const double length = std::hypot(dx, dy);
    if (length > 0.0) {
      const double ux = dx / length;
      const double uy = dy / length;
      const double arrow_length = std::max(11, ::MulDiv(16, static_cast<int>(dpi), 96));
      const double half_width = arrow_length * 0.5;
      const double base_x = tip.x - ux * arrow_length;
      const double base_y = tip.y - uy * arrow_length;
      POINT arrow[]{tip,
                    {static_cast<LONG>(std::lround(base_x - uy * half_width)),
                     static_cast<LONG>(std::lround(base_y + ux * half_width))},
                    {static_cast<LONG>(std::lround(base_x + uy * half_width)),
                     static_cast<LONG>(std::lround(base_y - ux * half_width))}};
      HBRUSH brush = ::CreateSolidBrush(color);
      HGDIOBJ old_brush = ::SelectObject(target, brush);
      (void)::Polygon(target, arrow, 3);
      (void)::SelectObject(target, old_brush);
      (void)::DeleteObject(brush);
    }
  }
  (void)::SelectObject(target, old_pen);
  (void)::DeleteObject(pen);
}
}  // namespace

void GestureInventory::create() {
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = window_proc;
  wc.hInstance = instance_;
  wc.lpszClassName = class_name;
  wc.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&wc);
  if (registered == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
  window_ = ::CreateWindowExW(
      WS_EX_CLIENTEDGE, class_name, L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP, 25, 50,
      610, 590, parent_, reinterpret_cast<HMENU>(static_cast<INT_PTR>(gesture_inventory_id)),
      instance_, this);
}

void GestureInventory::set_visible(bool visible) const noexcept {
  ::ShowWindow(window_, visible ? SW_SHOW : SW_HIDE);
  if (visible) refresh();
}

void GestureInventory::refresh() const noexcept {
  if (window_) ::InvalidateRect(window_, nullptr, TRUE);
}

LRESULT CALLBACK GestureInventory::window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<GestureInventory*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<GestureInventory*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    self->window_ = window;
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  return self ? self->handle_message(message, wp, lp) : ::DefWindowProcW(window, message, wp, lp);
}

LRESULT GestureInventory::handle_message(UINT message, WPARAM wp, LPARAM lp) {
  if (message == WM_PAINT) {
    PAINTSTRUCT state{};
    HDC target = ::BeginPaint(window_, &state);
    paint(target);
    ::EndPaint(window_, &state);
    return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  if (message == WM_SIZE) {
    scroll_to(0);
    return 0;
  }
  if (message == WM_MOUSEWHEEL) {
    SCROLLINFO info{sizeof(info), SIF_POS};
    ::GetScrollInfo(window_, SB_VERT, &info);
    scroll_to(info.nPos - GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 60);
    return 0;
  }
  if (message == WM_VSCROLL) {
    SCROLLINFO info{sizeof(info), SIF_ALL};
    ::GetScrollInfo(window_, SB_VERT, &info);
    int position = info.nPos;
    switch (LOWORD(wp)) {
      case SB_LINEUP: position -= 30; break;
      case SB_LINEDOWN: position += 30; break;
      case SB_PAGEUP: position -= static_cast<int>(info.nPage); break;
      case SB_PAGEDOWN: position += static_cast<int>(info.nPage); break;
      case SB_THUMBPOSITION:
      case SB_THUMBTRACK: position = info.nTrackPos; break;
      default: return 0;
    }
    scroll_to(position);
    return 0;
  }
  return ::DefWindowProcW(window_, message, wp, lp);
}

void GestureInventory::scroll_to(int position) noexcept {
  SCROLLINFO info{sizeof(info), SIF_POS};
  info.nPos = position;
  ::SetScrollInfo(window_, SB_VERT, &info, TRUE);
  ::InvalidateRect(window_, nullptr, TRUE);
}

void GestureInventory::paint(HDC target) noexcept {
  RECT client{};
  ::GetClientRect(window_, &client);
  ::FillRect(target, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
  SCROLLINFO current{sizeof(current), SIF_POS};
  ::GetScrollInfo(window_, SB_VERT, &current);
  const int saved = ::SaveDC(target);
  ::SetViewportOrgEx(target, 0, -current.nPos, nullptr);
  ::SetBkMode(target, TRANSPARENT);
  HGDIOBJ old_font = ::SelectObject(target, ::GetStockObject(DEFAULT_GUI_FONT));
  const UINT dpi = ::GetDpiForWindow(window_);
  const int width = client.right - client.left;
  const int columns = std::max(1, width / ::MulDiv(140, static_cast<int>(dpi), 96));
  const int tile_width = std::max(1, (width - 16) / columns);
  int y = 8;

  const auto draw_group = [&](const wchar_t* heading, bool active, COLORREF color) {
    std::vector<const gestures::GestureDefinition*> items;
    for (const auto& gesture : configuration_->gestures.gestures)
      if (gesture.enabled == active) items.push_back(&gesture);
    RECT heading_bounds{8, y, width - 8, y + 24};
    ::SetTextColor(target, RGB(0, 65, 145));
    (void)::DrawTextW(target, heading, -1, &heading_bounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    HPEN divider = ::CreatePen(PS_SOLID, 1, RGB(160, 190, 225));
    HGDIOBJ old_divider = ::SelectObject(target, divider);
    ::MoveToEx(target, 82, y + 13, nullptr);
    ::LineTo(target, width - 8, y + 13);
    (void)::SelectObject(target, old_divider);
    (void)::DeleteObject(divider);
    y += 26;
    for (std::size_t index = 0; index < items.size(); ++index) {
      const int column = static_cast<int>(index % static_cast<std::size_t>(columns));
      const int row = static_cast<int>(index / static_cast<std::size_t>(columns));
      const int left = 8 + column * tile_width;
      const int top = y + row * tile_height;
      RECT stroke_bounds{left + 18, top + 8, left + tile_width - 18, top + 76};
      draw_stroke(target, stroke_bounds, *items[index], color, dpi);
      RECT name_bounds{left + 4, top + 82, left + tile_width - 4, top + tile_height};
      ::SetTextColor(target, active ? RGB(20, 20, 20) : RGB(105, 105, 105));
      const std::wstring name = detail::wide(items[index]->name);
      (void)::DrawTextW(target, name.c_str(), -1, &name_bounds,
                        DT_CENTER | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS);
    }
    const int rows = items.empty() ? 0 :
        (static_cast<int>(items.size()) + columns - 1) / columns;
    y += rows * tile_height + 12;
  };

  draw_group(L"Activated", true, RGB(0, 145, 235));
  draw_group(L"Not Activated", false, RGB(120, 120, 120));
  content_height_ = y;
  (void)::SelectObject(target, old_font);
  ::RestoreDC(target, saved);

  SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE};
  info.nMin = 0;
  info.nMax = std::max(0, content_height_ - 1);
  info.nPage = static_cast<UINT>(std::max(0L, client.bottom - client.top));
  ::SetScrollInfo(window_, SB_VERT, &info, TRUE);
}

}  // namespace strokes::ui
