#pragma once

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsUserFeedbackService final : public IUserFeedbackService {
 public:
  [[nodiscard]] ActionResult message(std::string_view text) override;
  [[nodiscard]] ActionResult osd(std::string_view text) override;
};

}  // namespace strokes::actions
