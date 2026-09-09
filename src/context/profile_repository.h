#pragma once
#include <cstddef>
#include <string>
#include <vector>

#include "context/application_profile.h"

namespace strokes::context {
/// Applies validated identity-safe edits to application profiles.
class ProfileRepository {
 public:
  explicit ProfileRepository(std::vector<ApplicationProfile>& profiles) : profiles_(profiles) {}
  [[nodiscard]] bool create(std::string id, std::string name);
  [[nodiscard]] bool rename(const std::string& id, std::string name);
  [[nodiscard]] bool set_enabled(const std::string& id, bool enabled);
  [[nodiscard]] bool erase(const std::string& id);
  [[nodiscard]] bool add_criterion(const std::string& id, MatchCriterion criterion);
  [[nodiscard]] bool replace_criterion(const std::string& id, std::size_t index,
                                       MatchCriterion criterion);
  [[nodiscard]] bool remove_criterion(const std::string& id, std::size_t index);
  [[nodiscard]] bool set_action(const std::string& id, std::string gesture_id,
                                actions::Action action);
  [[nodiscard]] bool remove_action(const std::string& id, const std::string& gesture_id);
  [[nodiscard]] ApplicationProfile* find(const std::string& id) noexcept;

 private:
  std::vector<ApplicationProfile>& profiles_;
};
}  // namespace strokes::context
