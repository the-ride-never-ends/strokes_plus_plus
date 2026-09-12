#pragma once

#include "actions/action_services.h"
#include "actions/keyboard_action.h"

namespace strokes::actions {

/// Adapts the existing balanced keyboard injector to the generalized service boundary.
class KeyboardService final : public IKeyboardService {
 public:
  explicit KeyboardService(IKeyboardInput& input) : input_(input) {}
  [[nodiscard]] ActionResult send_shortcut(std::string_view shortcut) override;

 private:
  IKeyboardInput& input_;
};

}  // namespace strokes::actions
