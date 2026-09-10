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
  check(loaded.value->gestures.gestures.size() == 3 &&
            loaded.value->gestures.gestures[1].id == "minimize" &&
            loaded.value->gestures.gestures[2].id == "maximize" &&
            loaded.value->profiles.global_actions.at("minimize").value == "ALT+SPACE,N" &&
            loaded.value->profiles.global_actions.at("maximize").value == "WIN+UP",
        "defaults include editable diagonal minimize and maximize gestures");
  loaded.value->global.gestures_enabled = false;
  loaded.value->global.movement_threshold = 17;
  loaded.value->profiles.global_actions.at("right").value = "CTRL+W";
  loaded.value->profiles.global_actions.at("minimize").value = "ALT+F9";
  std::string error;
  loaded.value->gestures.gestures.push_back(
      {"down", "Down", true, {{"down-1", {{4, 2}, {4, 20}}}}});
  loaded.value->profiles.profiles.push_back(
      {"notes",
       "Notes",
       true,
       {{context::ApplicationProperty::process_name, context::MatchMode::contains, "notepad"}},
       {{"down", {actions::ActionType::keyboard_shortcut, "CTRL+S"}}}});
  check(store.save(*loaded.value, error), "configuration bundle saves atomically");
  auto reloaded = store.load();
  check(reloaded && !reloaded.value->global.gestures_enabled &&
            reloaded.value->global.movement_threshold == 17,
        "global options persist across reload");
  check(reloaded && reloaded.value->profiles.global_actions.at("right").value == "CTRL+W",
        "action mappings persist across reload");
  check(reloaded && reloaded.value->profiles.global_actions.at("minimize").value == "ALT+F9",
        "the minimize gesture shortcut remains user-configurable across reloads");
  check(reloaded && reloaded.value->gestures.gestures.size() == 4 &&
            reloaded.value->gestures.gestures[3].templates[0].points.size() == 2,
        "gesture definitions and templates persist across reload");
  check(reloaded && reloaded.value->profiles.profiles.size() == 1 &&
            reloaded.value->profiles.profiles[0].actions_by_gesture.at("down").value == "CTRL+S",
        "profiles, criteria, and overrides persist across reload");
}
void malformed_and_recovery() {
  TemporaryDirectory temp;
  config::ConfigurationStore store(temp.path);
  auto loaded = store.load();
  check(static_cast<bool>(loaded), "recovery test starts with a valid configuration");
  if (!loaded) return;
  loaded.value->gestures.gestures.push_back(
      {"down", "Down", true, {{"sample", {{0, 0}, {0, 20}}}}});
  std::string save_error;
  check(store.save(*loaded.value, save_error), "test configuration saves before corruption");
  {
    std::ofstream out(temp.path / "config.json", std::ios::trunc);
    out << "{bad";
  }
  auto bad = store.load();
  check(bad && bad.warnings.find("config.json") != std::string::npos,
        "malformed global configuration is quarantined and reported");
  check(bad && bad.value->gestures.gestures.size() == 4,
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
  interrupted_bundle_transaction();
  interrupted_first_save();
}
}  // namespace strokes::tests
