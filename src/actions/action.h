#pragma once

namespace strokes::actions {

enum class ActionType {
  keyboard_shortcut,
  process,
  url,
  mouse,
  window,
  media,
  volume,
  virtual_desktop,
  lua,
};

}  // namespace strokes::actions
