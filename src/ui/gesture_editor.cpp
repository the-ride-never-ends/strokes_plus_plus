#include "ui/gesture_editor.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "actions/keyboard_shortcut.h"
#include "gestures/gesture_repository.h"
#include "ui/settings_controls.h"
#include "ui/settings_ids.h"
#include "ui/windows_gesture_trainer.h"
#include "ui/windows_action_editor.h"

namespace strokes::ui {
namespace {
constexpr wchar_t preview_class[] = L"StrokesPlusPlusGesturePreview";
}

using detail::control;
using detail::read_utf8;
using detail::text;
using detail::wide;

void GestureEditor::create() {
  text(window_, gesture_section_label_id, L"Gestures", 25, 50, 280);
  control(window_, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL, gestures_id, 25, 75, 280, 300);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, gesture_name_id, 325, 75, 300, 24);
  control(window_, L"BUTTON", L"Add", BS_PUSHBUTTON, gesture_add_id, 325, 107, 50, 26);
  control(window_, L"BUTTON", L"Rename", BS_PUSHBUTTON, gesture_rename_id, 379, 107, 62, 26);
  control(window_, L"BUTTON", L"Delete", BS_PUSHBUTTON, gesture_delete_id, 445, 107, 50, 26);
  control(window_, L"BUTTON", L"Train", BS_PUSHBUTTON, gesture_train_id, 499, 107, 50, 26);
  control(window_, L"BUTTON", L"Remove last sample", BS_PUSHBUTTON, gesture_remove_sample_id, 325,
          141, 112, 26);
  control(window_, L"BUTTON", L"Enable / Disable", BS_PUSHBUTTON, gesture_toggle_id, 445, 141, 124,
          26);
  text(window_, global_action_label_id, L"Selected gesture global action", 325, 193, 260);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, shortcut_id, 325, 218, 220, 24);
  control(window_, L"BUTTON", L"Configure", BS_PUSHBUTTON, global_assign_id, 553, 218, 72, 24);
  WNDCLASSEXW preview_window{sizeof(preview_window)};
  preview_window.lpfnWndProc = preview_proc;
  preview_window.hInstance = instance_;
  preview_window.lpszClassName = preview_class;
  preview_window.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  preview_window.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&preview_window);
  if (registered != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
    preview_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE, preview_class, L"", WS_CHILD | WS_VISIBLE, 325, 265, 300, 340, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(gesture_preview_id)), instance_, this);
  }
}

void GestureEditor::set_visible(bool visible) const noexcept {
  for (const int id : {gesture_section_label_id, global_action_label_id, shortcut_id, gestures_id,
                       gesture_name_id, gesture_add_id, gesture_rename_id, gesture_delete_id,
                       gesture_train_id, gesture_remove_sample_id, gesture_toggle_id,
                       global_assign_id})
    ::ShowWindow(::GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE);
  ::ShowWindow(preview_, visible ? SW_SHOW : SW_HIDE);
}

LRESULT CALLBACK GestureEditor::preview_proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
  auto* self = reinterpret_cast<GestureEditor*>(::GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    self = static_cast<GestureEditor*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
    ::SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (message == WM_PAINT && self) {
    PAINTSTRUCT paint{};
    HDC target = ::BeginPaint(window, &paint);
    self->paint_preview(window, target);
    ::EndPaint(window, &paint);
    return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  return ::DefWindowProcW(window, message, wp, lp);
}

void GestureEditor::paint_preview(HWND preview, HDC target) const noexcept {
  RECT client{};
  ::GetClientRect(preview, &client);
  ::FillRect(target, &client, reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1));
  const int chosen = index();
  if (chosen < 0 || static_cast<std::size_t>(chosen) >= configuration_->gestures.gestures.size())
    return;
  const auto& templates =
      configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].templates;
  if (templates.empty() || templates.front().points.empty()) return;
  const auto& stroke = templates.front().points;
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
  constexpr double padding = 24.0;
  const double width = right - left;
  const double height = bottom - top;
  const double available_width = std::max(1.0, client.right - client.left - padding * 2.0);
  const double available_height = std::max(1.0, client.bottom - client.top - padding * 2.0);
  const double x_scale = width > 0.0 ? available_width / width : available_width;
  const double y_scale = height > 0.0 ? available_height / height : available_height;
  const double scale = std::min(x_scale, y_scale);
  const double drawn_width = width * scale;
  const double drawn_height = height * scale;
  const double x_offset = (client.right - drawn_width) / 2.0;
  const double y_offset = (client.bottom - drawn_height) / 2.0;
  std::vector<POINT> points;
  points.reserve(stroke.size());
  for (const auto& point : stroke) {
    points.push_back({static_cast<LONG>(std::lround(x_offset + (point.x - left) * scale)),
                      static_cast<LONG>(std::lround(y_offset + (point.y - top) * scale))});
  }
  const int pen_width = std::max(2, ::MulDiv(4, static_cast<int>(::GetDpiForWindow(preview)), 96));
  HPEN pen = ::CreatePen(PS_SOLID, pen_width, RGB(0, 160, 220));
  HGDIOBJ previous = ::SelectObject(target, pen);
  if (points.size() == 1) {
    ::Ellipse(target, points[0].x - pen_width, points[0].y - pen_width,
              points[0].x + pen_width + 1, points[0].y + pen_width + 1);
  } else {
    (void)::Polyline(target, points.data(), static_cast<int>(points.size()));
    const POINT tip = points.back();
    auto previous_point = points.end() - 2;
    while (previous_point != points.begin() && previous_point->x == tip.x &&
           previous_point->y == tip.y)
      --previous_point;
    const double direction_x = static_cast<double>(tip.x - previous_point->x);
    const double direction_y = static_cast<double>(tip.y - previous_point->y);
    const double direction_length = std::hypot(direction_x, direction_y);
    if (direction_length > 0.0) {
      const double unit_x = direction_x / direction_length;
      const double unit_y = direction_y / direction_length;
      const double arrow_length = static_cast<double>(
          std::max(14, ::MulDiv(22, static_cast<int>(::GetDpiForWindow(preview)), 96)));
      const double arrow_half_width = arrow_length * 0.55;
      const double base_x = tip.x - unit_x * arrow_length;
      const double base_y = tip.y - unit_y * arrow_length;
      POINT arrow[] = {
          tip,
          {static_cast<LONG>(std::lround(base_x - unit_y * arrow_half_width)),
           static_cast<LONG>(std::lround(base_y + unit_x * arrow_half_width))},
          {static_cast<LONG>(std::lround(base_x + unit_y * arrow_half_width)),
           static_cast<LONG>(std::lround(base_y - unit_x * arrow_half_width))}};
      HBRUSH brush = ::CreateSolidBrush(RGB(0, 160, 220));
      HGDIOBJ previous_brush = ::SelectObject(target, brush);
      (void)::Polygon(target, arrow, 3);
      ::SelectObject(target, previous_brush);
      ::DeleteObject(brush);
    }
  }
  ::SelectObject(target, previous);
  ::DeleteObject(pen);
}

void GestureEditor::refresh_preview() const noexcept {
  if (preview_) ::InvalidateRect(preview_, nullptr, TRUE);
}

bool GestureEditor::handle(int command, int notification) {
  if (command == gesture_add_id) {
    add();
    return true;
  }
  if (command == gesture_rename_id) {
    rename();
    return true;
  }
  if (command == gesture_delete_id) {
    erase();
    return true;
  }
  if (command == gesture_train_id) {
    train();
    return true;
  }
  if (command == gesture_remove_sample_id) {
    drop_sample();
    return true;
  }
  if (command == gesture_toggle_id) {
    toggle();
    return true;
  }
  if (command == global_assign_id) {
    assign();
    return true;
  }
  if (command == gestures_id && notification == LBN_SELCHANGE) {
    load();
    return true;
  }
  return false;
}

int GestureEditor::index() const noexcept {
  const LRESULT selected = ::SendDlgItemMessageW(window_, gestures_id, LB_GETCURSEL, 0, 0);
  return selected == LB_ERR ? -1 : static_cast<int>(selected);
}

std::string GestureEditor::selected() const {
  const int chosen = index();
  if (chosen < 0) return {};
  return configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].id;
}

void GestureEditor::refresh() {
  ::SendDlgItemMessageW(window_, gestures_id, LB_RESETCONTENT, 0, 0);
  for (const auto& gesture : configuration_->gestures.gestures) {
    std::wstring label = wide(gesture.name);
    label += gesture.enabled ? L" [enabled]" : L" [disabled]";
    label += L" - " + std::to_wstring(gesture.templates.size()) + L" sample(s)";
    ::SendDlgItemMessageW(window_, gestures_id, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str()));
  }
  refresh_preview();
}

void GestureEditor::load() {
  const int chosen = index();
  if (chosen < 0) {
    refresh_preview();
    return;
  }
  const auto& gesture = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)];
  ::SetDlgItemTextW(window_, gesture_name_id, wide(gesture.name).c_str());
  auto action = configuration_->profiles.global_actions.find(gesture.id);
  std::string summary;
  if (action != configuration_->profiles.global_actions.end()) {
    summary = actions::action_type_name(action->second.type) + "." +
              actions::action_operation_name(action->second);
  }
  ::SetDlgItemTextW(window_, shortcut_id, wide(summary).c_str());
  ::SendDlgItemMessageW(window_, shortcut_id, EM_SETREADONLY, TRUE, 0);
  ::EnableWindow(::GetDlgItem(window_, shortcut_id), TRUE);
  ::EnableWindow(::GetDlgItem(window_, global_assign_id), TRUE);
  refresh_preview();
}

void GestureEditor::add() {
  const std::string name = read_utf8(window_, gesture_name_id);
  if (name.empty()) {
    ::MessageBoxW(window_, L"Enter a gesture name first.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  std::string id = "gesture-" + std::to_string(::GetTickCount64());
  unsigned suffix = 1;
  while (!repository.create(id, name)) {
    if (repository.find(id))
      id = "gesture-" + std::to_string(::GetTickCount64()) + "-" + std::to_string(suffix++);
    else {
      ::MessageBoxW(window_, L"Gesture names must be unique.", L"Strokes++", MB_OK | MB_ICONERROR);
      return;
    }
  }
  refresh();
  ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL,
                        configuration_->gestures.gestures.size() - 1, 0);
  load();
}

void GestureEditor::rename() {
  const int chosen = index();
  if (chosen < 0) return;
  const std::string name = read_utf8(window_, gesture_name_id);
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  if (!repository.rename(configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].id,
                         name))
    ::MessageBoxW(window_, L"Enter a unique gesture name.", L"Strokes++", MB_OK | MB_ICONERROR);
  refresh();
  ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::erase() {
  const int chosen = index();
  if (chosen < 0) return;
  if (::MessageBoxW(window_, L"Delete the selected gesture and all of its mappings?", L"Strokes++",
                    MB_YESNO | MB_ICONWARNING) != IDYES)
    return;
  const std::string id = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].id;
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  (void)repository.erase(id);
  configuration_->profiles.global_actions.erase(id);
  for (auto& profile : configuration_->profiles.profiles) profile.actions_by_gesture.erase(id);
  refresh();
  if (!configuration_->gestures.gestures.empty()) {
    const int next = std::min(chosen,
                              static_cast<int>(configuration_->gestures.gestures.size()) - 1);
    ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, next, 0);
    load();
  }
}

void GestureEditor::train() {
  const int chosen = index();
  if (chosen < 0) return;
  ::EnableWindow(window_, FALSE);
  WindowsGestureTrainer trainer;
  auto stroke = trainer.capture(instance_, configuration_->global.minimum_point_distance, window_);
  ::EnableWindow(window_, TRUE);
  ::SetForegroundWindow(window_);
  if (!stroke) return;
  auto& definition = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)];
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  const std::string id = "template-" + std::to_string(::GetTickCount64());
  if (repository.add_template(definition.id, {id, std::move(*stroke)}))
    (void)repository.set_enabled(definition.id, true);
  refresh();
  ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::drop_sample() {
  const int chosen = index();
  if (chosen < 0) return;
  auto& gesture = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)];
  if (gesture.templates.empty()) return;
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  (void)repository.remove_template(gesture.id, gesture.templates.back().id);
  refresh();
  ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::toggle() {
  const int chosen = index();
  if (chosen < 0) return;
  auto& gesture = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)];
  gestures::GestureRepository repository(configuration_->gestures.gestures);
  if (!repository.set_enabled(gesture.id, !gesture.enabled)) {
    ::MessageBoxW(window_,
                  L"A gesture needs at least one training sample before it can be enabled.",
                  L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh();
  ::SendDlgItemMessageW(window_, gestures_id, LB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::assign() {
  const int chosen = index();
  if (chosen < 0) return;
  const auto& gesture_id = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].id;
  auto existing = configuration_->profiles.global_actions.find(gesture_id);
  WindowsActionEditor editor;
  auto result = editor.edit(instance_, window_,
                            existing == configuration_->profiles.global_actions.end()
                                ? nullptr
                                : &existing->second);
  if (!result.accepted) return;
  if (result.action)
    configuration_->profiles.global_actions.insert_or_assign(gesture_id,
                                                              std::move(*result.action));
  else
    configuration_->profiles.global_actions.erase(gesture_id);
  load();
}

bool GestureEditor::save(config::ConfigurationBundle& target) const {
  (void)target;
  return true;
}

}  // namespace strokes::ui
