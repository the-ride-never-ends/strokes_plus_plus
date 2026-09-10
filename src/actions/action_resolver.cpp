#include "actions/action_resolver.h"

namespace strokes::actions {

std::optional<ResolvedAction> ActionResolver::resolve(
    const std::string& gesture_id, const context::ApplicationContext& application,
    const std::vector<context::ApplicationProfile>& profiles, const GlobalActions& global_actions,
    const context::ProfileMatcher& matcher) {
  // MVP.md 10.5 defines two steps: the matching application profile, then the
  // global mapping. The first profile that matches is therefore authoritative
  // even when it has no entry for this gesture; a later profile must not supply
  // one on its behalf.
  for (const auto& profile : profiles) {
    if (!matcher.matches(profile, application)) {
      continue;
    }
    if (const auto action = profile.actions_by_gesture.find(gesture_id);
        action != profile.actions_by_gesture.end()) {
      return ResolvedAction{action->second, ActionSource::application_profile, profile.id};
    }
    break;
  }

  if (const auto action = global_actions.find(gesture_id); action != global_actions.end()) {
    return ResolvedAction{action->second, ActionSource::global, {}};
  }
  return std::nullopt;
}

}  // namespace strokes::actions
