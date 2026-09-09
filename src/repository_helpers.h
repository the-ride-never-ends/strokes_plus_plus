#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace strokes::detail {

/// Finds an identity-bearing model in a repository vector.
template <typename Item>
Item* find_id(std::vector<Item>& items, const std::string& id) noexcept {
  const auto iterator =
      std::ranges::find_if(items, [&](const auto& item) { return item.id == id; });
  return iterator == items.end() ? nullptr : &*iterator;
}

/// Verifies that a prospective repository identity is nonempty and unused.
template <typename Item>
bool unique_id(const std::vector<Item>& items, const std::string& id) {
  return !id.empty() &&
         std::ranges::none_of(items, [&](const auto& item) { return item.id == id; });
}

/// Verifies that a prospective repository display name is nonempty and unused.
template <typename Item>
bool unique_name(const std::vector<Item>& items, const std::string& name,
                 const std::string* except_id = nullptr) {
  return !name.empty() && std::ranges::none_of(items, [&](const auto& item) {
    return item.name == name && (except_id == nullptr || item.id != *except_id);
  });
}

/// Erases exactly one model with the requested identity.
template <typename Item>
bool erase_id(std::vector<Item>& items, const std::string& id) {
  return std::erase_if(items, [&](const auto& item) { return item.id == id; }) == 1;
}

}  // namespace strokes::detail
