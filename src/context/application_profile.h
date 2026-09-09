#pragma once

#include <optional>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "actions/action.h"

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
  std::optional<std::regex> compiled_regex;
};

struct ApplicationProfile {
  std::string id;
  std::string name;
  bool enabled{true};
  std::vector<MatchCriterion> criteria;
  std::unordered_map<std::string, actions::Action> actions_by_gesture;
};

}  // namespace strokes::context
