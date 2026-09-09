#pragma once
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::tray {
enum class TrayCommand { enable, disable, settings, suspend, resume, exit };

/// Owns the notification-area icon and dispatches lifecycle commands.
class WindowsTrayIcon final {
 public:
  using Handler = void (*)(TrayCommand, void*) noexcept;
  ~WindowsTrayIcon();
  WindowsTrayIcon() = default;
  WindowsTrayIcon(const WindowsTrayIcon&) = delete;
  WindowsTrayIcon& operator=(const WindowsTrayIcon&) = delete;
  [[nodiscard]] bool create(HINSTANCE instance, Handler handler, void* context = nullptr);
  void destroy() noexcept;
  void set_enabled(bool enabled) noexcept;

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  void add_icon() noexcept;
  void show_menu() noexcept;
  HINSTANCE instance_{};
  HWND window_{};
  Handler handler_{};
  void* context_{};
  bool enabled_{true};
  bool class_registered_{};
  UINT taskbar_created_{};
};
}  // namespace strokes::tray
