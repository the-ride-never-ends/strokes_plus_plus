#include "actions/keyboard_action.h"

#include <algorithm>

namespace strokes::actions {

bool KeyboardActionExecutor::execute(const KeyboardShortcut& shortcut, IKeyboardInput& input) {
    std::vector<VirtualKey> injected_modifiers;
    std::vector<KeyEvent> events;
    events.reserve(shortcut.modifiers.size() * 2 + 2);

    for (const VirtualKey modifier : shortcut.modifiers) {
        if (!input.is_key_down(modifier)) {
            events.push_back({modifier, true});
            injected_modifiers.push_back(modifier);
        }
    }
    events.push_back({shortcut.key, true});
    events.push_back({shortcut.key, false});
    for (auto modifier = injected_modifiers.rbegin(); modifier != injected_modifiers.rend(); ++modifier) {
        events.push_back({*modifier, false});
    }
    return input.send(events);
}

}  // namespace strokes::actions
