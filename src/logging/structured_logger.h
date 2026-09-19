#pragma once
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

#include "actions/action_services.h"
#include "config/json.h"

namespace strokes::logging {
/// Appends thread-safe JSON Lines records to a size-bounded local log.
class StructuredLogger final : public actions::IDiagnosticService {
 public:
  enum class OpenMode { append, truncate };

  explicit StructuredLogger(const std::filesystem::path& path,
                            OpenMode mode = OpenMode::append);
  [[nodiscard]] bool ready() const noexcept;
  [[nodiscard]] bool log(std::string_view event, config::json::Object fields = {}) noexcept;
  [[nodiscard]] actions::ActionResult write(std::string_view level,
                                            std::string_view message) override;

 private:
  [[nodiscard]] bool rotate(std::size_t incoming) noexcept;
  std::filesystem::path path_;
  std::uintmax_t bytes_written_{};
  mutable std::mutex mutex_;
  std::ofstream output_;
};
}  // namespace strokes::logging
