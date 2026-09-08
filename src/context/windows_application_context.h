#pragma once

#include "context/application_context_provider.h"

namespace strokes::context {

class WindowsApplicationContextProvider final : public IApplicationContextProvider {
public:
    [[nodiscard]] std::optional<ApplicationContext> foreground_application() const override;
};

}  // namespace strokes::context
