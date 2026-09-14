#include "ui/profile_editor.h"

#include <string>
#include <utility>

#include "actions/keyboard_shortcut.h"
#include "context/profile_repository.h"
#include "ui/settings_controls.h"
#include "ui/settings_ids.h"
#include "ui/windows_action_editor.h"

namespace strokes::ui {
namespace {
// Both criterion combo boxes list their enumeration in declaration order.
constexpr LRESULT property_count = 3;
constexpr LRESULT mode_count = 3;
}  // namespace

using detail::control;
using detail::populate_processes;
using detail::read_utf8;
using detail::selected_combo;
using detail::text;
using detail::wide;

void ProfileEditor::create() {
  text(window_, profile_section_label_id, L"Application profiles", 25, 50, 210);
  control(window_, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL, profiles_id, 25, 75, 210, 160);
  text(window_, profile_name_label_id, L"Profile name", 250, 50, 100);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, profile_name_id, 250, 75, 180, 24);
  text(window_, profile_property_label_id, L"Match field", 340, 300, 90);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, profile_property_id, 430, 296, 195, 120);
  for (auto* value : {L"Process", L"Window title", L"Window class"})
    ::SendDlgItemMessageW(window_, profile_property_id, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(value));
  ::SendDlgItemMessageW(window_, profile_property_id, CB_SETCURSEL, 0, 0);
  text(window_, profile_mode_label_id, L"Match mode", 340, 332, 90);
  control(window_, L"COMBOBOX", L"", CBS_DROPDOWNLIST, profile_mode_id, 430, 328, 195, 120);
  for (auto* value : {L"Exact", L"Contains", L"Regular expression"})
    ::SendDlgItemMessageW(window_, profile_mode_id, CB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(value));
  ::SendDlgItemMessageW(window_, profile_mode_id, CB_SETCURSEL, 0, 0);
  text(window_, profile_value_label_id, L"Match value", 340, 364, 90);
  HWND process_values =
      control(window_, L"COMBOBOX", L"", CBS_DROPDOWN | CBS_AUTOHSCROLL | WS_VSCROLL,
              profile_value_id, 430, 360, 195, 180);
  populate_processes(process_values);
  control(window_, L"BUTTON", L"Add", BS_PUSHBUTTON, profile_add_id, 440, 75, 50, 26);
  control(window_, L"BUTTON", L"Rename", BS_PUSHBUTTON, profile_update_id, 494, 75, 62, 26);
  control(window_, L"BUTTON", L"Delete", BS_PUSHBUTTON, profile_delete_id, 560, 75, 60, 26);
  control(window_, L"BUTTON", L"Enable / Disable", BS_PUSHBUTTON, profile_toggle_id, 250, 109, 110,
          26);
  text(window_, profile_override_label_id, L"Override action", 250, 145, 100);
  control(window_, L"EDIT", L"", ES_AUTOHSCROLL, profile_shortcut_id, 350, 141, 170, 24);
  control(window_, L"BUTTON", L"Configure override", BS_PUSHBUTTON, profile_assign_id, 528, 141, 97,
          26);
  text(window_, profile_criteria_label_id, L"Selected profile criteria", 25, 275, 260);
  control(window_, L"LISTBOX", L"", LBS_NOTIFY | WS_VSCROLL, profile_criteria_id, 25, 300, 295,
          170);
  control(window_, L"BUTTON", L"Add criterion", BS_PUSHBUTTON, criterion_add_id, 25, 476, 90, 24);
  control(window_, L"BUTTON", L"Update criterion", BS_PUSHBUTTON, criterion_update_id, 119, 476,
          96, 24);
  control(window_, L"BUTTON", L"Remove criterion", BS_PUSHBUTTON, criterion_remove_id, 219, 476,
          101, 24);
}

void ProfileEditor::set_visible(bool visible) const noexcept {
  for (const int id : {profile_section_label_id, profile_name_label_id, profile_property_label_id,
                       profile_mode_label_id, profile_value_label_id, profile_override_label_id,
                       profile_criteria_label_id, profiles_id, profile_name_id, profile_property_id,
                       profile_mode_id, profile_value_id, profile_criteria_id, criterion_add_id,
                       criterion_update_id, criterion_remove_id, profile_shortcut_id, profile_add_id,
                       profile_update_id, profile_delete_id, profile_toggle_id, profile_assign_id})
    ::ShowWindow(::GetDlgItem(window_, id), visible ? SW_SHOW : SW_HIDE);
}

bool ProfileEditor::handle(int command, int notification, const std::string& gesture_id) {
  if (command == profile_add_id) {
    add(gesture_id);
    return true;
  }
  if (command == profile_update_id) {
    rename();
    return true;
  }
  if (command == profile_delete_id) {
    erase();
    return true;
  }
  if (command == profile_toggle_id) {
    toggle();
    return true;
  }
  if (command == criterion_add_id) {
    add_criterion();
    return true;
  }
  if (command == criterion_update_id) {
    update_criterion();
    return true;
  }
  if (command == criterion_remove_id) {
    remove_criterion();
    return true;
  }
  if (command == profile_assign_id) {
    assign(gesture_id);
    return true;
  }
  if (command == profiles_id && notification == LBN_SELCHANGE) {
    load(gesture_id);
    return true;
  }
  if (command == profile_criteria_id && notification == LBN_SELCHANGE) {
    load_criterion();
    return true;
  }
  return false;
}

int ProfileEditor::profile_index() const noexcept {
  const LRESULT selected = ::SendDlgItemMessageW(window_, profiles_id, LB_GETCURSEL, 0, 0);
  return selected == LB_ERR ? -1 : static_cast<int>(selected);
}

int ProfileEditor::criterion_index() const noexcept {
  const LRESULT selected = ::SendDlgItemMessageW(window_, profile_criteria_id, LB_GETCURSEL, 0, 0);
  return selected == LB_ERR ? -1 : static_cast<int>(selected);
}

void ProfileEditor::refresh() {
  ::SendDlgItemMessageW(window_, profiles_id, LB_RESETCONTENT, 0, 0);
  for (const auto& profile : configuration_->profiles.profiles) {
    std::wstring label = wide(profile.name);
    label += profile.enabled ? L" [enabled]" : L" [disabled]";
    ::SendDlgItemMessageW(window_, profiles_id, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str()));
  }
}

void ProfileEditor::refresh_criteria() {
  ::SendDlgItemMessageW(window_, profile_criteria_id, LB_RESETCONTENT, 0, 0);
  const int selected = profile_index();
  if (selected < 0) return;
  const auto& criteria =
      configuration_->profiles.profiles[static_cast<std::size_t>(selected)].criteria;
  for (const auto& criterion : criteria) {
    const wchar_t* property =
        criterion.property == context::ApplicationProperty::process_name   ? L"Process"
        : criterion.property == context::ApplicationProperty::window_title ? L"Title"
                                                                           : L"Class";
    const wchar_t* mode = criterion.mode == context::MatchMode::exact      ? L"exact"
                          : criterion.mode == context::MatchMode::contains ? L"contains"
                                                                           : L"regex";
    std::wstring label = property;
    label += L" ";
    label += mode;
    label += L": ";
    label += wide(criterion.value);
    ::SendDlgItemMessageW(window_, profile_criteria_id, LB_ADDSTRING, 0,
                          reinterpret_cast<LPARAM>(label.c_str()));
  }
}

void ProfileEditor::load(const std::string& gesture_id) {
  const int selected = profile_index();
  if (selected < 0) return;
  const auto& profile = configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
  ::SetDlgItemTextW(window_, profile_name_id, wide(profile.name).c_str());
  refresh_criteria();
  if (!profile.criteria.empty()) {
    ::SendDlgItemMessageW(window_, profile_criteria_id, LB_SETCURSEL, 0, 0);
    load_criterion();
  } else
    ::SetDlgItemTextW(window_, profile_value_id, L"");
  if (gesture_id.empty()) return;
  auto action = profile.actions_by_gesture.find(gesture_id);
  std::string summary;
  if (action != profile.actions_by_gesture.end())
    summary = actions::action_type_name(action->second.type) + "." +
              actions::action_operation_name(action->second);
  ::SetDlgItemTextW(window_, profile_shortcut_id, wide(summary).c_str());
  ::SendDlgItemMessageW(window_, profile_shortcut_id, EM_SETREADONLY, TRUE, 0);
}

void ProfileEditor::load_criterion() {
  const int profile = profile_index(), criterion_slot = criterion_index();
  if (profile < 0 || criterion_slot < 0) return;
  const auto& criterion = configuration_->profiles.profiles[static_cast<std::size_t>(profile)]
                              .criteria[static_cast<std::size_t>(criterion_slot)];
  ::SendDlgItemMessageW(window_, profile_property_id, CB_SETCURSEL,
                        static_cast<WPARAM>(criterion.property), 0);
  ::SendDlgItemMessageW(window_, profile_mode_id, CB_SETCURSEL, static_cast<WPARAM>(criterion.mode),
                        0);
  ::SetDlgItemTextW(window_, profile_value_id, wide(criterion.value).c_str());
}

void ProfileEditor::add(const std::string& gesture_id) {
  const std::string name = read_utf8(window_, profile_name_id);
  const std::string value = read_utf8(window_, profile_value_id);
  const auto property_selection = selected_combo(window_, profile_property_id, property_count);
  const auto mode_selection = selected_combo(window_, profile_mode_id, mode_count);
  if (name.empty() || value.empty() || !property_selection || !mode_selection) {
    ::MessageBoxW(window_, L"Enter a unique profile name and match value.", L"Strokes++",
                  MB_OK | MB_ICONERROR);
    return;
  }
  const auto property = static_cast<context::ApplicationProperty>(*property_selection);
  const auto mode = static_cast<context::MatchMode>(*mode_selection);
  context::ProfileRepository repository(configuration_->profiles.profiles);
  std::string id = "profile-" + std::to_string(::GetTickCount64());
  if (!repository.create(id, name)) {
    ::MessageBoxW(window_, L"Profile name must be unique.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  if (!repository.add_criterion(id, {property, mode, value}) || !repository.set_enabled(id, true)) {
    (void)repository.erase(id);
    ::MessageBoxW(window_, L"The match criterion is invalid.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh();
  ::SendDlgItemMessageW(window_, profiles_id, LB_SETCURSEL,
                        configuration_->profiles.profiles.size() - 1, 0);
  load(gesture_id);
}

void ProfileEditor::rename() {
  const int selected = profile_index();
  if (selected < 0) return;
  const std::string name = read_utf8(window_, profile_name_id);
  if (name.empty()) {
    ::MessageBoxW(window_, L"Enter a profile name.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  auto& profile = configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
  context::ProfileRepository repository(configuration_->profiles.profiles);
  if (!repository.rename(profile.id, name)) {
    ::MessageBoxW(window_, L"Profile names must be unique.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh();
  ::SendDlgItemMessageW(window_, profiles_id, LB_SETCURSEL, selected, 0);
}

void ProfileEditor::erase() {
  const int selected = profile_index();
  if (selected < 0) return;
  if (::MessageBoxW(window_, L"Delete the selected profile?", L"Strokes++",
                    MB_YESNO | MB_ICONWARNING) != IDYES)
    return;
  context::ProfileRepository repository(configuration_->profiles.profiles);
  (void)repository.erase(configuration_->profiles.profiles[static_cast<std::size_t>(selected)].id);
  refresh();
  refresh_criteria();
}

void ProfileEditor::toggle() {
  const int selected = profile_index();
  if (selected < 0) return;
  auto& profile = configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
  context::ProfileRepository repository(configuration_->profiles.profiles);
  if (!repository.set_enabled(profile.id, !profile.enabled)) {
    ::MessageBoxW(window_,
                  L"A profile needs at least one match criterion before it can be enabled.",
                  L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh();
  ::SendDlgItemMessageW(window_, profiles_id, LB_SETCURSEL, selected, 0);
}

void ProfileEditor::add_criterion() {
  const int selected = profile_index();
  if (selected < 0) return;
  const std::string value = read_utf8(window_, profile_value_id);
  const auto property_selection = selected_combo(window_, profile_property_id, property_count);
  const auto mode_selection = selected_combo(window_, profile_mode_id, mode_count);
  if (!property_selection || !mode_selection) return;
  const auto property = static_cast<context::ApplicationProperty>(*property_selection);
  const auto mode = static_cast<context::MatchMode>(*mode_selection);
  context::ProfileRepository repository(configuration_->profiles.profiles);
  auto& profile = configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
  if (!repository.add_criterion(profile.id, {property, mode, value})) {
    ::MessageBoxW(window_, L"Enter a valid match criterion.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh_criteria();
  ::SendDlgItemMessageW(window_, profile_criteria_id, LB_SETCURSEL, profile.criteria.size() - 1, 0);
}

void ProfileEditor::update_criterion() {
  const int selected = profile_index(), criterion_slot = criterion_index();
  if (selected < 0 || criterion_slot < 0) return;
  const std::string value = read_utf8(window_, profile_value_id);
  const auto property_selection = selected_combo(window_, profile_property_id, property_count);
  const auto mode_selection = selected_combo(window_, profile_mode_id, mode_count);
  if (!property_selection || !mode_selection) return;
  const auto property = static_cast<context::ApplicationProperty>(*property_selection);
  const auto mode = static_cast<context::MatchMode>(*mode_selection);
  context::ProfileRepository repository(configuration_->profiles.profiles);
  const auto& id = configuration_->profiles.profiles[static_cast<std::size_t>(selected)].id;
  if (!repository.replace_criterion(id, static_cast<std::size_t>(criterion_slot),
                                    {property, mode, value})) {
    ::MessageBoxW(window_, L"Enter a valid match criterion.", L"Strokes++", MB_OK | MB_ICONERROR);
    return;
  }
  refresh_criteria();
  ::SendDlgItemMessageW(window_, profile_criteria_id, LB_SETCURSEL, criterion_slot, 0);
}

void ProfileEditor::remove_criterion() {
  const int selected = profile_index(), criterion_slot = criterion_index();
  if (selected < 0 || criterion_slot < 0) return;
  context::ProfileRepository repository(configuration_->profiles.profiles);
  const auto& id = configuration_->profiles.profiles[static_cast<std::size_t>(selected)].id;
  (void)repository.remove_criterion(id, static_cast<std::size_t>(criterion_slot));
  refresh_criteria();
  refresh();
  ::SendDlgItemMessageW(window_, profiles_id, LB_SETCURSEL, selected, 0);
}

void ProfileEditor::assign(const std::string& gesture_id) {
  const int selected = profile_index();
  if (selected < 0 || gesture_id.empty()) return;
  context::ProfileRepository repository(configuration_->profiles.profiles);
  auto& profile = configuration_->profiles.profiles[static_cast<std::size_t>(selected)];
  const auto existing = profile.actions_by_gesture.find(gesture_id);
  WindowsActionEditor editor;
  auto result = editor.edit(instance_, window_,
                            existing == profile.actions_by_gesture.end() ? nullptr
                                                                         : &existing->second);
  if (!result.accepted) return;
  if (result.action)
    (void)repository.set_action(profile.id, gesture_id, std::move(*result.action));
  else
    (void)repository.remove_action(profile.id, gesture_id);
  load(gesture_id);
}

}  // namespace strokes::ui
