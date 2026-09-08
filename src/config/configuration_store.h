#pragma once
#include "config/configuration.h"
#include <filesystem>
#include <optional>
#include <string>

namespace strokes::config {
struct ConfigurationBundle {
    GlobalOptions global;
    GestureFile gestures;
    ProfileFile profiles;
};
struct ConfigurationLoadResult {
    std::optional<ConfigurationBundle> value;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

class ConfigurationStore {
public:
    explicit ConfigurationStore(std::filesystem::path directory);
    [[nodiscard]] ConfigurationLoadResult load_or_create() const;
    [[nodiscard]] bool save(const ConfigurationBundle& value, std::string& error) const;
    [[nodiscard]] const std::filesystem::path& directory() const noexcept { return directory_; }
    [[nodiscard]] static ConfigurationBundle defaults();
private:
    std::filesystem::path directory_;
};
}  // namespace strokes::config
