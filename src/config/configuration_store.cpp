#include "config/configuration_store.h"

#include <array>
#include <cstdio>
#include <fstream>
#include <sstream>
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
    for (std::size_t index = 0; index < Size; ++index) {
      auto temporary = paths[index];
      temporary += ".tmp";
      if (committed[index]) std::filesystem::remove(paths[index], ec);
      ec.clear();
      auto backup = paths[index];
      backup += ".bak";
      if (backed_up[index]) std::filesystem::rename(backup, paths[index], ec);
      ec.clear();
      auto missing = paths[index];
      missing += ".bak.missing";
      if (marked_missing[index]) std::filesystem::remove(missing, ec);
      ec.clear();
      std::filesystem::remove(temporary, ec);
      ec.clear();
    }
    std::filesystem::remove(marker, ec);
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
      if (!decoded.error.empty()) append_warning(warnings, path.string() + ": " + decoded.error);
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
  result.gestures.gestures.push_back(
      {"right", "Right", true, {{"default-right", {{0, 0}, {30, 0}, {60, 0}, {100, 0}}}}});
  result.profiles.global_actions.emplace(
      "right", actions::Action{actions::ActionType::keyboard_shortcut, "ALT+RIGHT"});
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
