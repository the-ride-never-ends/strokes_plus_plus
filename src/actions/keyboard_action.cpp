#include "actions/keyboard_action.h"

#include <algorithm>
#include <iterator>

namespace strokes::actions {
namespace {
void append_releases(std::vector<KeyEvent>& events, const std::vector<VirtualKey>& injected) {
  for (auto modifier = injected.rbegin(); modifier != injected.rend(); ++modifier)
    events.push_back({*modifier, false});
}

void append_restores(std::vector<KeyEvent>& events,
                     const std::vector<VirtualKey>& neutralized, IKeyboardInput& input) {
  for (auto modifier = neutralized.rbegin(); modifier != neutralized.rend(); ++modifier) {
    if (input.is_key_down(*modifier)) events.push_back({*modifier, true});
  }
}

bool requests(const KeyboardShortcut& shortcut, VirtualKey physical) {
  VirtualKey logical = physical;
  if (physical == VirtualKey::left_shift || physical == VirtualKey::right_shift)
    logical = VirtualKey::shift;
  else if (physical == VirtualKey::left_control || physical == VirtualKey::right_control)
    logical = VirtualKey::control;
  else if (physical == VirtualKey::left_alt || physical == VirtualKey::right_alt)
    logical = VirtualKey::alt;
  return std::ranges::find(shortcut.modifiers, logical) != shortcut.modifiers.end();
}

bool is_down(IKeyboardInput& input, VirtualKey logical) {
  if (logical == VirtualKey::shift)
    return input.is_key_down(VirtualKey::left_shift) || input.is_key_down(VirtualKey::right_shift);
  if (logical == VirtualKey::control)
    return input.is_key_down(VirtualKey::left_control) ||
           input.is_key_down(VirtualKey::right_control);
  if (logical == VirtualKey::alt)
    return input.is_key_down(VirtualKey::left_alt) || input.is_key_down(VirtualKey::right_alt);
  return input.is_key_down(logical);
}
}  // namespace

bool KeyboardActionExecutor::execute(const KeyboardShortcut& shortcut, IKeyboardInput& input) {
  std::vector<VirtualKey> injected_modifiers;
  std::vector<VirtualKey> neutralized_modifiers;
  std::vector<KeyEvent> events;
  events.reserve(shortcut.modifiers.size() * 2 + std::size(modifier_keys) * 2 + 2);

  for (const VirtualKey modifier : modifier_keys) {
    if (!requests(shortcut, modifier) && input.is_key_down(modifier)) {
      events.push_back({modifier, false});
      neutralized_modifiers.push_back(modifier);
    }
  }

  for (const VirtualKey modifier : shortcut.modifiers) {
    if (!is_down(input, modifier)) {
      events.push_back({modifier, true});
      injected_modifiers.push_back(modifier);
    }
  }
  events.push_back({shortcut.key, true});
  events.push_back({shortcut.key, false});
  append_releases(events, injected_modifiers);
  const bool sent = input.send(events);

  // The restore pass runs after the injection so that a modifier the user let
  // go of mid-action is not pressed again. This requires is_key_down to report
  // physical state; an implementation backed by the injected key-state table
  // would always answer "released" here.
  std::vector<KeyEvent> restores;
  append_restores(restores, neutralized_modifiers, input);
  if (!restores.empty()) (void)input.send(restores);
  if (sent) return true;

  std::vector<KeyEvent> cleanup;
  cleanup.reserve(injected_modifiers.size());
  append_releases(cleanup, injected_modifiers);
  if (!cleanup.empty()) (void)input.send(cleanup);
  return false;
}

}  // namespace strokes::actions
