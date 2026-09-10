#include "context/profile_repository.h"

#include <algorithm>
#include <utility>

#include "actions/keyboard_shortcut.h"
#include "repository_helpers.h"

namespace strokes::context {
namespace {

bool valid_action(const actions::Action& action) {
  return action.type == actions::ActionType::keyboard_shortcut &&
         actions::parse_shortcut_sequence(action.value).has_value();
}

}  // namespace

ApplicationProfile* ProfileRepository::find(const std::string& id) noexcept {
  return detail::find_id(profiles_, id);
}

bool ProfileRepository::create(std::string id, std::string name) {
  if (!detail::unique_id(profiles_, id) || !detail::unique_name(profiles_, name)) {
    return false;
  }
  profiles_.push_back({std::move(id), std::move(name), false, {}, {}});
  return true;
}

bool ProfileRepository::rename(const std::string& id, std::string name) {
  auto* profile = find(id);
  if (profile == nullptr || !detail::unique_name(profiles_, name, &id)) {
    return false;
  }
  profile->name = std::move(name);
  return true;
}

bool ProfileRepository::set_enabled(const std::string& id, bool enabled) {
  auto* profile = find(id);
  if (profile == nullptr || (enabled && profile->criteria.empty())) return false;
  profile->enabled = enabled;
  return true;
}

bool ProfileRepository::erase(const std::string& id) { return detail::erase_id(profiles_, id); }

bool ProfileRepository::add_criterion(const std::string& id, MatchCriterion criterion) {
  auto* profile = find(id);
  if (profile == nullptr || !prepare_criterion(criterion)) return false;
  profile->criteria.push_back(std::move(criterion));
  return true;
}

bool ProfileRepository::replace_criterion(const std::string& id, std::size_t index,
                                          MatchCriterion criterion) {
  auto* profile = find(id);
  if (profile == nullptr || index >= profile->criteria.size() || !prepare_criterion(criterion)) {
    return false;
  }
  profile->criteria[index] = std::move(criterion);
  return true;
}

bool ProfileRepository::remove_criterion(const std::string& id, std::size_t index) {
  auto* profile = find(id);
  if (profile == nullptr || index >= profile->criteria.size()) return false;
  profile->criteria.erase(profile->criteria.begin() + static_cast<std::ptrdiff_t>(index));
  if (profile->criteria.empty()) profile->enabled = false;
  return true;
}

bool ProfileRepository::set_action(const std::string& id, std::string gesture_id,
                                   actions::Action action) {
  auto* profile = find(id);
  if (profile == nullptr || gesture_id.empty() || !valid_action(action)) return false;
  profile->actions_by_gesture.insert_or_assign(std::move(gesture_id), std::move(action));
  return true;
}

bool ProfileRepository::remove_action(const std::string& id, const std::string& gesture_id) {
  auto* profile = find(id);
  return profile != nullptr && profile->actions_by_gesture.erase(gesture_id) == 1;
}

}  // namespace strokes::context
