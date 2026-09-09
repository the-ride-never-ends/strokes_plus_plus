#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace strokes::config::json {

struct Value;
using Array = std::vector<Value>;
using Object = std::map<std::string, Value, std::less<>>;

struct Value {
  using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
  Storage data{nullptr};

  Value() = default;
  Value(std::nullptr_t) : data(nullptr) {}
  Value(bool value) : data(value) {}
  Value(double value) : data(value) {}
  Value(std::string value) : data(std::move(value)) {}
  Value(const char* value) : data(std::string(value)) {}
  Value(Array value) : data(std::move(value)) {}
  Value(Object value) : data(std::move(value)) {}

  template <typename T>
  [[nodiscard]] const T* get_if() const noexcept {
    return std::get_if<T>(&data);
  }
  template <typename T>
  [[nodiscard]] T* get_if() noexcept {
    return std::get_if<T>(&data);
  }
};

struct ParseError {
  std::size_t offset{};
  std::string message;
};

struct ParseResult {
  std::optional<Value> value;
  std::optional<ParseError> error;
  [[nodiscard]] explicit operator bool() const noexcept { return value.has_value(); }
};

[[nodiscard]] ParseResult parse(std::string_view text);
[[nodiscard]] std::string serialize(const Value& value, bool pretty = true);

}  // namespace strokes::config::json
