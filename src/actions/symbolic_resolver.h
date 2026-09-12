#pragma once

#include <cstdint>
#include <optional>

#include "actions/action_context.h"
#include "actions/action_definition.h"
#include "gestures/point.h"

namespace strokes::actions {

/// Resolves a configured point without substituting when its requested value is unavailable.
[[nodiscard]] std::optional<gestures::Point> resolve_position(
    const PositionDefinition& position, const ActionContext& context);

/// Resolves a configured HWND without falling back to another window.
[[nodiscard]] std::optional<std::uintptr_t> resolve_window(WindowTarget target,
                                                           const ActionContext& context);

}  // namespace strokes::actions
