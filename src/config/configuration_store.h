#pragma once
#include <filesystem>
#include <optional>
#include <string>

#include "config/configuration.h"

namespace strokes::config {
struct ConfigurationBundle {
  GlobalOptions global;
  GestureFile gestures;
  ProfileFile profiles;
};
struct ConfigurationLoadResult {
  std::optional<ConfigurationBundle> value;
  std::string error;
  std::string warnings;
  [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

/// Loads and transactionally persists the three-file configuration bundle.
class ConfigurationStore {
 public:
  explicit ConfigurationStore(std::filesystem::path directory);
  /// Loads each component independently, recovering invalid files where possible.
  ///
  /// Returns:
  ///   A value on success, recoverable diagnostics in `warnings`, and a fatal
  ///   diagnostic in `error` only when no bundle can be returned.
  [[nodiscard]] ConfigurationLoadResult load() const;
  [[nodiscard]] bool save(const ConfigurationBundle& value, std::string& error) const;
  [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
  [[nodiscard]] static ConfigurationBundle defaults();

 private:
  std::filesystem::path directory_;
};
}  // namespace strokes::config
