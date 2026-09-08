#pragma once

#include <string>

namespace strokes::actions {

enum class ActionType {
    keyboard_shortcut,
};

struct Action {
    ActionType type{ActionType::keyboard_shortcut};
    std::string value;

    friend bool operator==(const Action&, const Action&) = default;
};

}  // namespace strokes::actions
