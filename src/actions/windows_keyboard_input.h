#pragma once

#include "actions/keyboard_action.h"
#include "actions/physical_key_state.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <utility>

namespace strokes::actions {

/// Injects keyboard events through the Windows SendInput API.
///
/// Held-key queries are answered from the physical key state maintained by the
/// low-level keyboard hook rather than from GetAsyncKeyState, whose table this
/// class's own injection modifies.
class WindowsKeyboardInput final : public IKeyboardInput {
 public:
  using Sender = std::function<UINT(UINT, INPUT*, int)>;
  explicit WindowsKeyboardInput(const PhysicalKeyState& keys, Sender sender = ::SendInput)
      : keys_(&keys), sender_(std::move(sender)) {}
  [[nodiscard]] bool is_key_down(VirtualKey key) const override;
  [[nodiscard]] bool send(std::span<const KeyEvent> events) override;

 private:
  const PhysicalKeyState* keys_;
  Sender sender_;
};

}  // namespace strokes::actions
