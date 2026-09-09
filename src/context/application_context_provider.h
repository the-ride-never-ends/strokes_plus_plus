#pragma once

#include <optional>

#include "context/application_context.h"

namespace strokes::context {

/// Captures the foreground application at gesture activation time.
class IApplicationContextProvider {
 public:
  virtual ~IApplicationContextProvider() = default;
  [[nodiscard]] virtual std::optional<ApplicationContext> foreground_application() const = 0;
};

}  // namespace strokes::context
