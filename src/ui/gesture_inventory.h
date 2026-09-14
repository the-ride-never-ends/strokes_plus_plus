#pragma once

#include "config/configuration_store.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {

/// Read-only gallery of every configured gesture pattern, grouped by activation state.
class GestureInventory final {
 public:
  GestureInventory(HWND parent, HINSTANCE instance, config::ConfigurationBundle& configuration)
      : parent_(parent), instance_(instance), configuration_(&configuration) {}

  void create();
  void set_visible(bool visible) const noexcept;
  void refresh() const noexcept;

 private:
  static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
  LRESULT handle_message(UINT, WPARAM, LPARAM);
  void paint(HDC target) noexcept;
  void scroll_to(int position) noexcept;

  HWND parent_{};
  HINSTANCE instance_{};
  config::ConfigurationBundle* configuration_{};
  HWND window_{};
  int content_height_{};
};

}  // namespace strokes::ui
