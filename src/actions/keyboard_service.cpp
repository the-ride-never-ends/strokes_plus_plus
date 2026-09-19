#include "actions/keyboard_service.h"

#include <chrono>
#include <iterator>
#include <thread>

#include "actions/keyboard_shortcut.h"

namespace strokes::actions {

ActionResult KeyboardService::send_shortcut(std::string_view shortcut) {
  const auto sequence = parse_shortcut_sequence(shortcut);
  if (!sequence) {
    return ActionResult::failed(ActionError::invalid_definition, "invalid_shortcut",
                                "The keyboard shortcut is invalid.");
  }
  for (auto step = sequence->begin(); step != sequence->end(); ++step) {
    if (!KeyboardActionExecutor::execute(*step, input_)) {
      return ActionResult::failed(ActionError::platform_failure, "keyboard_injection_failed",
                                  "Windows did not accept the keyboard input.");
    }
    if (std::next(step) != sequence->end())
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
  return ActionResult::succeeded();
}

ActionResult KeyboardService::send_key(std::string_view key, bool key_down) {
  const auto parsed = parse_key(key);
  if (!parsed)
    return ActionResult::failed(ActionError::invalid_definition, "invalid_key",
                                "The keyboard key is invalid.");
  const KeyEvent event{*parsed, key_down};
  return input_.send(std::span{&event, std::size_t{1}})
             ? ActionResult::succeeded()
             : ActionResult::failed(ActionError::platform_failure, "keyboard_injection_failed",
                                    "Windows did not accept the keyboard input.");
}

std::optional<bool> KeyboardService::is_key_down(std::string_view key) const {
  const auto parsed = parse_key(key);
  if (!parsed) return std::nullopt;
  if (*parsed == VirtualKey::shift)
    return input_.is_key_down(VirtualKey::left_shift) ||
           input_.is_key_down(VirtualKey::right_shift);
  if (*parsed == VirtualKey::control)
    return input_.is_key_down(VirtualKey::left_control) ||
           input_.is_key_down(VirtualKey::right_control);
  if (*parsed == VirtualKey::alt)
    return input_.is_key_down(VirtualKey::left_alt) || input_.is_key_down(VirtualKey::right_alt);
  return input_.is_key_down(*parsed);
}

}  // namespace strokes::actions
