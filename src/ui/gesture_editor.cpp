#include "ui/gesture_editor.h"

#include <string>
#include <utility>

#include "actions/keyboard_shortcut.h"
#include "gestures/gesture_repository.h"
#include "ui/settings_controls.h"
#include "ui/settings_ids.h"
#include "ui/windows_gesture_trainer.h"
#include "ui/windows_action_editor.h"

namespace strokes::ui {

using detail::control;
using detail::read_utf8;
using detail::text;
using detail::wide;

void GestureEditor::create() {
  text(window_, 0, L"Selected gesture global action", 20, 380);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, shortcut_id, 230, 376, 110, 24);
  control(window_, L"BUTTON", L"Configure", BS_PUSHBUTTON, global_assign_id, 344, 376, 60, 24);
  text(window_, 0, L"Gestures", 410, 12);
  control(window_, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL, gestures_id, 400, 38, 220, 150);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, gesture_name_id, 400, 194, 220, 24);
  control(window_, L"BUTTON", L"Add", BS_PUSHBUTTON, gesture_add_id, 400, 224, 50, 26);
  control(window_, L"BUTTON", L"Rename", BS_PUSHBUTTON, gesture_rename_id, 454, 224, 62, 26);
  control(window_, L"BUTTON", L"Delete", BS_PUSHBUTTON, gesture_delete_id, 520, 224, 50, 26);
  control(window_, L"BUTTON", L"Train", BS_PUSHBUTTON, gesture_train_id, 574, 224, 50, 26);
  control(window_, L"BUTTON", L"Remove last sample", BS_PUSHBUTTON, gesture_remove_sample_id, 400,
          254, 96, 26);
  control(window_, L"BUTTON", L"Enable / Disable", BS_PUSHBUTTON, gesture_toggle_id, 500, 254, 124,
          26);
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
}

void GestureEditor::load() {
  const int chosen = index();
  if (chosen < 0) return;
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
