#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "actions/action_definition.h"
#include "actions/action_context.h"
#include "actions/action_result.h"
#include "gestures/point.h"

namespace strokes::actions {

class IKeyboardService {
 public:
  virtual ~IKeyboardService() = default;
  [[nodiscard]] virtual ActionResult send_shortcut(std::string_view shortcut) = 0;
  [[nodiscard]] virtual ActionResult send_key(std::string_view, bool) {
    return ActionResult::failed(ActionError::unsupported_operation, "keyboard_state_unavailable",
                                "Individual keyboard events are unavailable.");
  }
  [[nodiscard]] virtual std::optional<bool> is_key_down(std::string_view) const {
    return std::nullopt;
  }
};

class IProcessService {
 public:
  virtual ~IProcessService() = default;
  [[nodiscard]] virtual ActionResult launch(const ProcessParameters& parameters) = 0;
};

class IShellService {
 public:
  virtual ~IShellService() = default;
  [[nodiscard]] virtual ActionResult open_uri(std::string_view uri) = 0;
};

class IMouseService {
 public:
  virtual ~IMouseService() = default;
  [[nodiscard]] virtual std::optional<gestures::Point> current_position() const = 0;
  [[nodiscard]] virtual ActionResult perform(MouseOperation operation,
                                             std::optional<MouseButton> button,
                                             gestures::Point position) = 0;
};

class IWindowService {
 public:
  struct Bounds {
    int left{};
    int top{};
    int right{};
    int bottom{};
    int width{};
    int height{};
  };
  struct MonitorInfo {
    std::string identifier;
    Bounds bounds;
    Bounds work_area;
  };

  virtual ~IWindowService() = default;
  [[nodiscard]] virtual ActionResult perform(WindowOperation operation, std::uintptr_t window,
                                             const WindowParameters& parameters) = 0;
  [[nodiscard]] virtual std::optional<Bounds> bounds(std::uintptr_t window) const = 0;
  [[nodiscard]] virtual std::optional<MonitorInfo> monitor(std::uintptr_t window) const = 0;
  [[nodiscard]] virtual bool exists(std::uintptr_t window) const {
    return bounds(window).has_value();
  }
  [[nodiscard]] virtual std::optional<std::string> title(std::uintptr_t) const {
    return std::nullopt;
  }
  [[nodiscard]] virtual std::optional<std::string> class_name(std::uintptr_t) const {
    return std::nullopt;
  }
  [[nodiscard]] virtual std::optional<std::string> process_name(std::uintptr_t) const {
    return std::nullopt;
  }
};

class IMediaService {
 public:
  virtual ~IMediaService() = default;
  [[nodiscard]] virtual ActionResult perform(MediaOperation operation) = 0;
};

class IAudioService {
 public:
  virtual ~IAudioService() = default;
  [[nodiscard]] virtual ActionResult perform(VolumeOperation operation,
                                             std::optional<double> amount) = 0;
  [[nodiscard]] virtual std::optional<double> volume() const { return std::nullopt; }
  [[nodiscard]] virtual ActionResult set_volume(double) {
    return ActionResult::failed(ActionError::unsupported_operation, "volume_set_unavailable",
                                "Setting volume is unavailable.");
  }
  [[nodiscard]] virtual std::optional<bool> is_muted() const { return std::nullopt; }
};

class IVirtualDesktopService {
 public:
  virtual ~IVirtualDesktopService() = default;
  [[nodiscard]] virtual ActionResult perform(VirtualDesktopOperation operation) = 0;
};

/// Executes scripts in the application-owned, managed Lua runtime.
class ILuaService {
 public:
  virtual ~ILuaService() = default;
  [[nodiscard]] virtual ActionResult execute(std::string_view script,
                                             const ActionContext& context) = 0;
};

class IDiagnosticService {
 public:
  virtual ~IDiagnosticService() = default;
  [[nodiscard]] virtual ActionResult write(std::string_view level,
                                           std::string_view message) = 0;
};

class IUserFeedbackService {
 public:
  virtual ~IUserFeedbackService() = default;
  [[nodiscard]] virtual ActionResult message(std::string_view text) = 0;
  [[nodiscard]] virtual ActionResult osd(std::string_view text) = 0;
};

struct ActionServices {
  IKeyboardService* keyboard{};
  IProcessService* process{};
  IShellService* shell{};
  IMouseService* mouse{};
  IWindowService* window{};
  IMediaService* media{};
  IAudioService* audio{};
  IVirtualDesktopService* virtual_desktop{};
  ILuaService* lua{};
  IDiagnosticService* diagnostics{};
  IUserFeedbackService* user_feedback{};
};

}  // namespace strokes::actions
