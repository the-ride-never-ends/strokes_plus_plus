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
  mutable std::mutex mutex_;
  std::ofstream output_;
};
}  // namespace strokes::logging
