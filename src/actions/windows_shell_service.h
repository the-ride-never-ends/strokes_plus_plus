#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstdint>
#include <functional>
#include <string_view>
#include <utility>

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsShellService final : public IShellService {
 public:
  using Opener = std::function<std::intptr_t(std::wstring_view)>;
  explicit WindowsShellService(Opener opener = default_open) : opener_(std::move(opener)) {}
  [[nodiscard]] ActionResult open_uri(std::string_view uri) override;

 private:
  static std::intptr_t default_open(std::wstring_view uri);
  Opener opener_;
};

}  // namespace strokes::actions
