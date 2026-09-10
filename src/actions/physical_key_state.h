#pragma once

#include <array>
#include <atomic>
#include <cstddef>

#include "actions/keyboard_shortcut.h"

namespace strokes::actions {

/// Records which keys are physically held, ignoring injected input.
///
/// Injected key events update the same asynchronous key-state table
/// `GetAsyncKeyState` reads, so an action that neutralizes a modifier destroys
/// the evidence it needs to restore it. A low-level hook can tell physical
/// events from injected ones, so this holds the answer that survives injection.
class PhysicalKeyState {
 public:
  /// Records a physical transition for one key.
  void update(VirtualKey key, bool pressed) noexcept {
    keys_[index(key)].store(pressed, std::memory_order_relaxed);
  }

  /// Reports whether a key is currently held by the user.
  [[nodiscard]] bool held(VirtualKey key) const noexcept {
    return keys_[index(key)].load(std::memory_order_relaxed);
  }

 private:
  // Windows virtual-key codes are byte sized.
  static constexpr std::size_t capacity = 256;

  [[nodiscard]] static constexpr std::size_t index(VirtualKey key) noexcept {
    return static_cast<std::size_t>(key) & (capacity - 1);
  }

  std::array<std::atomic<bool>, capacity> keys_{};
};

}  // namespace strokes::actions
