#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "config/configuration_codec.h"
#include "config/configuration_store.h"
#include "config/json.h"
#include "test_support.h"

namespace strokes::tests {
namespace {
struct TemporaryDirectory {
  std::filesystem::path path;
  TemporaryDirectory() {
    path = std::filesystem::temp_directory_path() /
           ("strokes-plus-plus-store-tests-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
  ~TemporaryDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
  }
};
void creation_and_persistence() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto loaded = store.load();
  check(static_cast<bool>(loaded), "missing configuration files are created");
  if (!loaded) return;
  check(std::filesystem::exists(temp.path / "config.json") &&
            std::filesystem::exists(temp.path / "gestures.json") &&
            std::filesystem::exists(temp.path / "profiles.json"),
        "all three configuration files are created");
  check(loaded.value->gestures.gestures.size() == 41 &&
            loaded.value->gestures.gestures[0].name == "Diagonal Down-Left" &&
            loaded.value->gestures.gestures[1].name == "Diagonal Up-Right" &&
            loaded.value->gestures.gestures[30].name == "Up Right / Down Left" &&
            loaded.value->gestures.gestures[30].enabled &&
            loaded.value->profiles.global_actions.at("slash-down").type ==
                actions::ActionType::window &&
            std::get<actions::WindowParameters>(
                loaded.value->profiles.global_actions.at("slash-down").parameters)
                    .operation == actions::WindowOperation::minimize &&
            std::get<actions::WindowParameters>(
                loaded.value->profiles.global_actions.at("slash-up").parameters)
                    .operation == actions::WindowOperation::toggle_maximize_restore &&
            loaded.value->profiles.global_actions.size() == 22 &&
            loaded.value->profiles.profiles.size() == 2,
        "defaults include the complete direction-named gesture catalog with separate actions");
  loaded.value->global.gestures_enabled = false;
  loaded.value->global.movement_threshold = 17;
  *actions::keyboard_shortcut(loaded.value->profiles.global_actions.at("up")) = "CTRL+W";
  loaded.value->profiles.global_actions.at("slash-down") =
      actions::ActionDefinition::keyboard("ALT+F9");
  const actions::ActionDefinition process_action{
      1, actions::ActionType::process,
      actions::ProcessParameters{actions::ProcessOperation::launch, "tool.exe", "--flag",
                                 "C:\\Tools"}};
  const actions::ActionDefinition mouse_action{
      1, actions::ActionType::mouse,
      actions::MouseParameters{actions::MouseOperation::click, actions::MouseButton::left,
                               {actions::PositionTarget::gesture_start, std::nullopt}}};
  loaded.value->profiles.global_actions.emplace("process", process_action);
  loaded.value->profiles.global_actions.emplace("mouse", mouse_action);
  std::string error;
  loaded.value->gestures.gestures.push_back(
      {"custom-down", "Custom Down", true, {{"custom-down-1", {{4, 2}, {4, 20}}}}});
  loaded.value->profiles.profiles.push_back(
      {"notes",
       "Notes",
       true,
       {{context::ApplicationProperty::process_name, context::MatchMode::contains, "notepad"}},
       {{"custom-down", actions::ActionDefinition::keyboard("CTRL+S")}}});
  check(store.save(*loaded.value, error), "configuration bundle saves atomically");
  auto reloaded = store.load();
  check(reloaded && !reloaded.value->global.gestures_enabled &&
            reloaded.value->global.movement_threshold == 17,
        "global options persist across reload");
  check(reloaded && *actions::keyboard_shortcut(
                          reloaded.value->profiles.global_actions.at("up")) == "CTRL+W",
        "action mappings persist across reload");
  check(reloaded && *actions::keyboard_shortcut(
                          reloaded.value->profiles.global_actions.at("slash-down")) == "ALT+F9",
        "the slash-down gesture action remains user-configurable across reloads");
  check(reloaded && reloaded.value->profiles.global_actions.at("process") == process_action &&
            reloaded.value->profiles.global_actions.at("mouse") == mouse_action,
        "generic action mappings and optional parameters survive save, restart, and reload");
  check(reloaded && reloaded.value->gestures.gestures.size() == 42 &&
            reloaded.value->gestures.gestures[41].templates[0].points.size() == 2,
        "gesture definitions and templates persist across reload");
  check(reloaded && reloaded.value->profiles.profiles.size() == 3 &&
            reloaded.value->profiles.profiles.back().id == "notes" &&
            *actions::keyboard_shortcut(
                reloaded.value->profiles.profiles.back().actions_by_gesture.at("custom-down")) ==
                "CTRL+S",
        "profiles, criteria, and overrides persist across reload");
}
void malformed_and_recovery() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto loaded = store.load();
  check(static_cast<bool>(loaded), "recovery test starts with a valid configuration");
  if (!loaded) return;
  loaded.value->gestures.gestures.push_back(
      {"recovery-down", "Recovery Down", true, {{"sample", {{0, 0}, {0, 20}}}}});
  std::string save_error;
  check(store.save(*loaded.value, save_error), "test configuration saves before corruption");
  {
    std::ofstream out(temp.path / "config.json", std::ios::trunc);
    out << "{bad";
  }
  auto bad = store.load();
  check(bad && bad.warnings.find("config.json") != std::string::npos,
        "malformed global configuration is quarantined and reported");
  check(bad && bad.value->gestures.gestures.size() == 42,
        "malformed global configuration does not discard valid gestures");
  check(std::filesystem::exists(temp.path / "config.json.invalid"),
        "malformed global configuration is preserved for recovery");
  std::filesystem::remove(temp.path / "config.json");
  {
    std::ofstream out(temp.path / "config.json.bak", std::ios::trunc);
    out << config::json::serialize(config::encode(loaded.value->global));
  }
  auto recovered = store.load();
  check(static_cast<bool>(recovered), "interrupted-save backup is recovered");
}

void legacy_catalog_migration() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto legacy = config::ConfigurationStore::defaults();
  legacy.gestures.version = 1;
  legacy.gestures.gestures.resize(3);
  legacy.gestures.gestures[0].id = "minimize";
  legacy.gestures.gestures[0].name = "Minimize";
  legacy.gestures.gestures[1].id = "maximize";
  legacy.gestures.gestures[1].name = "Maximize";
  legacy.gestures.gestures[2] =
      {"right", "Right", true, {{"legacy-right", {{0, 0}, {100, 0}}}}};
  legacy.profiles.global_actions.insert_or_assign(
      "minimize", std::move(legacy.profiles.global_actions.at("slash-down")));
  legacy.profiles.global_actions.erase("slash-down");
  legacy.profiles.global_actions.insert_or_assign(
      "maximize", std::move(legacy.profiles.global_actions.at("slash-up")));
  legacy.profiles.global_actions.erase("slash-up");
  std::string error;
  check(store.save(legacy, error), "legacy gesture catalog fixture saves");
  const auto migrated = store.load();
  check(migrated && migrated.value->gestures.version == 4 &&
            migrated.value->gestures.gestures.size() == 41,
        "version-one gesture files gain the complete direction-pattern catalog");
  check(migrated && migrated.value->profiles.global_actions.contains("slash-down") &&
            migrated.value->profiles.global_actions.contains("slash-up") &&
            !migrated.value->profiles.global_actions.contains("minimize") &&
            !migrated.value->profiles.global_actions.contains("maximize"),
        "legacy command-named gesture actions follow their direction-named identities");
}

void diagonal_label_migration() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto version_two = config::ConfigurationStore::defaults();
  version_two.gestures.version = 2;
  const std::array old_names{std::pair{"slash-down", "/ Down"},
                             std::pair{"slash-up", "/ Up"},
                             std::pair{"backslash-down", "\\ Down"},
                             std::pair{"backslash-up", "\\ Up"},
                             std::pair{"slash-up-down", "/ Up Down"},
                             std::pair{"backslash-up-down", "\\ Up Down"},
                             std::pair{"down-slash-up", "Down / Up"}};
  for (const auto& [id, name] : old_names) {
    const auto found = std::ranges::find(version_two.gestures.gestures, id,
                                         &gestures::GestureDefinition::id);
    if (found != version_two.gestures.gestures.end()) found->name = name;
  }
  std::string error;
  check(store.save(version_two, error), "version-two diagonal-label fixture saves");
  const auto migrated = store.load();
  check(migrated && migrated.value->gestures.version == 4 &&
            migrated.value->gestures.gestures[0].name == "Diagonal Down-Left" &&
            migrated.value->gestures.gestures[1].name == "Diagonal Up-Right" &&
            migrated.value->gestures.gestures[30].name == "Up Right / Down Left",
        "version-two slash labels migrate to explicit diagonal directions");
}

void default_action_migration() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto previous = config::ConfigurationStore::defaults();
  previous.profiles.version = 1;
  previous.profiles.global_actions.clear();
  previous.profiles.global_actions.emplace("right",
                                           actions::ActionDefinition::keyboard("CTRL+SHIFT+R"));
  std::string error;
  check(store.save(previous, error), "previous default-action fixture saves");
  const auto migrated = store.load();
  check(migrated && migrated.value->profiles.version == 3 &&
            migrated.value->profiles.global_actions.size() == 22 &&
            migrated.value->profiles.profiles.size() == 2,
        "older profiles gain the StrokesPlus.net defaults and application overrides");
  check(migrated && migrated.value->profiles.global_actions.at("right").type ==
                        actions::ActionType::media,
        "default-action migration replaces the previous built-in assignment set");
}
void interrupted_bundle_transaction() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto original = store.load();
  check(static_cast<bool>(original), "transaction test starts with a valid configuration");
  if (!original) return;
  original.value->global.movement_threshold = 21;
  std::string error;
  check(store.save(*original.value, error), "baseline bundle saves before interruption simulation");
  for (const auto* name : {"config.json", "gestures.json", "profiles.json"}) {
    const auto path = temp.path / name;
    auto backup = path;
    backup += ".bak";
    std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
  }
  {
    std::ofstream marker(temp.path / ".configuration-transaction");
    marker << "pending\n";
  }
  {
    std::ofstream changed(temp.path / "config.json", std::ios::trunc);
    changed << "{}";
  }
  auto recovered = store.load();
  check(recovered && recovered.value->global.movement_threshold == 21,
        "interrupted bundle transaction rolls every component back together");
  check(!std::filesystem::exists(temp.path / ".configuration-transaction"),
        "transaction marker is removed after rollback");
}

void interrupted_first_save() {
  TemporaryDirectory temp;
  std::filesystem::create_directories(temp.path);
  {
    std::ofstream marker(temp.path / ".configuration-transaction");
    marker << "pending\n";
  }
  {
    std::ofstream sentinel(temp.path / "config.json.bak.missing");
    sentinel << "missing\n";
  }
  {
    std::ofstream partial(temp.path / "config.json");
    partial << "{}";
  }
  config::ConfigurationStore store(temp.path);
  const auto recovered = store.load();
  check(static_cast<bool>(recovered), "an interrupted first save is recovered");
  check(recovered && recovered.value->global.movement_threshold == 8.0,
        "a partially committed new file is removed before defaults are created");
  check(!std::filesystem::exists(temp.path / "config.json.bak.missing"),
        "missing-file transaction sentinel is removed after rollback");
}
}  // namespace
void run_configuration_store_tests() {
  creation_and_persistence();
  malformed_and_recovery();
  legacy_catalog_migration();
  diagonal_label_migration();
  default_action_migration();
  interrupted_bundle_transaction();
  interrupted_first_save();
}
}  // namespace strokes::tests
