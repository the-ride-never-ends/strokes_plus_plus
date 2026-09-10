#include "actions/windows_keyboard_input.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

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
  const UINT sent = sender_(input_count, inputs.data(), static_cast<int>(sizeof(INPUT)));
  return sent == input_count;
}

}  // namespace strokes::actions
