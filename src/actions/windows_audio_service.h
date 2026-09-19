#pragma once

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsAudioService final : public IAudioService {
 public:
  [[nodiscard]] static float adjusted_level(float current, VolumeOperation operation,
                                            std::optional<double> amount) noexcept;
  [[nodiscard]] ActionResult perform(VolumeOperation operation,
                                     std::optional<double> amount) override;
  [[nodiscard]] std::optional<double> volume() const override;
  [[nodiscard]] ActionResult set_volume(double value) override;
  [[nodiscard]] std::optional<bool> is_muted() const override;
};

}  // namespace strokes::actions
