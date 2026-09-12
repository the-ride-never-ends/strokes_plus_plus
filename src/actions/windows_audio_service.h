#pragma once

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsAudioService final : public IAudioService {
 public:
  [[nodiscard]] static float adjusted_level(float current, VolumeOperation operation,
                                            std::optional<double> amount) noexcept;
  [[nodiscard]] ActionResult perform(VolumeOperation operation,
                                     std::optional<double> amount) override;
};

}  // namespace strokes::actions
