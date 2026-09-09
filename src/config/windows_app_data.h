#pragma once
#include <filesystem>
#include <optional>
namespace strokes::config {
[[nodiscard]] std::optional<std::filesystem::path> configuration_directory();
}
