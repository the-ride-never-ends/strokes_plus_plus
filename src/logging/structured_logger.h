#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "config/json.h"

namespace strokes::logging {
/// Appends thread-safe JSON Lines records to a size-bounded local log.
class StructuredLogger {
 public:
  explicit StructuredLogger(const std::filesystem::path& path);
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool log(std::string_view event, config::json::Object fields = {}) noexcept;

 private:
  [[nodiscard]] bool rotate(std::size_t incoming) noexcept;
  std::filesystem::path path_;
  std::uintmax_t bytes_written_{};
  mutable std::mutex mutex_;
  std::ofstream output_;
};
}  // namespace strokes::logging
