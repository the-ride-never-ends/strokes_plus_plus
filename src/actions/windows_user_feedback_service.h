#pragma once

#include <atomic>
#include <future>
#include <string_view>
#include <thread>

#include "actions/action_services.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::actions {

/// Shows short messages in a layered window owned by its own message-loop thread.
///
/// Both members hand the text to that thread and return immediately, so a script never
/// waits for the user and feedback cannot hold the action worker or delay shutdown.
class WindowsUserFeedbackService final : public IUserFeedbackService {
 public:
  WindowsUserFeedbackService();
  ~WindowsUserFeedbackService() override;
  WindowsUserFeedbackService(const WindowsUserFeedbackService&) = delete;
  WindowsUserFeedbackService& operator=(const WindowsUserFeedbackService&) = delete;

  [[nodiscard]] ActionResult message(std::string_view text) override;
  [[nodiscard]] ActionResult osd(std::string_view text) override;

 private:
  void run(std::promise<void>& ready) noexcept;
  [[nodiscard]] ActionResult show(std::string_view text, UINT duration);

  std::atomic<HWND> window_{};
  std::thread worker_;
};

}  // namespace strokes::actions
