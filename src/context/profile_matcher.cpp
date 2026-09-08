#include "context/profile_matcher.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <regex>
#include <string_view>

namespace strokes::context {
namespace {

std::string lowercase(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    std::ranges::transform(text, std::back_inserter(result), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return result;
}

std::string_view property_value(
    ApplicationProperty property,
    const ApplicationContext& application) {
    switch (property) {
    case ApplicationProperty::process_name:
        return application.executable_name;
    case ApplicationProperty::window_title:
        return application.window_title;
    case ApplicationProperty::window_class:
        return application.window_class;
    }
    return {};
}

}  // namespace

bool ProfileMatcher::matches(
    const ApplicationProfile& profile,
    const ApplicationContext& application) const {
    if (!profile.enabled || profile.criteria.empty()) {
        return false;
    }
    return std::ranges::all_of(profile.criteria, [&](const MatchCriterion& criterion) {
        return matches_criterion(criterion, application);
    });
}

bool ProfileMatcher::matches_criterion(
    const MatchCriterion& criterion,
    const ApplicationContext& application) {
    if (criterion.value.empty()) {
        return false;
    }

    const std::string_view actual = property_value(criterion.property, application);
    switch (criterion.mode) {
    case MatchMode::exact:
        return lowercase(actual) == lowercase(criterion.value);
    case MatchMode::contains:
        return lowercase(actual).find(lowercase(criterion.value)) != std::string::npos;
    case MatchMode::regex:
        try {
            return std::regex_search(
                actual.begin(), actual.end(),
                std::regex(criterion.value, std::regex::ECMAScript | std::regex::icase));
        } catch (const std::regex_error&) {
            return false;
        }
    }
    return false;
}

}  // namespace strokes::context
