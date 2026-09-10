#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui::detail {

/// Creates a static label, allocating an identifier when none is supplied.
void text(HWND parent, int id, const wchar_t* value, int x, int y, int w = 170, int h = 22);
/// Creates a focusable child control participating in dialog tab order.
HWND control(HWND parent, const wchar_t* type, const wchar_t* value, DWORD style, int id, int x,
             int y, int w, int h);
[[nodiscard]] std::wstring number(double value);
[[nodiscard]] std::wstring integer(std::size_t value);
[[nodiscard]] bool read_double(HWND window, int id, double low, double high, double& output);
[[nodiscard]] bool read_integer(HWND window, int id, long low, long high, long& output);
[[nodiscard]] std::optional<LRESULT> selected_combo(HWND window, int id, LRESULT count);
[[nodiscard]] std::string read_utf8(HWND window, int id);
[[nodiscard]] std::wstring wide(std::string_view value);
void populate_processes(HWND combo);

}  // namespace strokes::ui::detail
