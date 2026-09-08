#pragma once

#include "input/gesture_session.h"

namespace strokes::input {

class IModifierStateProvider {
public:
    virtual ~IModifierStateProvider() = default;
    [[nodiscard]] virtual ModifierState current_modifiers() const = 0;
};

}  // namespace strokes::input
