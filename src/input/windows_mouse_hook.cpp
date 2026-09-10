#include "input/windows_mouse_hook.h"

#include <chrono>

namespace strokes::input {

std::atomic<WindowsMouseHook*> WindowsMouseHook::active_hook_{nullptr};

WindowsMouseHook::~WindowsMouseHook() { stop(); }

bool WindowsMouseHook::start(Handler handler, void* context) noexcept {
  if (running() || handler == nullptr) {
    return false;
  }

  WindowsMouseHook* expected = nullptr;
  if (!active_hook_.compare_exchange_strong(expected, this, std::memory_order_acq_rel)) {
    return false;
  }
  handler_ = handler;
  context_ = context;
  hook_ = ::SetWindowsHookExW(WH_MOUSE_LL, hook_callback, ::GetModuleHandleW(nullptr), 0);
  if (hook_ == nullptr) {
    handler_ = nullptr;
    context_ = nullptr;
    active_hook_.store(nullptr, std::memory_order_release);
    return false;
  }
  return true;
}

void WindowsMouseHook::stop() noexcept {
  if (hook_ != nullptr) {
    ::UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
  }
  WindowsMouseHook* expected = this;
  active_hook_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
  handler_ = nullptr;
  context_ = nullptr;
}

LRESULT CALLBACK WindowsMouseHook::hook_callback(int code, WPARAM message, LPARAM data) noexcept {
  if (code == HC_ACTION && data != 0) {
    const auto& native = *reinterpret_cast<const MSLLHOOKSTRUCT*>(data);
    if ((native.flags & (LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED)) == 0) {
      MouseInputEvent event;
      if (translate(message, native, event)) {
        if (WindowsMouseHook* hook = active_hook_.load(std::memory_order_acquire);
            hook != nullptr && hook->handler_ != nullptr && hook->handler_(event, hook->context_)) {
          return 1;
        }
      }
    }
  }
  return ::CallNextHookEx(nullptr, code, message, data);
}

bool WindowsMouseHook::translate(WPARAM message, const MSLLHOOKSTRUCT& native,
                                 MouseInputEvent& event) noexcept {
  event.position = {static_cast<double>(native.pt.x), static_cast<double>(native.pt.y)};
  event.timestamp = std::chrono::milliseconds(native.time);
  if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
      message == WM_XBUTTONDOWN)
    event.target_window = reinterpret_cast<std::uintptr_t>(::GetForegroundWindow());

  switch (message) {
    case WM_MOUSEMOVE:
      event.type = MouseEventType::pointer_moved;
      return true;
    case WM_LBUTTONDOWN:
      event.type = MouseEventType::button_down;
      // Left is not a supported activation button, but routing the down event
      // lets an active gesture treat it as an invalid sequence.
      event.button = ActivationButton::left;
      return true;
    case WM_RBUTTONDOWN:
      event.type = MouseEventType::button_down;
      event.button = ActivationButton::right;
      return true;
    case WM_RBUTTONUP:
      event.type = MouseEventType::button_up;
      event.button = ActivationButton::right;
      return true;
    case WM_MBUTTONDOWN:
      event.type = MouseEventType::button_down;
      event.button = ActivationButton::middle;
      return true;
    case WM_MBUTTONUP:
      event.type = MouseEventType::button_up;
      event.button = ActivationButton::middle;
      return true;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
      event.type =
          message == WM_XBUTTONDOWN ? MouseEventType::button_down : MouseEventType::button_up;
      event.button = HIWORD(native.mouseData) == XBUTTON1 ? ActivationButton::x_button_1
                                                          : ActivationButton::x_button_2;
      return true;
    default:
      return false;
  }
}

}  // namespace strokes::input
