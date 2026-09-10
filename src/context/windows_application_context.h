#pragma once

#include "context/application_context_provider.h"

namespace strokes::context {

/// Captures UTF-8 process and window metadata for the current foreground window.
class WindowsApplicationContextProvider final : public IApplicationContextProvider {
 public:
  [[nodiscard]] std::optional<ApplicationContext> foreground_application() const override;
  [[nodiscard]] std::optional<ApplicationContext> application_for_window(
      std::uintptr_t window) const override;
};

}  // namespace strokes::context
