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
  f1 = 0x70,
};

struct KeyboardShortcut {
  std::vector<VirtualKey> modifiers;
  VirtualKey key{};
};

[[nodiscard]] std::optional<KeyboardShortcut> parse_shortcut(std::string_view text);

}  // namespace strokes::actions
