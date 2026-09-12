#pragma once

#include <memory>

#include "actions/action_definition.h"
#include "actions/action_services.h"
#include "actions/executable_action.h"

namespace strokes::actions {

struct ActionFactoryResult {
  std::unique_ptr<IAction> action;
  ActionValidationResult validation;

  [[nodiscard]] explicit operator bool() const noexcept { return action != nullptr; }
};

class ActionFactory {
 public:
  /// Validates and copies a definition into an independently executable action.
  [[nodiscard]] static ActionFactoryResult create(const ActionDefinition& definition,
                                                  ActionServices services);
};

}  // namespace strokes::actions
