#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "actions/action.h"
#include "context/application_context.h"
#include "context/application_profile.h"
#include "context/profile_matcher.h"

namespace strokes::actions {

enum class ActionSource {
  application_profile,
  global,
};

struct ResolvedAction {
  Action action;
  ActionSource source{ActionSource::global};
  std::string profile_id;
};

/// Resolves a recognized gesture to the first matching profile action or global fallback.
class ActionResolver {
 public:
  using GlobalActions = std::unordered_map<std::string, Action>;

  [[nodiscard]] static std::optional<ResolvedAction> resolve(
      const std::string& gesture_id, const context::ApplicationContext& application,
      const std::vector<context::ApplicationProfile>& profiles, const GlobalActions& global_actions,
      const context::ProfileMatcher& matcher = {});
};

}  // namespace strokes::actions
