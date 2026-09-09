#pragma once

#include "actions/keyboard_action.h"

namespace strokes::actions {

/// Injects keyboard events through the Windows SendInput API.
class WindowsKeyboardInput final : public IKeyboardInput {
 public:
  [[nodiscard]] bool is_key_down(VirtualKey key) const override;
  [[nodiscard]] bool send(std::span<const KeyEvent> events) override;
};

}  // namespace strokes::actions
