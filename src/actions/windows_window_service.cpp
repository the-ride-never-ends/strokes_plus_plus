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
    case WindowOperation::toggle_maximize_restore: {
      const bool was_maximized = ::IsZoomed(handle) != FALSE;
      (void)::ShowWindow(handle, was_maximized ? SW_RESTORE : SW_MAXIMIZE);
      const bool changed = was_maximized ? !::IsZoomed(handle) : ::IsZoomed(handle);
      return changed ? ActionResult::succeeded()
                     : failed("window_toggle_failed", "The window state did not change.");
    }
    case WindowOperation::center: {
      RECT rectangle{};
      MONITORINFO monitor_info{};
      monitor_info.cbSize = sizeof(monitor_info);
      const HMONITOR monitor = ::MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
      if (!::GetWindowRect(handle, &rectangle) || !::GetMonitorInfoW(monitor, &monitor_info))
        return failed("window_center_failed", "The window bounds could not be read.");
      const int width = rectangle.right - rectangle.left;
      const int height = rectangle.bottom - rectangle.top;
      const int x = monitor_info.rcWork.left + (monitor_info.rcWork.right - monitor_info.rcWork.left - width) / 2;
      const int y = monitor_info.rcWork.top + (monitor_info.rcWork.bottom - monitor_info.rcWork.top - height) / 2;
      return ::SetWindowPos(handle, nullptr, x, y, 0, 0,
                            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_NOSIZE)
                 ? ActionResult::succeeded()
                 : failed("window_center_failed", "Windows rejected the centering request.");
    }
    case WindowOperation::activate:
      if (::SetForegroundWindow(handle) && ::GetForegroundWindow() == handle)
        return ActionResult::succeeded();
      return failed("window_activation_denied",
                    "Windows prevented the target window from becoming foreground.");
    case WindowOperation::move:
    case WindowOperation::resize:
    case WindowOperation::move_resize: {
      const bool moves = operation == WindowOperation::move ||
                         operation == WindowOperation::move_resize;
      const bool resizes = operation == WindowOperation::resize ||
                           operation == WindowOperation::move_resize;
      if ((moves && (!parameters.x || !parameters.y)) ||
          (resizes && (!parameters.width || !parameters.height))) {
        return ActionResult::failed(ActionError::invalid_definition, "incomplete_window_bounds",
                                    "The window operation is missing required bounds.");
      }
      UINT flags = SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER;
      int x = 0;
      int y = 0;
      int width = 0;
      int height = 0;
      if (operation == WindowOperation::move) {
        x = *parameters.x;
        y = *parameters.y;
        flags |= SWP_NOSIZE;
      } else if (operation == WindowOperation::resize) {
        width = *parameters.width;
        height = *parameters.height;
        flags |= SWP_NOMOVE;
      } else {
        x = *parameters.x;
        y = *parameters.y;
        width = *parameters.width;
        height = *parameters.height;
      }
      return ::SetWindowPos(handle, nullptr, x, y, width, height, flags)
                 ? ActionResult::succeeded()
                 : failed("window_bounds_failed", "Windows rejected the window bounds change.");
    }
  }
  return ActionResult::failed(ActionError::unsupported_operation, "unknown_window_operation",
                              "The window operation is unsupported.");
}

}  // namespace strokes::actions
