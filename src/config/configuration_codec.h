#pragma once

#include "config/configuration.h"
#include "config/json.h"

#include <optional>
#include <string>

namespace strokes::config {

template <typename T>
struct DecodeResult {
    std::optional<T> value;
    std::string error;
    [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

[[nodiscard]] json::Value encode(const GlobalOptions& options);
[[nodiscard]] json::Value encode(const GestureFile& file);
[[nodiscard]] json::Value encode(const ProfileFile& file);
[[nodiscard]] DecodeResult<GlobalOptions> decode_global_options(const json::Value& value);
[[nodiscard]] DecodeResult<GestureFile> decode_gesture_file(const json::Value& value);
[[nodiscard]] DecodeResult<ProfileFile> decode_profile_file(const json::Value& value);

}  // namespace strokes::config
