#include "ui/gesture_editor.h"

#include <algorithm>
#include <array>
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
  text(window_, gesture_section_label_id, L"Global Actions", 25, 50, 230);
  control(window_, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL, gestures_id, 25, 75, 235, 430);
  control(window_, L"BUTTON", L"Add Action", BS_PUSHBUTTON, global_add_id, 25, 515, 75, 28);
  control(window_, L"BUTTON", L"Edit Action", BS_PUSHBUTTON, global_assign_id, 105, 515, 75, 28);
  control(window_, L"BUTTON", L"Delete Action", BS_PUSHBUTTON, global_remove_id, 185, 515, 75, 28);

  text(window_, global_action_label_id, L"Gesture", 280, 50, 320);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, gesture_select_id, 280, 75, 320, 300);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, gesture_name_id, 280, 107, 320, 24);
  control(window_, L"BUTTON", L"New", BS_PUSHBUTTON, gesture_add_id, 280, 139, 52, 26);
  control(window_, L"BUTTON", L"Rename", BS_PUSHBUTTON, gesture_rename_id, 338, 139, 62, 26);
  control(window_, L"BUTTON", L"Delete", BS_PUSHBUTTON, gesture_delete_id, 406, 139, 58, 26);
  control(window_, L"BUTTON", L"Train", BS_PUSHBUTTON, gesture_train_id, 470, 139, 54, 26);
  control(window_, L"BUTTON", L"Enable / Disable", BS_PUSHBUTTON, gesture_toggle_id, 280, 171, 124,
          26);
  control(window_, L"BUTTON", L"Remove last sample", BS_PUSHBUTTON, gesture_remove_sample_id, 412,
          171, 128, 26);
  WNDCLASSEXW preview_window{sizeof(preview_window)};
  preview_window.lpfnWndProc = preview_proc;
  preview_window.hInstance = instance_;
  preview_window.lpszClassName = preview_class;
  preview_window.hCursor = ::LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  preview_window.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  const ATOM registered = ::RegisterClassExW(&preview_window);
  if (registered != 0 || ::GetLastError() == ERROR_CLASS_ALREADY_EXISTS) {
    preview_ = ::CreateWindowExW(
        WS_EX_CLIENTEDGE, preview_class, L"", WS_CHILD | WS_VISIBLE, 280, 207, 320, 255, window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(gesture_preview_id)), instance_, this);
  }
  text(window_, assigned_action_label_id, L"Assigned action", 280, 474, 150);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL | ES_READONLY, shortcut_id, 280, 497, 320, 24);
}

void GestureEditor::set_visible(bool visible) const noexcept {
  for (const int id : {gesture_section_label_id, global_action_label_id, assigned_action_label_id,
                       shortcut_id, gestures_id, gesture_select_id, gesture_name_id,
                       gesture_add_id, gesture_rename_id, gesture_delete_id, gesture_train_id,
                       gesture_remove_sample_id, gesture_toggle_id, global_add_id,
                       global_assign_id,
                       global_remove_id})
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
  if (!selected_action_id_.empty()) {
    ::SetBkMode(target, TRANSPARENT);
    ::SetTextColor(target, ::GetSysColor(COLOR_WINDOWTEXT));
    (void)::DrawTextW(target, L"No Gesture Assigned", -1, &client,
                      DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    return;
  }
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
  if (command == global_add_id) {
    add_action();
    return true;
  }
  if (command == global_assign_id) {
    edit_action();
    return true;
  }
  if (command == global_remove_id) {
    remove_action();
    return true;
  }
  if (command == gestures_id && notification == LBN_SELCHANGE) {
    select_action();
    return true;
  }
  if (command == gesture_select_id && notification == CBN_SELCHANGE) {
    selected_action_id_.clear();
    if (preview_) ::SetWindowTextW(preview_, L"");
    load();
    return true;
  }
  return false;
}

int GestureEditor::index() const noexcept {
  const LRESULT selected = ::SendDlgItemMessageW(window_, gesture_select_id, CB_GETCURSEL, 0, 0);
  return selected == CB_ERR ? -1 : static_cast<int>(selected);
}

std::string GestureEditor::selected() const {
  if (!selected_action_id_.empty()) return selected_action_id_;
  const int chosen = index();
  if (chosen < 0) return {};
  return configuration_->gestures.gestures[static_cast<std::size_t>(chosen)].id;
}

void GestureEditor::refresh() {
  ::SendDlgItemMessageW(window_, gestures_id, LB_RESETCONTENT, 0, 0);
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_RESETCONTENT, 0, 0);
  action_gesture_ids_.clear();
  selected_action_id_.clear();
  if (preview_) ::SetWindowTextW(preview_, L"");
  for (const auto& gesture : configuration_->gestures.gestures) {
    std::wstring gesture_label = wide(gesture.name);
    gesture_label += gesture.enabled ? L" [active]" : L" [inactive]";
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(gesture_label.c_str()));
    const auto action = configuration_->profiles.global_actions.find(gesture.id);
    if (action == configuration_->profiles.global_actions.end()) continue;
    const std::wstring label = wide(actions::action_label(action->second));
    ::SendDlgItemMessageW(window_, gestures_id, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str()));
    action_gesture_ids_.push_back(gesture.id);
  }
  for (const auto& [id, name] : std::array<std::pair<const char*, const char*>, 4>{
           {{"wheel-down", "Wheel Down: Volume Down"},
            {"wheel-up", "Wheel Up: Volume Up"},
            {"rocker-back", "Rocker Back"},
            {"rocker-forward", "Rocker Forward"}}}) {
    if (!configuration_->profiles.global_actions.contains(id)) continue;
    const std::wstring label = wide(name);
    ::SendDlgItemMessageW(window_, gestures_id, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str()));
    action_gesture_ids_.push_back(id);
  }
  bool has_unassigned = std::ranges::any_of(
      configuration_->gestures.gestures, [this](const auto& gesture) {
        return !configuration_->profiles.global_actions.contains(gesture.id);
      });
  for (const char* id : {"wheel-down", "wheel-up", "rocker-back", "rocker-forward"})
    has_unassigned |= !configuration_->profiles.global_actions.contains(id);
  ::EnableWindow(::GetDlgItem(window_, global_add_id), has_unassigned);
  if (!configuration_->gestures.gestures.empty())
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, 0, 0);
  refresh_preview();
}

void GestureEditor::load() {
  const int chosen = index();
  if (chosen < 0) {
    ::SetDlgItemTextW(window_, gesture_name_id, L"");
    std::string summary;
    const auto action = configuration_->profiles.global_actions.find(selected_action_id_);
    if (action != configuration_->profiles.global_actions.end())
      summary = actions::action_display_name(action->second);
    ::SetDlgItemTextW(window_, shortcut_id, wide(summary).c_str());
    for (const int id : {gesture_rename_id, gesture_delete_id, gesture_train_id,
                         gesture_toggle_id, gesture_remove_sample_id})
      ::EnableWindow(::GetDlgItem(window_, id), FALSE);
    ::EnableWindow(::GetDlgItem(window_, global_assign_id),
                   action != configuration_->profiles.global_actions.end());
    ::EnableWindow(::GetDlgItem(window_, global_remove_id),
                   action != configuration_->profiles.global_actions.end());
    refresh_preview();
    return;
  }
  const auto& gesture = configuration_->gestures.gestures[static_cast<std::size_t>(chosen)];
  ::SetDlgItemTextW(window_, gesture_name_id, wide(gesture.name).c_str());
  auto action = configuration_->profiles.global_actions.find(gesture.id);
  std::string summary;
  if (action != configuration_->profiles.global_actions.end()) {
    summary = actions::action_display_name(action->second);
  }
  ::SetDlgItemTextW(window_, shortcut_id, wide(summary).c_str());
  ::SendDlgItemMessageW(window_, shortcut_id, EM_SETREADONLY, TRUE, 0);
  ::EnableWindow(::GetDlgItem(window_, shortcut_id), TRUE);
  ::EnableWindow(::GetDlgItem(window_, global_assign_id),
                 action != configuration_->profiles.global_actions.end());
  ::EnableWindow(::GetDlgItem(window_, global_remove_id),
                 action != configuration_->profiles.global_actions.end());
  for (const int id : {gesture_rename_id, gesture_delete_id, gesture_train_id,
                       gesture_toggle_id, gesture_remove_sample_id})
    ::EnableWindow(::GetDlgItem(window_, id), TRUE);
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
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL,
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
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
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
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, next, 0);
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
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
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
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
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
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::add_action() {
  std::vector<GlobalActionTarget> targets;
  for (const auto& gesture : configuration_->gestures.gestures) {
    if (!configuration_->profiles.global_actions.contains(gesture.id))
      targets.push_back({gesture.id, gesture.name});
  }
  for (const auto& [id, name] : std::array<std::pair<const char*, const char*>, 4>{
           {{"wheel-down", "Wheel Down"}, {"wheel-up", "Wheel Up"},
            {"rocker-back", "Rocker Back"}, {"rocker-forward", "Rocker Forward"}}}) {
    if (!configuration_->profiles.global_actions.contains(id)) targets.push_back({id, name});
  }
  if (targets.empty()) return;
  WindowsActionEditor editor;
  auto result = editor.add_global(instance_, window_, std::move(targets), selected());
  if (!result.accepted || !result.action || result.gesture_id.empty()) return;
  const std::string gesture_id = std::move(result.gesture_id);
  configuration_->profiles.global_actions.emplace(gesture_id, std::move(*result.action));
  refresh();
  const auto found = std::ranges::find(configuration_->gestures.gestures, gesture_id,
                                       &gestures::GestureDefinition::id);
  if (found == configuration_->gestures.gestures.end()) {
    selected_action_id_ = gesture_id;
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL,
                          static_cast<WPARAM>(CB_ERR), 0);
    if (preview_) ::SetWindowTextW(preview_, L"No Gesture Assigned");
  } else {
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL,
                          static_cast<WPARAM>(found - configuration_->gestures.gestures.begin()),
                          0);
  }
  load();
}

void GestureEditor::edit_action() {
  const int chosen = index();
  const std::string gesture_id = selected();
  if (gesture_id.empty()) return;
  const bool input_trigger = chosen < 0;
  auto existing = configuration_->profiles.global_actions.find(gesture_id);
  if (existing == configuration_->profiles.global_actions.end()) return;
  WindowsActionEditor editor;
  auto result = editor.edit(instance_, window_, &existing->second);
  if (!result.accepted) return;
  if (result.action)
    configuration_->profiles.global_actions.insert_or_assign(gesture_id,
                                                              std::move(*result.action));
  else
    configuration_->profiles.global_actions.erase(gesture_id);
  refresh();
  if (input_trigger) {
    selected_action_id_ = gesture_id;
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL,
                          static_cast<WPARAM>(CB_ERR), 0);
    if (preview_) ::SetWindowTextW(preview_, L"No Gesture Assigned");
  } else {
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
  }
  load();
}

void GestureEditor::remove_action() {
  const int chosen = index();
  const std::string gesture_id = selected();
  if (gesture_id.empty()) return;
  configuration_->profiles.global_actions.erase(gesture_id);
  refresh();
  if (chosen >= 0)
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, chosen, 0);
  load();
}

void GestureEditor::select_action() {
  const LRESULT selected = ::SendDlgItemMessageW(window_, gestures_id, LB_GETCURSEL, 0, 0);
  if (selected == LB_ERR || static_cast<std::size_t>(selected) >= action_gesture_ids_.size()) return;
  const auto found = std::ranges::find(configuration_->gestures.gestures,
                                       action_gesture_ids_[static_cast<std::size_t>(selected)],
                                       &gestures::GestureDefinition::id);
  if (found == configuration_->gestures.gestures.end()) {
    selected_action_id_ = action_gesture_ids_[static_cast<std::size_t>(selected)];
    ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL,
                          static_cast<WPARAM>(CB_ERR), 0);
    ::SetDlgItemTextW(window_, gesture_name_id, L"");
    if (preview_) ::SetWindowTextW(preview_, L"No Gesture Assigned");
    load();
    return;
  }
  selected_action_id_.clear();
  if (preview_) ::SetWindowTextW(preview_, L"");
  const auto index = static_cast<LRESULT>(found - configuration_->gestures.gestures.begin());
  ::SendDlgItemMessageW(window_, gesture_select_id, CB_SETCURSEL, index, 0);
  load();
}

bool GestureEditor::save(config::ConfigurationBundle& target) const {
  (void)target;
  return true;
}

}  // namespace strokes::ui
