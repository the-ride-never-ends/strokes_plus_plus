#pragma once

#include <iostream>
#include <source_location>
#include <string_view>

namespace strokes::tests {

inline int failures = 0;

inline void check(bool condition, std::string_view message,
                  const std::source_location location = std::source_location::current()) {
  if (!condition) {
    std::cerr << location.file_name() << ':' << location.line() << ": FAIL: " << message << '\n';
    ++failures;
  }
}

void run_state_machine_tests();
void run_profile_tests();
void run_keyboard_action_tests();
void run_input_queue_tests();
void run_mouse_input_router_tests();
void run_gesture_engine_tests();
void run_json_tests();
void run_configuration_tests();
void run_configuration_store_tests();
void run_logging_tests();
void run_repository_tests();

}  // namespace strokes::tests
