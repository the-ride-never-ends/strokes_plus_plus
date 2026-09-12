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

}  // namespace strokes::actions
