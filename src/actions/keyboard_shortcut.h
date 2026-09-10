#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace strokes::actions {

// Values intentionally match the Win32 virtual-key constants while keeping
// shortcut parsing independent of Windows headers.
enum class VirtualKey : std::uint16_t {
  backspace = 0x08,
  tab = 0x09,
  enter = 0x0D,
  shift = 0x10,
  control = 0x11,
  alt = 0x12,
  escape = 0x1B,
  space = 0x20,
  page_up = 0x21,
  page_down = 0x22,
  end = 0x23,
  home = 0x24,
  left = 0x25,
  up = 0x26,
  right = 0x27,
  down = 0x28,
  delete_key = 0x2E,
  digit_0 = 0x30,
  letter_a = 0x41,
  left_windows = 0x5B,
  right_windows = 0x5C,
  left_shift = 0xA0,
  right_shift = 0xA1,
  left_control = 0xA2,
  right_control = 0xA3,
  left_alt = 0xA4,
  right_alt = 0xA5,
  f1 = 0x70,
};

/// The sided modifier keys the action executor observes and injects.
inline constexpr VirtualKey modifier_keys[]{
    VirtualKey::left_control, VirtualKey::right_control, VirtualKey::left_shift,
    VirtualKey::right_shift,  VirtualKey::left_alt,      VirtualKey::right_alt,
    VirtualKey::left_windows, VirtualKey::right_windows};

/// Reports whether a virtual key is one of the tracked modifiers.
[[nodiscard]] constexpr bool modifier(VirtualKey key) noexcept {
  for (const VirtualKey candidate : modifier_keys) {
    if (candidate == key) return true;
  }
  return false;
}

struct KeyboardShortcut {
  std::vector<VirtualKey> modifiers;
  VirtualKey key{};
};

[[nodiscard]] std::optional<KeyboardShortcut> parse_shortcut(std::string_view text);

using KeyboardShortcutSequence = std::vector<KeyboardShortcut>;

/// Parses one or more shortcut chords separated by commas.
[[nodiscard]] std::optional<KeyboardShortcutSequence> parse_shortcut_sequence(
    std::string_view text);

}  // namespace strokes::actions
