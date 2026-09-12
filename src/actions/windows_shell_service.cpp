#include "actions/windows_shell_service.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <shellapi.h>

#include <cstdint>
#include <string>

#include "actions/windows_text.h"

namespace strokes::actions {

ActionResult WindowsShellService::open_uri(std::string_view uri) {
  const auto wide = detail::decode_utf8(uri);
  if (!wide || wide->empty()) {
    return ActionResult::failed(ActionError::invalid_runtime_target, "invalid_uri_text",
                                "The URI is empty or invalid UTF-8.");
  }
  const auto result = opener_(*wide);
  if (result <= 32) {
    return ActionResult::failed(ActionError::platform_failure, "uri_open_failed",
                                "Windows could not open the URI (shell result " +
                                    std::to_string(result) + ").");
  }
  return ActionResult::succeeded();
}

std::intptr_t WindowsShellService::default_open(std::wstring_view uri) {
  const std::wstring terminated(uri);
  return reinterpret_cast<std::intptr_t>(
      ::ShellExecuteW(nullptr, L"open", terminated.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
}

}  // namespace strokes::actions
