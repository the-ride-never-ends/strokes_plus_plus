#pragma once

#include <optional>

#include "actions/action_definition.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {

struct ActionEditResult {
  bool accepted{};
  std::optional<actions::ActionDefinition> action;
};

/// Modal editor for every persisted built-in and Lua action definition.
class WindowsActionEditor {
 public:
  [[nodiscard]] ActionEditResult edit(HINSTANCE instance, HWND owner,
                                      const actions::ActionDefinition* existing);

 private:
  static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wp, LPARAM lp);
  LRESULT handle_message(UINT message, WPARAM wp, LPARAM lp);
  void create_controls();
  void load(const actions::ActionDefinition* existing);
  void refresh(bool reset_choices);
  void browse_executable();
  void browse_directory();
  void validate_lua();
  void test_lua();
  void show_lua_help();
  void rescale_children(UINT old_dpi, UINT new_dpi) noexcept;
  [[nodiscard]] std::optional<actions::ActionDefinition> read() const;
  void finish(bool accepted, bool remove = false);

  HWND window_{};
  HWND owner_{};
  HINSTANCE instance_{};
  bool finished_{};
  UINT dpi_{96};
  ActionEditResult result_;
};

}  // namespace strokes::ui
