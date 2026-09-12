#pragma once

#include "actions/action_context.h"
#include "actions/action_result.h"

namespace strokes::actions {

class IAction {
 public:
  virtual ~IAction() = default;
  [[nodiscard]] virtual ActionResult execute(const ActionContext& context) = 0;
};

}  // namespace strokes::actions
