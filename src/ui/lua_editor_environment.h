#pragma once

#include <filesystem>
#include <string>

#include "actions/action_services.h"

namespace strokes::ui {

/// The automation environment the action editor validates and tests Lua scripts against.
struct LuaEditorEnvironment {
  actions::ActionServices services;
  std::filesystem::path module_directory;
  std::string initialization_script;
};

/// Installed by the application host while the settings window is open, and cleared when
/// it closes. Written and read only on the thread that owns the settings window.
inline const LuaEditorEnvironment* lua_environment = nullptr;

}  // namespace strokes::ui
