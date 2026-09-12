#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <vector>

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsMediaService final : public IMediaService {
 public:
  using Sender = std::function<UINT(UINT, INPUT*, int)>;
  explicit WindowsMediaService(Sender sender = ::SendInput) : sender_(std::move(sender)) {}
  [[nodiscard]] ActionResult perform(MediaOperation operation) override;
  [[nodiscard]] static std::vector<INPUT> make_inputs(MediaOperation operation);

 private:
  Sender sender_;
};

}  // namespace strokes::actions
