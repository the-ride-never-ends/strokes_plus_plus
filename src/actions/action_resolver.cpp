#include "actions/action_resolver.h"

namespace strokes::actions {

std::optional<ResolvedAction> ActionResolver::resolve(
    const std::string& gesture_id,
    const context::ApplicationContext& application,
    const std::vector<context::ApplicationProfile>& profiles,
    const GlobalActions& global_actions,
    const context::ProfileMatcher& matcher) {
    for (const auto& profile : profiles) {
        if (!matcher.matches(profile, application)) {
            continue;
        }
        if (const auto action = profile.actions_by_gesture.find(gesture_id);
            action != profile.actions_by_gesture.end()) {
            return ResolvedAction{action->second, ActionSource::application_profile, profile.id};
        }
    }

    if (const auto action = global_actions.find(gesture_id); action != global_actions.end()) {
        return ResolvedAction{action->second, ActionSource::global, {}};
    }
    return std::nullopt;
}

}  // namespace strokes::actions
