#include "actions/windows_process_service.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <string>
#include <vector>

#include "actions/windows_text.h"

namespace strokes::actions {
namespace {

std::wstring quote_executable(const std::wstring& path) {
  std::wstring result = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t character : path) {
    if (character == L'\\') {
      ++backslashes;
    } else if (character == L'\"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(character);
      backslashes = 0;
    } else {
      result.append(backslashes, L'\\');
      backslashes = 0;
      result.push_back(character);
    }
  }
  result.append(backslashes * 2, L'\\');
  result.push_back(L'\"');
  return result;
}

std::optional<std::wstring> resolve_executable(const std::wstring& path) {
  const DWORD needed = ::SearchPathW(nullptr, path.c_str(), nullptr, 0, nullptr, nullptr);
  if (needed == 0) return std::nullopt;
  std::vector<wchar_t> buffer(static_cast<std::size_t>(needed) + 1);
  const DWORD copied = ::SearchPathW(nullptr, path.c_str(), nullptr,
                                     static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
  if (copied == 0 || copied >= buffer.size()) return std::nullopt;
  return std::wstring(buffer.data(), copied);
}

}  // namespace

ActionResult WindowsProcessService::launch(const ProcessParameters& parameters) {
  const auto path = detail::decode_utf8(parameters.path);
  const auto arguments = detail::decode_utf8(parameters.arguments);
  const auto working_directory = detail::decode_utf8(parameters.working_directory);
  if (!path || path->empty() || !arguments || !working_directory) {
    return ActionResult::failed(ActionError::invalid_runtime_target, "invalid_process_text",
                                "The executable path or launch parameters are invalid UTF-8.");
  }
  const auto executable = resolve_executable(*path);
  if (!executable) {
    return ActionResult::failed(ActionError::invalid_runtime_target, "executable_not_found",
                                "The executable does not exist or is not available on PATH.");
  }

  std::wstring command_line = quote_executable(*executable);
  if (!arguments->empty()) {
    command_line.push_back(L' ');
    command_line.append(*arguments);
  }
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  const wchar_t* working = working_directory->empty() ? nullptr : working_directory->c_str();
  if (!::CreateProcessW(executable->c_str(), command_line.data(), nullptr, nullptr, FALSE, 0, nullptr,
                        working, &startup, &process)) {
    return ActionResult::failed(ActionError::platform_failure, "process_launch_failed",
                                "Windows could not launch the executable (error " +
                                    std::to_string(::GetLastError()) + ").");
  }
  ::CloseHandle(process.hThread);
  ::CloseHandle(process.hProcess);
  return ActionResult::succeeded();
}

}  // namespace strokes::actions
