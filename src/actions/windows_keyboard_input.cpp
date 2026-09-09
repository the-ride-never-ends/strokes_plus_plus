#include "actions/windows_keyboard_input.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <limits>
#include <vector>

namespace strokes::actions {

bool WindowsKeyboardInput::is_key_down(VirtualKey key) const {
  return (::GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}

bool WindowsKeyboardInput::send(std::span<const KeyEvent> events) {
  if (events.empty() || events.size() > std::numeric_limits<UINT>::max()) {
    return false;
  }

  std::vector<INPUT> inputs;
  inputs.reserve(events.size());
  for (const auto& event : events) {
    INPUT input{};
    input.type = INPUT_KEYBOARD;
    input.ki.wVk = static_cast<WORD>(event.key);
    input.ki.dwFlags = event.key_down ? 0U : KEYEVENTF_KEYUP;
    inputs.push_back(input);
  }
  const UINT input_count = static_cast<UINT>(inputs.size());
  const UINT sent = ::SendInput(input_count, inputs.data(), static_cast<int>(sizeof(INPUT)));
  if (sent == input_count) return true;

  std::vector<INPUT> cleanup;
  std::vector<VirtualKey> pressed;
  for (UINT index = 0; index < sent; ++index) {
    const auto key = events[index].key;
    if (events[index].key_down) {
      if (std::ranges::find(pressed, key) == pressed.end()) pressed.push_back(key);
    } else {
      std::erase(pressed, key);
    }
  }
  cleanup.reserve(pressed.size());
  for (auto iterator = pressed.rbegin(); iterator != pressed.rend(); ++iterator) {
    INPUT release{};
    release.type = INPUT_KEYBOARD;
    release.ki.wVk = static_cast<WORD>(*iterator);
    release.ki.dwFlags = KEYEVENTF_KEYUP;
    cleanup.push_back(release);
  }
  if (!cleanup.empty()) {
    (void)::SendInput(static_cast<UINT>(cleanup.size()), cleanup.data(),
                      static_cast<int>(sizeof(INPUT)));
  }
  return false;
}

}  // namespace strokes::actions
