#pragma once
#include "config/json.h"
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string_view>

namespace strokes::logging {
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
