#pragma once

#include "context/application_context.h"
#include "context/application_profile.h"

namespace strokes::context {

class ProfileMatcher {
public:
    [[nodiscard]] bool matches(
        const ApplicationProfile& profile,
        const ApplicationContext& application) const;

private:
    [[nodiscard]] static bool matches_criterion(
        const MatchCriterion& criterion,
        const ApplicationContext& application);
};

}  // namespace strokes::context
