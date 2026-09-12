#pragma once

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsProcessService final : public IProcessService {
 public:
  [[nodiscard]] ActionResult launch(const ProcessParameters& parameters) override;
};

}  // namespace strokes::actions
