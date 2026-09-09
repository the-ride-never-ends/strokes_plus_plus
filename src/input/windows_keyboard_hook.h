#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>

namespace strokes::input {
/// Owns the minimal low-level Escape cancellation hook.
class WindowsKeyboardHook final {
 public:
  using EscapeHandler = bool (*)(void*) noexcept;
  WindowsKeyboardHook() = default;
  ~WindowsKeyboardHook();
  WindowsKeyboardHook(const WindowsKeyboardHook&) = delete;
  WindowsKeyboardHook& operator=(const WindowsKeyboardHook&) = delete;
  [[nodiscard]] bool start(EscapeHandler handler, void* context = nullptr) noexcept;
  void stop() noexcept;
  [[nodiscard]] bool running() const noexcept { return hook_ != nullptr; }
  [[nodiscard]] static bool filter_escape(WPARAM message, const KBDLLHOOKSTRUCT& native,
                                          bool handler_result, bool& suppress_key_up) noexcept;

 private:
  static LRESULT CALLBACK hook_callback(int, WPARAM, LPARAM) noexcept;
  void finish_stop() noexcept;
  HHOOK hook_{};
  EscapeHandler handler_{};
  void* context_{};
  bool suppress_escape_up_{};
  bool stop_pending_{};
  static std::atomic<WindowsKeyboardHook*> active_hook_;
};
}  // namespace strokes::input
