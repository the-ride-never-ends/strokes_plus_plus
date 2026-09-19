#include "actions/windows_user_feedback_service.h"

#include "ui/settings_controls.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::actions {

ActionResult WindowsUserFeedbackService::message(std::string_view text) {
  ::MessageBoxW(nullptr, ui::detail::wide(text).c_str(), L"Strokes++",
                MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND);
  return ActionResult::succeeded();
}

ActionResult WindowsUserFeedbackService::osd(std::string_view text) {
  using MessageBoxTimeout = int(WINAPI*)(HWND, LPCWSTR, LPCWSTR, UINT, WORD, DWORD);
  const auto function = reinterpret_cast<MessageBoxTimeout>(
      ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "MessageBoxTimeoutW"));
  if (!function)
    return ActionResult::failed(ActionError::unsupported_operation, "osd_unavailable",
                                "Timed on-screen messages are unavailable.");
  (void)function(nullptr, ui::detail::wide(text).c_str(), L"Strokes++",
                 MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND, 0, 1500);
  return ActionResult::succeeded();
}

}  // namespace strokes::actions
