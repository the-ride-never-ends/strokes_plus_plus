#include <utility>
#include <vector>

#include "actions/action_resolver.h"
#include "context/profile_matcher.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using actions::Action;
using actions::ActionResolver;
using actions::ActionSource;
using context::ApplicationContext;
using context::ApplicationProfile;
using context::ApplicationProperty;
using context::MatchCriterion;
using context::MatchMode;
using context::ProfileMatcher;

const ApplicationContext chrome{123, 456, "chrome.exe", "GitHub - Google Chrome",
                                "Chrome_WidgetWin_1"};

ApplicationProfile profile_with(MatchCriterion criterion) {
  (void)context::prepare_criterion(criterion);
  return {"chrome", "Chrome", true, {std::move(criterion)}, {}};
}

void criterion_tests() {
  ProfileMatcher matcher;
  check(matcher.matches(
            profile_with({ApplicationProperty::process_name, MatchMode::exact, "CHROME.EXE"}),
            chrome),
        "process exact match is case-insensitive");
  check(!matcher.matches(
            profile_with({ApplicationProperty::process_name, MatchMode::exact, "notepad.exe"}),
            chrome),
        "different process does not match");
  check(matcher.matches(profile_with({ApplicationProperty::window_title, MatchMode::exact,
                                      "GitHub - Google Chrome"}),
                        chrome),
        "exact title matches");
  check(
      matcher.matches(
          profile_with({ApplicationProperty::window_title, MatchMode::contains, "github"}), chrome),
      "contained title text matches");
  check(matcher.matches(profile_with({ApplicationProperty::window_class, MatchMode::exact,
                                      "Chrome_WidgetWin_1"}),
                        chrome),
        "window class matches");
  check(matcher.matches(profile_with({ApplicationProperty::window_title, MatchMode::regex,
                                      R"(^GitHub.*Chrome$)"}),
                        chrome),
        "regular expression matches");
  check(
      !matcher.matches(
          profile_with({ApplicationProperty::window_title, MatchMode::regex, "[invalid"}), chrome),
      "invalid regular expression safely fails");
  const ApplicationContext unicode{1, 2, "app.exe", "Übersicht", "AppClass"};
  check(matcher.matches(
            profile_with({ApplicationProperty::window_title, MatchMode::exact, "übersicht"}),
            unicode),
        "non-ASCII window titles match case-insensitively in the user locale");
}

void combined_and_disabled_tests() {
  ProfileMatcher matcher;
  ApplicationProfile profile{"chrome",
                             "Chrome",
                             true,
                             {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"},
                              {ApplicationProperty::window_title, MatchMode::contains, "GitHub"}},
                             {}};
  check(matcher.matches(profile, chrome), "all configured criteria can match");
  profile.criteria.push_back({ApplicationProperty::window_class, MatchMode::exact, "WrongClass"});
  check(!matcher.matches(profile, chrome), "one failed required criterion rejects profile");
  profile.enabled = false;
  check(!matcher.matches(profile, chrome), "disabled profile is excluded");

  ApplicationProfile empty{"empty", "Empty", true, {}, {}};
  check(!matcher.matches(empty, chrome),
        "profile without criteria does not match every application");
}

void resolution_tests() {
  const Action profile_action{actions::ActionType::keyboard_shortcut, "CTRL+SHIFT+TAB"};
  const Action global_action{actions::ActionType::keyboard_shortcut, "ALT+LEFT"};
  ApplicationProfile profile =
      profile_with({ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"});
  profile.actions_by_gesture.emplace("left", profile_action);
  const std::vector profiles{profile};
  const ActionResolver::GlobalActions globals{{"left", global_action}};

  auto resolved = ActionResolver::resolve("left", chrome, profiles, globals);
  check(resolved && resolved->action == profile_action, "profile action overrides global action");
  check(resolved && resolved->source == ActionSource::application_profile,
        "profile action reports its source");
  check(resolved && resolved->profile_id == "chrome", "resolved action reports profile id");

  const ApplicationContext notepad{1, 2, "notepad.exe", "Notes", "Notepad"};
  resolved = ActionResolver::resolve("left", notepad, profiles, globals);
  check(resolved && resolved->action == global_action, "global action is fallback");
  check(resolved && resolved->source == ActionSource::global, "global source is reported");
  check(!ActionResolver::resolve("unknown", chrome, profiles, globals),
        "missing mapping resolves no action");

  auto matching_without_override = profile;
  matching_without_override.actions_by_gesture.clear();
  resolved = ActionResolver::resolve("left", chrome, {matching_without_override}, globals);
  check(resolved && resolved->action == global_action,
        "matching profile without gesture override falls back globally");

  auto second_profile = profile;
  second_profile.id = "chrome-second";
  second_profile.actions_by_gesture["left"].value = "CTRL+2";
  const std::vector precedence_profiles{profile, second_profile};
  resolved = ActionResolver::resolve("left", chrome, precedence_profiles, globals);
  check(resolved && resolved->profile_id == "chrome" && resolved->action == profile_action,
        "first matching profile in configuration order has precedence");
}

}  // namespace

void run_profile_tests() {
  criterion_tests();
  combined_and_disabled_tests();
  resolution_tests();
}

}  // namespace strokes::tests
