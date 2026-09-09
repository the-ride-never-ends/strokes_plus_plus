#pragma once

#include "input/input_event.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <atomic>

namespace strokes::input {

/// Owns the process-wide low-level Windows mouse hook.
class WindowsMouseHook final {
 public:
  using Handler = bool (*)(const MouseInputEvent& event, void* context) noexcept;

  WindowsMouseHook() = default;
  ~WindowsMouseHook();

  WindowsMouseHook(const WindowsMouseHook&) = delete;
  WindowsMouseHook& operator=(const WindowsMouseHook&) = delete;

  [[nodiscard]] bool start(Handler handler, void* context = nullptr) noexcept;
  void stop() noexcept;
  [[nodiscard]] bool running() const noexcept { return hook_ != nullptr; }

 private:
  static LRESULT CALLBACK hook_callback(int code, WPARAM message, LPARAM data) noexcept;
  [[nodiscard]] static bool translate(WPARAM message, const MSLLHOOKSTRUCT& native,
                                      MouseInputEvent& event) noexcept;

  HHOOK hook_{};
  Handler handler_{};
  void* context_{};
  static std::atomic<WindowsMouseHook*> active_hook_;
};

}  // namespace strokes::input
