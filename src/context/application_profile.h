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

/// Validates a criterion and caches its regular expression when required.
[[nodiscard]] inline bool prepare_criterion(MatchCriterion& criterion) {
  if (criterion.value.empty()) return false;
  criterion.compiled_regex.reset();
  if (criterion.mode != MatchMode::regex) return true;
  try {
    criterion.compiled_regex.emplace(criterion.value, std::regex::ECMAScript | std::regex::icase);
    return true;
  } catch (const std::regex_error&) {
    return false;
  }
}

struct ApplicationProfile {
  std::string id;
  std::string name;
  bool enabled{true};
  std::vector<MatchCriterion> criteria;
  std::unordered_map<std::string, actions::Action> actions_by_gesture;
};

}  // namespace strokes::context
