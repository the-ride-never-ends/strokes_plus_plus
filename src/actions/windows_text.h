#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <optional>
#include <limits>
#include <string>
#include <string_view>

namespace strokes::actions::detail {

inline std::optional<std::wstring> decode_utf8(std::string_view text) {
  if (text.empty()) return std::wstring{};
  if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return std::nullopt;
  const int size = static_cast<int>(text.size());
  const int needed = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size,
                                            nullptr, 0);
  if (needed <= 0) return std::nullopt;
  std::wstring result(static_cast<std::size_t>(needed), L'\0');
  if (::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), size, result.data(),
                            needed) != needed)
    return std::nullopt;
  return result;
}

}  // namespace strokes::actions::detail
