#pragma once

#include "actions/action_services.h"
#include "actions/keyboard_action.h"

namespace strokes::actions {

/// Adapts the existing balanced keyboard injector to the generalized service boundary.
class KeyboardService final : public IKeyboardService {
 public:
  explicit KeyboardService(IKeyboardInput& input) : input_(input) {}
  [[nodiscard]] ActionResult send_shortcut(std::string_view shortcut) override;
  [[nodiscard]] ActionResult send_key(std::string_view key, bool key_down) override;
  [[nodiscard]] std::optional<bool> is_key_down(std::string_view key) const override;

 private:
  IKeyboardInput& input_;
};

}  // namespace strokes::actions
