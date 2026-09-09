#pragma once

#include "input/modifier_state_provider.h"

namespace strokes::input {

/// Reads physical modifier-key state from Windows.
class WindowsModifierStateProvider final : public IModifierStateProvider {
 public:
  [[nodiscard]] ModifierState current_modifiers() const override;
};

}  // namespace strokes::input
