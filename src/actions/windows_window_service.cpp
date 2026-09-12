#include "actions/windows_window_service.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::actions {
namespace {

IWindowService::Bounds convert(RECT rectangle) {
  return {rectangle.left, rectangle.top, rectangle.right, rectangle.bottom,
          rectangle.right - rectangle.left, rectangle.bottom - rectangle.top};
}

ActionResult failed(std::string code, std::string message) {
  return ActionResult::failed(ActionError::platform_failure, std::move(code), std::move(message));
}

}  // namespace

std::optional<IWindowService::Bounds> WindowsWindowService::bounds(std::uintptr_t window) const {
  const auto handle = reinterpret_cast<HWND>(window);
  RECT rectangle{};
  if (!::IsWindow(handle) || !::GetWindowRect(handle, &rectangle)) return std::nullopt;
  return convert(rectangle);
}

std::optional<IWindowService::MonitorInfo> WindowsWindowService::monitor(
    std::uintptr_t window) const {
  const auto handle = reinterpret_cast<HWND>(window);
  if (!::IsWindow(handle)) return std::nullopt;
  const HMONITOR monitor = ::MonitorFromWindow(handle, MONITOR_DEFAULTTONULL);
  if (!monitor) return std::nullopt;
  MONITORINFOEXW information{};
  information.cbSize = sizeof(information);
  if (!::GetMonitorInfoW(monitor, &information)) return std::nullopt;
  const int utf8_size = ::WideCharToMultiByte(CP_UTF8, 0, information.szDevice, -1, nullptr, 0,
                                               nullptr, nullptr);
  std::string identifier;
  if (utf8_size > 1) {
    identifier.resize(static_cast<std::size_t>(utf8_size));
    (void)::WideCharToMultiByte(CP_UTF8, 0, information.szDevice, -1, identifier.data(), utf8_size,
                                nullptr, nullptr);
    identifier.pop_back();
  }
  return MonitorInfo{std::move(identifier), convert(information.rcMonitor),
                     convert(information.rcWork)};
}

ActionResult WindowsWindowService::perform(WindowOperation operation, std::uintptr_t window,
                                           const WindowParameters& parameters) {
  const auto handle = reinterpret_cast<HWND>(window);
  if (!::IsWindow(handle)) {
    return ActionResult::failed(ActionError::invalid_runtime_target, "window_not_found",
                                "The target window no longer exists.");
  }
  switch (operation) {
    case WindowOperation::close:
      return ::PostMessageW(handle, WM_CLOSE, 0, 0)
                 ? ActionResult::succeeded()
                 : failed("window_close_failed", "Windows rejected the close request.");
    case WindowOperation::minimize:
      (void)::ShowWindow(handle, SW_MINIMIZE);
      return ::IsIconic(handle) ? ActionResult::succeeded()
                                : failed("window_minimize_failed", "The window was not minimized.");
    case WindowOperation::maximize:
      (void)::ShowWindow(handle, SW_MAXIMIZE);
      return ::IsZoomed(handle) ? ActionResult::succeeded()
                                : failed("window_maximize_failed", "The window was not maximized.");
    case WindowOperation::restore:
      (void)::ShowWindow(handle, SW_RESTORE);
      return !::IsIconic(handle) && !::IsZoomed(handle)
                 ? ActionResult::succeeded()
                 : failed("window_restore_failed", "The window was not restored.");
    case WindowOperation::activate:
      if (::SetForegroundWindow(handle) && ::GetForegroundWindow() == handle)
        return ActionResult::succeeded();
      return failed("window_activation_denied",
                    "Windows prevented the target window from becoming foreground.");
    case WindowOperation::move:
    case WindowOperation::resize:
    case WindowOperation::move_resize: {
      const auto current = bounds(window);
      if (!current)
        return ActionResult::failed(ActionError::invalid_runtime_target, "window_not_found",
                                    "The target window no longer exists.");
      const int x = parameters.x.value_or(current->left);
      const int y = parameters.y.value_or(current->top);
      const int width = parameters.width.value_or(current->width);
      const int height = parameters.height.value_or(current->height);
      return ::SetWindowPos(handle, nullptr, x, y, width, height,
                            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER)
                 ? ActionResult::succeeded()
                 : failed("window_bounds_failed", "Windows rejected the window bounds change.");
    }
  }
  return ActionResult::failed(ActionError::unsupported_operation, "unknown_window_operation",
                              "The window operation is unsupported.");
}

}  // namespace strokes::actions
