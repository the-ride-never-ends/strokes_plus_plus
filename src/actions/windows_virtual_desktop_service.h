#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <functional>
#include <vector>

#include "actions/action_services.h"

namespace strokes::actions {

class WindowsVirtualDesktopService final : public IVirtualDesktopService {
 public:
  using Sender = std::function<UINT(UINT, INPUT*, int)>;
  explicit WindowsVirtualDesktopService(Sender sender = ::SendInput) : sender_(std::move(sender)) {}
  [[nodiscard]] ActionResult perform(VirtualDesktopOperation operation) override;
  [[nodiscard]] static std::vector<INPUT> make_inputs(VirtualDesktopOperation operation);

 private:
  Sender sender_;
};

}  // namespace strokes::actions
