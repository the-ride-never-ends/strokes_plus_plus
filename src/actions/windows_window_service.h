#pragma once

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsWindowService final : public IWindowService {
 public:
  [[nodiscard]] ActionResult perform(WindowOperation operation, std::uintptr_t window,
                                     const WindowParameters& parameters) override;
  [[nodiscard]] std::optional<Bounds> bounds(std::uintptr_t window) const override;
  [[nodiscard]] std::optional<MonitorInfo> monitor(std::uintptr_t window) const override;
};

}  // namespace strokes::actions
