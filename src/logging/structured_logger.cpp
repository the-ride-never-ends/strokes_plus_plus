#include "logging/structured_logger.h"

#include <chrono>
#include <utility>

namespace strokes::logging {
namespace {
constexpr std::uintmax_t maximum_log_size = 5U * 1024U * 1024U;
}

StructuredLogger::StructuredLogger(const std::filesystem::path& path) : path_(path) {
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) return;
  const bool exists = std::filesystem::exists(path, error);
  if (error) return;
  const auto size = exists ? std::filesystem::file_size(path, error) : 0;
  if (!error && exists && size >= maximum_log_size) {
    auto previous = path;
    previous += ".1";
    std::filesystem::remove(previous, error);
    error.clear();
    std::filesystem::rename(path, previous, error);
  }
  if (!error) {
    output_.open(path, std::ios::binary | std::ios::app);
    bytes_written_ = exists && size < maximum_log_size ? size : 0;
  }
}

bool StructuredLogger::rotate(std::size_t incoming) noexcept {
  if (bytes_written_ + incoming <= maximum_log_size) return true;
  output_.close();
  std::error_code error;
  auto previous = path_;
  previous += ".1";
  std::filesystem::remove(previous, error);
  error.clear();
  std::filesystem::rename(path_, previous, error);
  if (error) {
    output_.clear();
    output_.open(path_, std::ios::binary | std::ios::app);
    return output_.good();
  }
  output_.open(path_, std::ios::binary | std::ios::trunc);
  bytes_written_ = 0;
  return output_.good();
}

bool StructuredLogger::ready() const noexcept {
  std::scoped_lock lock(mutex_);
  return output_.is_open() && output_.good();
}

bool StructuredLogger::log(std::string_view event, config::json::Object fields) noexcept {
  try {
    const auto now =
        std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now())
            .time_since_epoch()
            .count();
    fields.insert_or_assign("event", std::string(event));
    fields.insert_or_assign("timestamp_ms", static_cast<double>(now));
    const std::string line = config::json::serialize(config::json::Value{std::move(fields)}, false);
    std::scoped_lock lock(mutex_);
    if (!output_ || !rotate(line.size() + 1)) return false;
    output_ << line << '\n';
    output_.flush();
    if (output_.good()) bytes_written_ += line.size() + 1;
    return output_.good();
  } catch (...) {
    return false;
  }
}
}  // namespace strokes::logging
