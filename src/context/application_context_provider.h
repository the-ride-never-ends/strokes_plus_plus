#pragma once

#include "context/application_context.h"

#include <optional>

namespace strokes::context {

class IApplicationContextProvider {
public:
    virtual ~IApplicationContextProvider() = default;
    [[nodiscard]] virtual std::optional<ApplicationContext> foreground_application() const = 0;
};

}  // namespace strokes::context
