#pragma once

#include <atomic>
#include <chrono>
#include <filesystem>
#include <mutex>
#include <string_view>

#include "actions/action_services.h"

struct lua_State;

namespace strokes::actions {

/// Owns one persistent Lua 5.4 state and executes actions through protected calls.
///
/// Every member except request_cancel serializes on one mutex, so a caller on another
/// thread waits for the running script. request_cancel is the only member that may be
/// called while a script is executing.
class LuaRuntime final : public ILuaService {
 public:
  explicit LuaRuntime(std::chrono::milliseconds execution_limit = std::chrono::seconds(1));
  ~LuaRuntime() override;
  LuaRuntime(const LuaRuntime&) = delete;
  LuaRuntime& operator=(const LuaRuntime&) = delete;

  /// Runs one script against the values captured for the action.
  ///
  /// Args:
  ///   script: The Lua source assigned to the gesture.
  ///   context: The gesture, application and window values exposed to the script.
  /// Returns:
  ///   Success, or the syntax, runtime, cancellation or explicit-false failure.
  [[nodiscard]] ActionResult execute(std::string_view script,
                                     const ActionContext& context) override;
  /// Runs shared initialization code in the current runtime.
  ///
  /// Args:
  ///   script: The initialization source, which may be empty.
  [[nodiscard]] ActionResult initialize(std::string_view script);
  /// Replaces the runtime, clearing runtime-only state and cached modules.
  ///
  /// Args:
  ///   initialization_script: The initialization source to run in the new runtime.
  /// Returns:
  ///   The initialization result; the replacement runtime is usable either way.
  [[nodiscard]] ActionResult reload(std::string_view initialization_script);
  /// Compiles a script without running it.
  [[nodiscard]] ActionResult validate_script(std::string_view script);
  /// Sets the directory the restricted require implementation searches.
  void set_module_directory(const std::filesystem::path& directory);
  /// Replaces the automation services the bindings call.
  void set_services(ActionServices services);
  /// Interrupts the running script and refuses later ones until the next reload.
  void request_cancel() noexcept;

 private:
  lua_State* state_{};
  ActionServices services_;
  std::chrono::milliseconds execution_limit_;
  std::atomic_bool cancel_requested_{};
  std::mutex mutex_;
  std::filesystem::path module_directory_;
};

}  // namespace strokes::actions
