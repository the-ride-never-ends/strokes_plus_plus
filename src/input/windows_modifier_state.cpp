#include "input/windows_modifier_state.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::input {
namespace {

bool pressed(int virtual_key) noexcept { return (::GetAsyncKeyState(virtual_key) & 0x8000) != 0; }

}  // namespace

ModifierState WindowsModifierStateProvider::current_modifiers() const {
  return {
      .control = pressed(VK_CONTROL),
      .shift = pressed(VK_SHIFT),
      .alt = pressed(VK_MENU),
      .windows = pressed(VK_LWIN) || pressed(VK_RWIN),
  };
}

}  // namespace strokes::input
