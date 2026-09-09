#pragma once

#include <span>
#include <vector>

#include "actions/keyboard_shortcut.h"

namespace strokes::actions {

struct KeyEvent {
  VirtualKey key{};
  bool key_down{};

  friend bool operator==(const KeyEvent&, const KeyEvent&) = default;
};

/// Abstracts keyboard state and input injection for portable action execution.
class IKeyboardInput {
 public:
  virtual ~IKeyboardInput() = default;
  [[nodiscard]] virtual bool is_key_down(VirtualKey key) const = 0;
  [[nodiscard]] virtual bool send(std::span<const KeyEvent> events) = 0;
};

/// Builds and submits balanced keyboard input while preserving physical modifier state.
class KeyboardActionExecutor {
 public:
  [[nodiscard]] static bool execute(const KeyboardShortcut& shortcut, IKeyboardInput& input);
};

}  // namespace strokes::actions
