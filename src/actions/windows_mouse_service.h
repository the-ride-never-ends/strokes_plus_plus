#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <utility>
#include <vector>

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsMouseService final : public IMouseService {
 public:
  using Sender = std::function<UINT(UINT, INPUT*, int)>;
  explicit WindowsMouseService(Sender sender = ::SendInput) : sender_(std::move(sender)) {}

  [[nodiscard]] std::optional<gestures::Point> current_position() const override;
  [[nodiscard]] ActionResult perform(MouseOperation operation,
                                     std::optional<MouseButton> button,
                                     gestures::Point position) override;
  [[nodiscard]] static std::vector<INPUT> make_inputs(MouseOperation operation,
                                                       std::optional<MouseButton> button,
                                                       gestures::Point position,
                                                       RECT virtual_screen);

 private:
  Sender sender_;
};

}  // namespace strokes::actions
