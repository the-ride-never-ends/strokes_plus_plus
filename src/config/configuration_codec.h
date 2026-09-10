#pragma once

#include <optional>
#include <string>

#include "config/configuration.h"
#include "config/json.h"

namespace strokes::config {

template <typename T>
struct DecodeResult {
  std::optional<T> value;
  std::string error;
  std::string warning;
  [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

[[nodiscard]] json::Value encode(const GlobalOptions& options);
[[nodiscard]] json::Value encode(const GestureFile& file);
[[nodiscard]] json::Value encode(const ProfileFile& file);
[[nodiscard]] DecodeResult<GlobalOptions> decode_options(const json::Value& value);
[[nodiscard]] DecodeResult<GestureFile> decode_gestures(const json::Value& value);
[[nodiscard]] DecodeResult<ProfileFile> decode_profiles(const json::Value& value);

}  // namespace strokes::config
