#pragma once

#include "actions/keyboard_shortcut.h"

#include <span>
#include <vector>

namespace strokes::actions {

struct KeyEvent {
    VirtualKey key{};
    bool key_down{};

    friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

class IKeyboardInput {
public:
    virtual ~IKeyboardInput() = default;
    [[nodiscard]] virtual bool is_key_down(VirtualKey key) const = 0;
    [[nodiscard]] virtual bool send(std::span<const KeyEvent> events) = 0;
};

class KeyboardActionExecutor {
public:
    [[nodiscard]] static bool execute(const KeyboardShortcut& shortcut, IKeyboardInput& input);
};

}  // namespace strokes::actions
