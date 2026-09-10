#pragma once

#include "actions/keyboard_action.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <utility>

namespace strokes::actions {

/// Injects keyboard events through the Windows SendInput API.
class WindowsKeyboardInput final : public IKeyboardInput {
 public:
  using Sender = std::function<UINT(UINT, INPUT*, int)>;
  explicit WindowsKeyboardInput(Sender sender = ::SendInput) : sender_(std::move(sender)) {}
  [[nodiscard]] bool is_key_down(VirtualKey key) const override;
  [[nodiscard]] bool send(std::span<const KeyEvent> events) override;

 private:
  Sender sender_;
};

}  // namespace strokes::actions
