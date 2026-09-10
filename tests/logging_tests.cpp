#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "logging/structured_logger.h"
#include "test_support.h"

namespace strokes::tests {
void run_logging_tests() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("strokes-plus-plus-logger-tests-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  {
    logging::StructuredLogger logger(directory / "strokes.log");
    check(logger.ready(), "structured logger creates its directory and file");
    check(logger.log("gesture_start", {{"process", "notepad.exe"}, {"x", -120.0}}),
          "structured event is written");
    check(logger.log("recognition_result", {{"gesture", "left"}, {"score", 0.91}}),
          "second structured event is written");
    check(logger.log("large_record", {{"payload", std::string(5U * 1024U * 1024U, 'x')}}),
          "large log record is written");
    check(logger.log("rotation_trigger"), "logging continues after runtime rotation");
  }
  std::ifstream input(directory / "strokes.log");
  std::string line;
  int records = 0;
  while (std::getline(input, line)) {
    auto parsed = config::json::parse(line);
    check(static_cast<bool>(parsed), "each log line is valid JSON");
    auto* object = parsed ? parsed.value->get_if<config::json::Object>() : nullptr;
    check(object && object->contains("event") && object->contains("timestamp_ms"),
          "log record has required fields");
    ++records;
  }
  check(records == 1 && std::filesystem::exists(directory / "strokes.log.1"),
        "logger rotates while the process remains running");

  {
    logging::StructuredLogger logger(directory / "session.log",
                                     logging::StructuredLogger::OpenMode::truncate);
    check(logger.log("first_session"), "truncating logger writes its first session");
  }
  {
    logging::StructuredLogger logger(directory / "session.log",
                                     logging::StructuredLogger::OpenMode::truncate);
    check(logger.log("second_session"), "truncating logger writes its next session");
  }
  std::ifstream session(directory / "session.log");
  const std::string contents((std::istreambuf_iterator<char>(session)),
                             std::istreambuf_iterator<char>());
  check(contents.find("first_session") == std::string::npos &&
            contents.find("second_session") != std::string::npos,
        "truncating logger overwrites the previous run");
  std::filesystem::remove_all(directory, error);
}
}  // namespace strokes::tests
