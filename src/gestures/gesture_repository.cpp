#include "gestures/gesture_repository.h"

#include <algorithm>
#include <utility>

#include "gestures/normalizer.h"
#include "repository_helpers.h"

namespace strokes::gestures {

GestureDefinition* GestureRepository::find(const std::string& id) noexcept {
  return detail::find_id(gestures_, id);
}

bool GestureRepository::create(std::string id, std::string name) {
  if (!detail::unique_id(gestures_, id) || !detail::unique_name(gestures_, name)) return false;
  gestures_.push_back({std::move(id), std::move(name), false, {}});
  return true;
}

bool GestureRepository::rename(const std::string& id, std::string name) {
  auto* gesture = find(id);
  if (gesture == nullptr || !detail::unique_name(gestures_, name, &id)) return false;
  gesture->name = std::move(name);
  return true;
}

bool GestureRepository::set_enabled(const std::string& id, bool enabled) {
  auto* gesture = find(id);
  if (gesture == nullptr || (enabled && gesture->templates.empty())) return false;
  gesture->enabled = enabled;
  return true;
}

bool GestureRepository::erase(const std::string& id) { return detail::erase_id(gestures_, id); }

bool GestureRepository::add_template(const std::string& id, GestureTemplate value) {
  auto* gesture = find(id);
  if (gesture == nullptr || value.id.empty() || !StrokeNormalizer{}.normalize(value.points) ||
      std::ranges::any_of(gesture->templates, [&](const auto& gesture_template) {
        return gesture_template.id == value.id;
      })) {
    return false;
  }
  gesture->templates.push_back(std::move(value));
  return true;
}

bool GestureRepository::remove_template(const std::string& id, const std::string& template_id) {
  auto* gesture = find(id);
  if (gesture == nullptr) return false;
  const bool removed = std::erase_if(gesture->templates, [&](const auto& gesture_template) {
                         return gesture_template.id == template_id;
                       }) == 1;
  if (removed && gesture->templates.empty()) gesture->enabled = false;
  return removed;
}

}  // namespace strokes::gestures
