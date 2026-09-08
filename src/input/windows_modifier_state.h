#pragma once

#include "input/modifier_state_provider.h"

namespace strokes::input {

class WindowsModifierStateProvider final : public IModifierStateProvider {
public:
    [[nodiscard]] ModifierState current_modifiers() const override;
};

}  // namespace strokes::input
