#pragma once

#include "actions/action.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace strokes::context {

enum class ApplicationProperty {
    process_name,
    window_title,
    window_class,
};

enum class MatchMode {
    exact,
    contains,
    regex,
};

struct MatchCriterion {
    ApplicationProperty property{ApplicationProperty::process_name};
    MatchMode mode{MatchMode::exact};
    std::string value;
};

struct ApplicationProfile {
    std::string id;
    std::string name;
    bool enabled{true};
    std::vector<MatchCriterion> criteria;
    std::unordered_map<std::string, actions::Action> actions_by_gesture;
};

}  // namespace strokes::context
