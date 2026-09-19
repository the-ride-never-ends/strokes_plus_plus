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
class LuaRuntime final : public ILuaService {
 public:
  explicit LuaRuntime(std::chrono::milliseconds execution_limit = std::chrono::seconds(1));
  ~LuaRuntime() override;
  LuaRuntime(const LuaRuntime&) = delete;
  LuaRuntime& operator=(const LuaRuntime&) = delete;

  [[nodiscard]] ActionResult execute(std::string_view script,
                                     const ActionContext& context) override;
  [[nodiscard]] ActionResult initialize(std::string_view script);
  [[nodiscard]] ActionResult reload(std::string_view initialization_script);
  [[nodiscard]] ActionResult validate_script(std::string_view script);
  void set_module_directory(const std::filesystem::path& directory);
  void set_services(ActionServices services);
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
