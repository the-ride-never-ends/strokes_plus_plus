#include "input/windows_keyboard_hook.h"

namespace strokes::input {
namespace {
bool pressed(actions::VirtualKey key) noexcept {
  return (::GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}
}  // namespace

std::atomic<WindowsKeyboardHook*> WindowsKeyboardHook::active_hook_{nullptr};

WindowsKeyboardHook::~WindowsKeyboardHook() {
  suppress_escape_up_ = false;
  finish_stop();
}

void WindowsKeyboardHook::seed() noexcept {
  // The hook only observes transitions, so modifiers already held when it is
  // installed have to be sampled once. No injection is in flight at this point,
  // so the asynchronous key state is still the user's physical state here.
  for (const actions::VirtualKey key : actions::modifier_keys) keys_->update(key, pressed(key));
}

bool WindowsKeyboardHook::start(EscapeHandler handler, void* context,
                                actions::PhysicalKeyState& keys) noexcept {
  if (handler == nullptr) return false;
  if (running()) {
    if (!stop_pending_) return false;
    handler_ = handler;
    context_ = context;
    keys_ = &keys;
    stop_pending_ = false;
    seed();
    return true;
  }
  WindowsKeyboardHook* expected = nullptr;
  if (!active_hook_.compare_exchange_strong(expected, this, std::memory_order_acq_rel))
    return false;
  handler_ = handler;
  context_ = context;
  keys_ = &keys;
  hook_ = ::SetWindowsHookExW(WH_KEYBOARD_LL, hook_callback, ::GetModuleHandleW(nullptr), 0);
  if (hook_ == nullptr) {
    handler_ = nullptr;
    context_ = nullptr;
    keys_ = nullptr;
    active_hook_.store(nullptr, std::memory_order_release);
    return false;
  }
  seed();
  return true;
}

void WindowsKeyboardHook::stop() noexcept {
  handler_ = nullptr;
  context_ = nullptr;
  if (suppress_escape_up_) {
    stop_pending_ = true;
    return;
  }
  finish_stop();
}

void WindowsKeyboardHook::finish_stop() noexcept {
  // A deferred stop deliberately completes from the callback after the
  // suppressed Escape key-up, which Win32 permits for low-level hooks.
  if (hook_ != nullptr) {
    ::UnhookWindowsHookEx(hook_);
    hook_ = nullptr;
  }
  WindowsKeyboardHook* expected = this;
  active_hook_.compare_exchange_strong(expected, nullptr, std::memory_order_acq_rel);
  handler_ = nullptr;
  context_ = nullptr;
  keys_ = nullptr;
  stop_pending_ = false;
  suppress_escape_up_ = false;
}

LRESULT CALLBACK WindowsKeyboardHook::hook_callback(int code, WPARAM message,
                                                    LPARAM data) noexcept {
  if (code == HC_ACTION && data != 0) {
    const auto& native = *reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
    if (auto* hook = active_hook_.load(std::memory_order_acquire); hook) {
      const bool is_down = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
      const bool is_up = message == WM_KEYUP || message == WM_SYSKEYUP;
      const bool physical =
          (native.flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED)) == 0;
      const auto key = static_cast<actions::VirtualKey>(native.vkCode);
      if (physical && (is_down || is_up) && hook->keys_ != nullptr && actions::modifier(key))
        hook->keys_->update(key, is_down);
      // Every physical press consults the handler, including a repeat, so that
      // a latch whose key-up was never delivered cannot swallow Escape for the
      // rest of the session.
      const bool handled = physical && is_down && native.vkCode == VK_ESCAPE &&
                           hook->handler_ != nullptr && hook->handler_(hook->context_);
      const bool filtered = filter_escape(message, native, handled, hook->suppress_escape_up_);
      if (hook->stop_pending_ && !hook->suppress_escape_up_) hook->finish_stop();
      if (filtered) return 1;
    }
  }
  return ::CallNextHookEx(nullptr, code, message, data);
}

bool WindowsKeyboardHook::filter_escape(WPARAM message, const KBDLLHOOKSTRUCT& native,
                                        bool handler_result, bool& suppress_key_up) noexcept {
  if (native.vkCode != VK_ESCAPE ||
      (native.flags & (LLKHF_INJECTED | LLKHF_LOWER_IL_INJECTED)) != 0)
    return false;
  if (message == WM_KEYUP || message == WM_SYSKEYUP) {
    const bool suppressed = suppress_key_up;
    suppress_key_up = false;
    return suppressed;
  }
  if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) {
    suppress_key_up = handler_result;
    return handler_result;
  }
  return false;
}
}  // namespace strokes::input
