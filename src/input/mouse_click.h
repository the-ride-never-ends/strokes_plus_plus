#pragma once

#include "input/gesture_session.h"

namespace strokes::input {

class IMouseClick {
public:
    virtual ~IMouseClick() = default;
    [[nodiscard]] virtual bool click(ActivationButton button) = 0;
};

}  // namespace strokes::input
