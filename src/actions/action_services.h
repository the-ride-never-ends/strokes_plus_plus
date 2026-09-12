#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "actions/action_definition.h"
#include "actions/action_result.h"
#include "gestures/point.h"

namespace strokes::actions {

class IKeyboardService {
 public:
  virtual ~IKeyboardService() = default;
  [[nodiscard]] virtual ActionResult send_shortcut(std::string_view shortcut) = 0;
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
};

class IVirtualDesktopService {
 public:
  virtual ~IVirtualDesktopService() = default;
  [[nodiscard]] virtual ActionResult perform(VirtualDesktopOperation operation) = 0;
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
};

}  // namespace strokes::actions
