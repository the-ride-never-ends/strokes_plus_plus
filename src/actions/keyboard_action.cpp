#include "actions/keyboard_action.h"

#include <algorithm>

namespace strokes::actions {

bool KeyboardActionExecutor::execute(const KeyboardShortcut& shortcut, IKeyboardInput& input) {
  static constexpr VirtualKey all_modifiers[]{VirtualKey::control, VirtualKey::shift,
                                              VirtualKey::alt, VirtualKey::left_windows};
  std::vector<VirtualKey> injected_modifiers;
  std::vector<VirtualKey> neutralized_modifiers;
  std::vector<KeyEvent> events;
  events.reserve(shortcut.modifiers.size() * 2 + std::size(all_modifiers) * 2 + 2);

  for (const VirtualKey modifier : all_modifiers) {
    const bool requested =
        std::ranges::find(shortcut.modifiers, modifier) != shortcut.modifiers.end();
    if (!requested && input.is_key_down(modifier)) {
      events.push_back({modifier, false});
      neutralized_modifiers.push_back(modifier);
    }
  }

  for (const VirtualKey modifier : shortcut.modifiers) {
    if (!input.is_key_down(modifier)) {
      events.push_back({modifier, true});
      injected_modifiers.push_back(modifier);
    }
  }
  events.push_back({shortcut.key, true});
  events.push_back({shortcut.key, false});
  for (auto modifier = injected_modifiers.rbegin(); modifier != injected_modifiers.rend();
       ++modifier) {
    events.push_back({*modifier, false});
  }
  for (auto modifier = neutralized_modifiers.rbegin(); modifier != neutralized_modifiers.rend();
       ++modifier) {
    events.push_back({*modifier, true});
  }
  if (input.send(events)) return true;

  std::vector<KeyEvent> cleanup;
  cleanup.reserve(injected_modifiers.size() + neutralized_modifiers.size());
  for (auto modifier = injected_modifiers.rbegin(); modifier != injected_modifiers.rend();
       ++modifier) {
    cleanup.push_back({*modifier, false});
  }
  for (auto modifier = neutralized_modifiers.rbegin(); modifier != neutralized_modifiers.rend();
       ++modifier) {
    cleanup.push_back({*modifier, true});
  }
  if (!cleanup.empty()) (void)input.send(cleanup);
  return false;
}

}  // namespace strokes::actions
