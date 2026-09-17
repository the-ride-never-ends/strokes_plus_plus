#include "config/configuration_store.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string_view>
#include <utility>

#include "config/configuration_codec.h"
#include "config/json.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace strokes::config {
namespace {

bool recover_write(const std::filesystem::path& path, std::string& error) {
  std::error_code ec;
  auto backup = path;
  backup += ".bak";
  const bool path_exists = std::filesystem::exists(path, ec);
  if (ec) {
    error = "cannot inspect " + path.string() + ": " + ec.message();
    return false;
  }
  const bool backup_exists = std::filesystem::exists(backup, ec);
  if (ec) {
    error = "cannot inspect " + backup.string() + ": " + ec.message();
    return false;
  }
  if (!path_exists && backup_exists) {
    std::filesystem::rename(backup, path, ec);
    if (ec) {
      error = "cannot recover " + path.string() + ": " + ec.message();
      return false;
    }
  }
  return true;
}

bool write_file(const std::filesystem::path& path, const std::string& text, std::string& error) {
  std::FILE* output = nullptr;
#ifdef _WIN32
  if (_wfopen_s(&output, path.c_str(), L"wb") != 0) output = nullptr;
#else
  output = std::fopen(path.c_str(), "wb");
#endif
  if (output == nullptr) {
    error = "cannot open " + path.string();
    return false;
  }
  const bool written = std::fwrite(text.data(), 1, text.size(), output) == text.size();
  const bool flushed = std::fflush(output) == 0;
#ifdef _WIN32
  const bool committed = _commit(_fileno(output)) == 0;
#else
  const bool committed = fsync(fileno(output)) == 0;
#endif
  const bool closed = std::fclose(output) == 0;
  if (!written || !flushed || !committed || !closed) {
    error = "cannot write " + path.string();
    return false;
  }
  return true;
}

bool atomic_write(const std::filesystem::path& path, const std::string& text, std::string& error) {
  auto temporary = path;
  temporary += ".tmp";
  auto backup = path;
  backup += ".bak";
  if (!write_file(temporary, text, error)) return false;

  std::error_code ec;
  const bool existed = std::filesystem::exists(path, ec);
  if (ec) {
    error = "cannot inspect " + path.string() + ": " + ec.message();
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    return false;
  }
  if (existed) {
    std::filesystem::remove(backup, ec);
    ec.clear();
    std::filesystem::rename(path, backup, ec);
    if (ec) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
      error = "cannot stage " + path.string() + ": " + ec.message();
      return false;
    }
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::error_code ignored;
    if (existed) std::filesystem::rename(backup, path, ignored);
    std::filesystem::remove(temporary, ignored);
    error = "cannot replace " + path.string() + ": " + ec.message();
    return false;
  }
  std::filesystem::remove(backup, ec);
  return true;
}

template <std::size_t Size>
bool recover_transaction(const std::filesystem::path& directory,
                         const std::array<std::filesystem::path, Size>& paths, std::string& error) {
  const auto marker = directory / ".configuration-transaction";
  std::error_code ec;
  const bool interrupted = std::filesystem::exists(marker, ec);
  if (ec) {
    error = "cannot inspect configuration transaction: " + ec.message();
    return false;
  }
  if (!interrupted) return true;
  for (const auto& path : paths) {
    auto backup = path;
    backup += ".bak";
    auto missing = path;
    missing += ".bak.missing";
    auto temporary = path;
    temporary += ".tmp";
    const bool backup_exists = std::filesystem::exists(backup, ec);
    if (ec) {
      error = "cannot inspect " + backup.string() + ": " + ec.message();
      return false;
    }
    const bool was_missing = std::filesystem::exists(missing, ec);
    if (ec) {
      error = "cannot inspect " + missing.string() + ": " + ec.message();
      return false;
    }
    if (backup_exists) {
      std::filesystem::remove(path, ec);
      ec.clear();
      std::filesystem::rename(backup, path, ec);
      if (ec) {
        error = "cannot roll back " + path.string() + ": " + ec.message();
        return false;
      }
    } else if (was_missing) {
      std::filesystem::remove(path, ec);
      if (ec) {
        error = "cannot remove partial " + path.string() + ": " + ec.message();
        return false;
      }
    }
    ec.clear();
    std::filesystem::remove(temporary, ec);
    ec.clear();
    std::filesystem::remove(missing, ec);
  }
  ec.clear();
  std::filesystem::remove(marker, ec);
  if (ec) {
    error = "cannot finish configuration rollback: " + ec.message();
    return false;
  }
  return true;
}

template <std::size_t Size>
bool transactional_write(const std::filesystem::path& directory,
                         const std::array<std::filesystem::path, Size>& paths,
                         const std::array<std::string, Size>& texts, std::string& error) {
  const auto marker = directory / ".configuration-transaction";
  if (!write_file(marker, "pending\n", error)) return false;

  std::array<bool, Size> backed_up{};
  std::array<bool, Size> marked_missing{};
  std::array<bool, Size> committed{};
  std::error_code ec;
  const auto rollback = [&] {
    bool succeeded = true;
    std::string rollback_error;
    const auto record_error = [&](const std::filesystem::path& path) {
      if (!ec) return;
      succeeded = false;
      if (rollback_error.empty())
        rollback_error = "cannot roll back " + path.string() + ": " + ec.message();
      ec.clear();
    };
    for (std::size_t index = 0; index < Size; ++index) {
      auto temporary = paths[index];
      temporary += ".tmp";
      if (committed[index]) std::filesystem::remove(paths[index], ec);
      record_error(paths[index]);
      auto backup = paths[index];
      backup += ".bak";
      if (backed_up[index]) std::filesystem::rename(backup, paths[index], ec);
      record_error(backup);
      auto missing = paths[index];
      missing += ".bak.missing";
      if (marked_missing[index]) std::filesystem::remove(missing, ec);
      record_error(missing);
      std::filesystem::remove(temporary, ec);
      ec.clear();
    }
    if (succeeded) {
      std::filesystem::remove(marker, ec);
      record_error(marker);
    }
    if (!succeeded) error += "; " + rollback_error + "; recovery marker retained";
    return succeeded;
  };

  for (std::size_t index = 0; index < Size; ++index) {
    auto temporary = paths[index];
    temporary += ".tmp";
    if (!write_file(temporary, texts[index], error)) {
      rollback();
      return false;
    }
  }
  for (std::size_t index = 0; index < Size; ++index) {
    auto backup = paths[index];
    backup += ".bak";
    std::filesystem::remove(backup, ec);
    ec.clear();
    const bool exists = std::filesystem::exists(paths[index], ec);
    if (ec) {
      error = "cannot inspect " + paths[index].string() + ": " + ec.message();
      rollback();
      return false;
    }
    if (exists) {
      std::filesystem::rename(paths[index], backup, ec);
      if (ec) {
        error = "cannot stage " + paths[index].string() + ": " + ec.message();
        rollback();
        return false;
      }
      backed_up[index] = true;
    } else {
      auto missing = paths[index];
      missing += ".bak.missing";
      if (!write_file(missing, "missing\n", error)) {
        rollback();
        return false;
      }
      marked_missing[index] = true;
    }
  }
  for (std::size_t index = 0; index < Size; ++index) {
    auto temporary = paths[index];
    temporary += ".tmp";
    std::filesystem::rename(temporary, paths[index], ec);
    if (ec) {
      error = "cannot commit " + paths[index].string() + ": " + ec.message();
      rollback();
      return false;
    }
    committed[index] = true;
  }
  for (std::size_t index = 0; index < Size; ++index) {
    auto backup = paths[index];
    backup += ".bak";
    std::filesystem::remove(backup, ec);
    ec.clear();
    auto missing = paths[index];
    missing += ".bak.missing";
    std::filesystem::remove(missing, ec);
    ec.clear();
  }
  std::filesystem::remove(marker, ec);
  if (ec) {
    error = "configuration committed but transaction cleanup failed: " + ec.message();
    return false;
  }
  return true;
}

std::optional<json::Value> read_json(const std::filesystem::path& path, std::string& error) {
  if (!recover_write(path, error)) return std::nullopt;
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    error = "cannot open " + path.string();
    return std::nullopt;
  }
  std::ostringstream data;
  data << input.rdbuf();
  if (!input.good() && !input.eof()) {
    error = "cannot read " + path.string();
    return std::nullopt;
  }
  auto parsed = json::parse(data.str());
  if (!parsed) {
    error =
        path.string() + ":" + std::to_string(parsed.error->offset) + ": " + parsed.error->message;
    return std::nullopt;
  }
  return std::move(*parsed.value);
}

template <class T>
bool write_encoded(const std::filesystem::path& path, const T& value, std::string& error) {
  return atomic_write(path, json::serialize(encode(value)), error);
}

void append_warning(std::string& warnings, const std::string& warning) {
  if (!warnings.empty()) warnings += "; ";
  warnings += warning;
}

gestures::Stroke cardinal_stroke(std::string_view directions) {
  gestures::Stroke result{{0.0, 0.0}};
  for (const char direction : directions) {
    auto next = result.back();
    switch (direction) {
      case 'D': next.y += 100.0; break;
      case 'L': next.x -= 100.0; break;
      case 'R': next.x += 100.0; break;
      case 'U': next.y -= 100.0; break;
      default: continue;
    }
    result.push_back(next);
  }
  return result;
}

gestures::GestureDefinition built_in(std::string id, std::string name, bool enabled,
                                     gestures::Stroke points) {
  const std::string template_id = "built-in-" + id;
  return {std::move(id), std::move(name), enabled, {{template_id, std::move(points)}}};
}

std::vector<gestures::GestureDefinition> built_in_gestures() {
  std::vector<gestures::GestureDefinition> result;
  const auto add_cardinal = [&](const char* id, const char* name, bool enabled,
                                std::string_view directions) {
    result.push_back(built_in(id, name, enabled, cardinal_stroke(directions)));
  };
  result.push_back(
      built_in("slash-down", "Diagonal Down-Left", true, {{100, 0}, {50, 50}, {0, 100}}));
  result.push_back(
      built_in("slash-up", "Diagonal Up-Right", true, {{0, 100}, {50, 50}, {100, 0}}));
  result.push_back(
      built_in("backslash-down", "Diagonal Down-Right", true,
               {{0, 0}, {50, 50}, {100, 100}}));
  result.push_back(
      built_in("backslash-up", "Diagonal Up-Left", true,
               {{100, 100}, {50, 50}, {0, 0}}));
  add_cardinal("down", "Down", true, "D");
  add_cardinal("down-left", "Down Left", true, "DL");
  add_cardinal("down-left-right", "Down Left Right", true, "DLR");
  add_cardinal("down-right", "Down Right", true, "DR");
  add_cardinal("down-right-up-left", "Down Right Up Left", true, "DRUL");
  add_cardinal("down-up", "Down Up", true, "DU");
  add_cardinal("down-up-down", "Down Up Down", true, "DUD");
  add_cardinal("down-up-down-up", "Down Up Down Up", true, "DUDU");
  add_cardinal("down-up-right-left", "Down Up Right Left", true, "DURL");
  add_cardinal("left", "Left", true, "L");
  add_cardinal("left-down", "Left Down", true, "LD");
  add_cardinal("left-right", "Left Right", true, "LR");
  add_cardinal("left-right-left", "Left Right Left", true, "LRL");
  add_cardinal("left-up", "Left Up", true, "LU");
  add_cardinal("right", "Right", true, "R");
  add_cardinal("right-down", "Right Down", true, "RD");
  add_cardinal("right-left", "Right Left", true, "RL");
  add_cardinal("right-left-right", "Right Left Right", true, "RLR");
  add_cardinal("right-left-right-left", "Right Left Right Left", true, "RLRL");
  add_cardinal("right-up", "Right Up", true, "RU");
  add_cardinal("up", "Up", true, "U");
  add_cardinal("up-down", "Up Down", true, "UD");
  add_cardinal("up-down-up", "Up Down Up", true, "UDU");
  add_cardinal("up-down-up-down", "Up Down Up Down", true, "UDUD");
  add_cardinal("up-left", "Up Left", true, "UL");
  add_cardinal("up-right", "Up Right", true, "UR");
  result.push_back(built_in("slash-up-down", "Diagonal Up-Right Down-Left", false,
                            {{0, 100}, {50, 50}, {100, 0}, {50, 50}, {0, 100}}));
  result.push_back(built_in("backslash-up-down", "Diagonal Up-Left Down-Right", false,
                            {{100, 100}, {50, 50}, {0, 0}, {50, 50}, {100, 100}}));
  result.push_back(built_in("down-slash-up", "Down Diagonal Up-Right", false,
                            {{0, 0}, {0, 100}, {100, 0}}));
  add_cardinal("down-right-left", "Down Right Left", false, "DRL");
  add_cardinal("left-right-down", "Left Right Down", false, "LRD");
  add_cardinal("up-right-left", "Up Right Left", false, "URL");
  return result;
}

void remap_action_key(actions::ActionResolver::GlobalActions& actions, const std::string& old_id,
                      const std::string& new_id) {
  const auto found = actions.find(old_id);
  if (found == actions.end()) return;
  auto action = std::move(found->second);
  actions.erase(found);
  actions.insert_or_assign(new_id, std::move(action));
}

void migrate_gesture_catalog(ConfigurationBundle& configuration) {
  if (configuration.gestures.version >= GestureFile::current_version) return;
  const auto remap = [&](const std::string& old_id, const std::string& new_id,
                         const std::string& new_name) {
    const auto found = std::ranges::find(configuration.gestures.gestures, old_id,
                                         &gestures::GestureDefinition::id);
    if (found != configuration.gestures.gestures.end()) {
      found->id = new_id;
      found->name = new_name;
    }
    remap_action_key(configuration.profiles.global_actions, old_id, new_id);
    for (auto& profile : configuration.profiles.profiles)
      remap_action_key(profile.actions_by_gesture, old_id, new_id);
  };
  remap("minimize", "slash-down", "Diagonal Down-Left");
  remap("maximize", "slash-up", "Diagonal Up-Right");
  for (auto& gesture : built_in_gestures()) {
    const auto found = std::ranges::find(configuration.gestures.gestures, gesture.id,
                                         &gestures::GestureDefinition::id);
    if (found == configuration.gestures.gestures.end()) {
      configuration.gestures.gestures.push_back(std::move(gesture));
    } else if (configuration.gestures.version < 3) {
      const bool old_diagonal_label =
          (found->id == "slash-down" && found->name == "/ Down") ||
          (found->id == "slash-up" && found->name == "/ Up") ||
          (found->id == "backslash-down" && found->name == "\\ Down") ||
          (found->id == "backslash-up" && found->name == "\\ Up") ||
          (found->id == "slash-up-down" && found->name == "/ Up Down") ||
          (found->id == "backslash-up-down" && found->name == "\\ Up Down") ||
          (found->id == "down-slash-up" && found->name == "Down / Up");
      if (old_diagonal_label) found->name = gesture.name;
    }
  }
  configuration.gestures.version = GestureFile::current_version;
}

actions::ActionDefinition window_action(actions::WindowOperation operation) {
  return {actions::ActionDefinition::current_version, actions::ActionType::window,
          actions::WindowParameters{operation, actions::WindowTarget::gesture_window}};
}

actions::ActionDefinition media_action(actions::MediaOperation operation) {
  return {actions::ActionDefinition::current_version, actions::ActionType::media,
          actions::MediaParameters{operation}};
}

actions::ActionDefinition volume_action(actions::VolumeOperation operation,
                                        std::optional<double> amount = std::nullopt) {
  return {actions::ActionDefinition::current_version, actions::ActionType::volume,
          actions::VolumeParameters{operation, amount}};
}

void add_default_global_actions(actions::ActionResolver::GlobalActions& actions) {
  const auto add = [&](const char* gesture, actions::ActionDefinition action) {
    actions.emplace(gesture, std::move(action));
  };
  add("slash-down", window_action(actions::WindowOperation::minimize));
  add("slash-up", window_action(actions::WindowOperation::maximize));
  add("backslash-down", actions::ActionDefinition::keyboard("ALT+F4"));
  add("backslash-up", window_action(actions::WindowOperation::restore));
  add("down", actions::ActionDefinition::keyboard("PAGEDOWN"));
  add("down-left", actions::ActionDefinition::keyboard("END"));
  add("down-left-right", actions::ActionDefinition::keyboard("F5"));
  add("down-right",
      {actions::ActionDefinition::current_version, actions::ActionType::process,
       actions::ProcessParameters{actions::ProcessOperation::launch, "explorer.exe", {}, {}}});
  add("down-right-up-left", actions::ActionDefinition::keyboard("CTRL+A"));
  add("down-up", volume_action(actions::VolumeOperation::mute_toggle));
  add("down-up-down", volume_action(actions::VolumeOperation::decrease, 5.0));
  add("down-up-down-up", volume_action(actions::VolumeOperation::increase, 5.0));
  add("down-up-right-left", actions::ActionDefinition::keyboard("ESC"));
  add("left", actions::ActionDefinition::keyboard("ALT+LEFT"));
  add("left-down", actions::ActionDefinition::keyboard("DELETE"));
  add("left-right", actions::ActionDefinition::keyboard("CTRL+C"));
  add("left-right-left", actions::ActionDefinition::keyboard("CTRL+X"));
  add("left-up", actions::ActionDefinition::keyboard("HOME"));
  add("right", actions::ActionDefinition::keyboard("ALT+RIGHT"));
  add("right-down", actions::ActionDefinition::keyboard("CTRL+V"));
  add("right-left", actions::ActionDefinition::keyboard("CTRL+Z"));
  add("right-left-right", actions::ActionDefinition::keyboard("CTRL+Y"));
  add("right-left-right-left",
      {actions::ActionDefinition::current_version, actions::ActionType::process,
       actions::ProcessParameters{actions::ProcessOperation::launch, "taskmgr.exe", {}, {}}});
  add("right-up", actions::ActionDefinition::keyboard("CTRL+N"));
  add("up", actions::ActionDefinition::keyboard("PAGEUP"));
  add("up-down", media_action(actions::MediaOperation::play_pause));
  add("up-down-up", media_action(actions::MediaOperation::next_track));
  add("up-down-up-down", media_action(actions::MediaOperation::previous_track));
  add("up-left", actions::ActionDefinition::keyboard("CTRL+HOME"));
  add("up-right", actions::ActionDefinition::keyboard("CTRL+END"));
}

void migrate_default_global_actions(ConfigurationBundle& configuration) {
  if (configuration.profiles.version >= ProfileFile::current_version) return;
  add_default_global_actions(configuration.profiles.global_actions);
  configuration.profiles.version = ProfileFile::current_version;
}

bool quarantine(const std::filesystem::path& path, std::string& error) {
  auto invalid = path;
  invalid += ".invalid";
  std::error_code ec;
  for (unsigned suffix = 1; std::filesystem::exists(invalid, ec) && !ec; ++suffix) {
    invalid = path;
    invalid += ".invalid." + std::to_string(suffix);
  }
  if (ec) {
    error = "cannot inspect invalid configuration path: " + ec.message();
    return false;
  }
  std::filesystem::rename(path, invalid, ec);
  if (ec) {
    error = "cannot preserve invalid " + path.string() + ": " + ec.message();
    return false;
  }
  return true;
}

template <class T, class Decoder>
bool load_component(const std::filesystem::path& path, const T& fallback, Decoder decoder,
                    T& output, std::string& warnings, std::string& error) {
  auto encoded = read_json(path, error);
  if (encoded) {
    auto decoded = decoder(*encoded);
    if (decoded) {
      output = std::move(*decoded.value);
      if (!decoded.warning.empty())
        append_warning(warnings, path.string() + ": " + decoded.warning);
      return true;
    }
    error = path.string() + ": " + decoded.error;
  }

  const std::string original_error = error;
  if (!quarantine(path, error)) return false;
  if (!write_encoded(path, fallback, error)) return false;
  output = fallback;
  append_warning(warnings, original_error + " (invalid file preserved; defaults restored)");
  return true;
}

}  // namespace

ConfigurationStore::ConfigurationStore(std::filesystem::path directory)
    : directory_(std::move(directory)) {}

ConfigurationBundle ConfigurationStore::defaults() {
  ConfigurationBundle result;
  result.gestures.gestures = built_in_gestures();
  add_default_global_actions(result.profiles.global_actions);
  return result;
}

ConfigurationLoadResult ConfigurationStore::load() const {
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) return {{}, "cannot create configuration directory: " + ec.message()};

  const auto config_path = directory_ / "config.json";
  const auto gesture_path = directory_ / "gestures.json";
  const auto profile_path = directory_ / "profiles.json";
  const auto initial = defaults();
  std::string error;
  const std::array paths{config_path, gesture_path, profile_path};
  if (!recover_transaction(directory_, paths, error)) return {{}, error};
  for (const auto& [path, text] :
       std::array{std::pair{config_path, json::serialize(encode(initial.global))},
                  std::pair{gesture_path, json::serialize(encode(initial.gestures))},
                  std::pair{profile_path, json::serialize(encode(initial.profiles))}}) {
    const bool exists = std::filesystem::exists(path, ec);
    if (ec) return {{}, "cannot inspect " + path.string() + ": " + ec.message()};
    if (!exists && !atomic_write(path, text, error)) return {{}, error};
  }

  ConfigurationBundle result;
  std::string warnings;
  if (!load_component(config_path, initial.global, decode_options, result.global, warnings,
                      error) ||
      !load_component(gesture_path, initial.gestures, decode_gestures, result.gestures, warnings,
                      error) ||
      !load_component(profile_path, initial.profiles, decode_profiles, result.profiles, warnings,
                      error)) {
    return {{}, error};
  }
  migrate_gesture_catalog(result);
  migrate_default_global_actions(result);
  return {std::move(result), {}, std::move(warnings)};
}

bool ConfigurationStore::save(const ConfigurationBundle& value, std::string& error) const {
  std::error_code ec;
  std::filesystem::create_directories(directory_, ec);
  if (ec) {
    error = "cannot create configuration directory: " + ec.message();
    return false;
  }
  const std::array paths{directory_ / "config.json", directory_ / "gestures.json",
                         directory_ / "profiles.json"};
  if (!recover_transaction(directory_, paths, error)) return false;
  const std::array texts{json::serialize(encode(value.global)),
                         json::serialize(encode(value.gestures)),
                         json::serialize(encode(value.profiles))};
  return transactional_write(directory_, paths, texts, error);
}

}  // namespace strokes::config
