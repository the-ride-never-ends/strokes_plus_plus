#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>

#include "actions/physical_key_state.h"

namespace strokes::input {
/// Owns the Escape cancellation hook and physical modifier tracking.
class WindowsKeyboardHook final {
 public:
  using EscapeHandler = bool (*)(void*) noexcept;
  WindowsKeyboardHook() = default;
  ~WindowsKeyboardHook();
  WindowsKeyboardHook(const WindowsKeyboardHook&) = delete;
  WindowsKeyboardHook& operator=(const WindowsKeyboardHook&) = delete;
  /// Installs the hook and begins recording physical modifier transitions.
  ///
  /// Args:
  ///   handler: Invoked for each physical Escape press; true cancels the key.
  ///   context: Opaque value passed back to the handler.
  ///   keys: Modifier state seeded on installation and updated per event.
  /// Returns:
  ///   Whether the hook is installed and owned by this instance.
  [[nodiscard]] bool start(EscapeHandler handler, void* context,
                           actions::PhysicalKeyState& keys) noexcept;
  void stop() noexcept;
  [[nodiscard]] bool running() const noexcept { return hook_ != nullptr; }
  /// Decides whether one Escape event is suppressed and updates the key-up latch.
  ///
  /// Args:
  ///   message: The low-level keyboard message.
  ///   native: The event payload, used for the key code and injection flags.
  ///   handler_result: Whether a press was consumed as a gesture cancellation.
  ///   suppress_key_up: Latch carried between the press and its release.
  /// Returns:
  ///   Whether the event must be withheld from the rest of the hook chain.
  [[nodiscard]] static bool filter_escape(WPARAM message, const KBDLLHOOKSTRUCT& native,
                                          bool handler_result, bool& suppress_key_up) noexcept;

 private:
  static LRESULT CALLBACK hook_callback(int, WPARAM, LPARAM) noexcept;
  void finish_stop() noexcept;
  void seed() noexcept;
  HHOOK hook_{};
  EscapeHandler handler_{};
  void* context_{};
  actions::PhysicalKeyState* keys_{};
  bool suppress_escape_up_{};
  bool stop_pending_{};
  static std::atomic<WindowsKeyboardHook*> active_hook_;
};
}  // namespace strokes::input
