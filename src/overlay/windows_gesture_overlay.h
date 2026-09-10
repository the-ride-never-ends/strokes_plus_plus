#pragma once

#include "engine/gesture_feedback.h"
#include "gestures/stroke.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <mutex>
#include <atomic>

namespace strokes::overlay {

/// Renders gesture feedback in a click-through buffered virtual-desktop window.
class WindowsGestureOverlay final : public engine::IGestureFeedback {
 public:
  struct Options {
    bool enabled{true};
    int line_width{4};
    BYTE opacity{217};
    COLORREF color{RGB(0, 160, 255)};
  };

  WindowsGestureOverlay() = default;
  ~WindowsGestureOverlay();
  WindowsGestureOverlay(const WindowsGestureOverlay&) = delete;
  WindowsGestureOverlay& operator=(const WindowsGestureOverlay&) = delete;

  [[nodiscard]] bool create(HINSTANCE instance, Options options = {});
  void destroy() noexcept;
  void show(const gestures::Stroke& points) override;
  void update(const gestures::Stroke& points) override;
  void hide() noexcept override;
  void configure(Options options) noexcept;

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  void paint() noexcept;
  void resize_screen() noexcept;
  void create_buffer() noexcept;
  void destroy_buffer() noexcept;
  void draw_segments() noexcept;
  void clear_buffer() noexcept;

  HINSTANCE instance_{};
  HWND window_{};
  bool class_registered_{};
  HDC memory_dc_{};
  HBITMAP bitmap_{};
  HGDIOBJ previous_bitmap_{};
  int origin_x_{};
  int origin_y_{};
  int width_{};
  int height_{};
  Options options_;
  std::mutex points_mutex_;
  gestures::Stroke points_;
  std::size_t painted_points_{};
  std::atomic<unsigned long> generation_{};
  std::atomic<bool> refresh_pending_{};
};

}  // namespace strokes::overlay
