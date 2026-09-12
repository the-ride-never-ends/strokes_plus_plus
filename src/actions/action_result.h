#pragma once

#include <string>
#include <utility>

namespace strokes::actions {

enum class ActionError {
  none,
  invalid_definition,
  invalid_runtime_target,
  unsupported_operation,
  platform_failure,
  execution_exception,
};

struct ActionResult {
  bool success{};
  ActionError error{ActionError::none};
  std::string code;
  std::string message;

  [[nodiscard]] static ActionResult succeeded() { return {true, ActionError::none, {}, {}}; }

  [[nodiscard]] static ActionResult failed(ActionError error, std::string code,
                                           std::string message) {
    return {false, error, std::move(code), std::move(message)};
  }
};

}  // namespace strokes::actions
