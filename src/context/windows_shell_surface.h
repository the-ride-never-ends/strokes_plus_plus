#pragma once

#include <cstdint>
#include <string_view>

namespace strokes::context {

/// Returns true for top-level Windows shell surfaces that must not become gesture targets.
/// These windows are implementation details of the taskbar and its flyouts; changing their
/// window state can leave Explorer's UI state out of sync with the actual HWND state.
[[nodiscard]] bool is_protected_shell_class(std::wstring_view class_name) noexcept;
[[nodiscard]] bool is_protected_shell_window(std::uintptr_t window) noexcept;

}  // namespace strokes::context
