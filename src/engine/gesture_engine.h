#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "actions/action_resolver.h"
#include "actions/action_result.h"
#include "actions/action_executor.h"
#include "actions/keyboard_action.h"
#include "actions/keyboard_service.h"
#include "actions/lua_runtime.h"
#include "context/application_context_provider.h"
#include "engine/gesture_feedback.h"
#include "gestures/recognizer.h"
#include "input/gesture_state_machine.h"
#include "input/input_event.h"
#include "input/modifier_state_provider.h"
#include "input/mouse_click.h"

namespace strokes::engine {

struct EngineUpdate {
  input::GestureUpdate gesture;
  std::optional<gestures::RecognitionResult> recognition;
  std::optional<actions::ActionSource> action_source;
  std::string profile_id;
  bool click_replayed{};
  bool action_attempted{};
  bool action_succeeded{};
  std::optional<actions::ActionResult> action_result;
  std::string action_type;
  std::string action_operation;
  std::string action_target;
};

/// Coordinates gesture state, recognition, action resolution, feedback, and execution.
class GestureEngine {
 public:
  GestureEngine(gestures::Recognizer& recognizer,
                const std::vector<context::ApplicationProfile>& profiles,
                const actions::ActionResolver::GlobalActions& global_actions,
                const context::IApplicationContextProvider& application_context,
                const input::IModifierStateProvider& modifier_state,
                input::IMouseClick& mouse_click, actions::IKeyboardInput& keyboard_input,
                input::GestureStateMachine state_machine = {},
                IGestureFeedback* feedback = nullptr,
                actions::ActionServices services = {},
                std::string_view lua_initialization_script = {},
                const std::filesystem::path& lua_module_directory = {});
  ~GestureEngine();

  [[nodiscard]] EngineUpdate process(const input::MouseInputEvent& event);
  [[nodiscard]] input::GestureState state() const noexcept { return state_machine_.state(); }
  [[nodiscard]] const std::optional<input::GestureSession>& session() const noexcept {
    return state_machine_.session();
  }

 private:
  [[nodiscard]] actions::ActionResult execute(const actions::ActionDefinition& action);
  [[nodiscard]] actions::ActionResult execute(const actions::ActionDefinition& action,
                                              const input::GestureSession& session,
                                              std::optional<gestures::RecognitionResult> recognition =
                                                  std::nullopt);

  gestures::Recognizer& recognizer_;
  const std::vector<context::ApplicationProfile>& profiles_;
  const actions::ActionResolver::GlobalActions& global_actions_;
  const context::IApplicationContextProvider& application_context_;
  const input::IModifierStateProvider& modifier_state_;
  input::IMouseClick& mouse_click_;
  actions::KeyboardService keyboard_service_;
  actions::LuaRuntime lua_runtime_;
  actions::ActionServices services_;
  std::unique_ptr<actions::ActionExecutor> action_executor_;
  input::GestureStateMachine state_machine_;
  IGestureFeedback* feedback_{};
};

}  // namespace strokes::engine
