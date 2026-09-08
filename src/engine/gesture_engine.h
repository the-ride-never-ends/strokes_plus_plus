#pragma once

#include "actions/action_resolver.h"
#include "actions/keyboard_action.h"
#include "engine/gesture_feedback.h"
#include "context/application_context_provider.h"
#include "gestures/recognizer.h"
#include "input/gesture_state_machine.h"
#include "input/input_event.h"
#include "input/modifier_state_provider.h"
#include "input/mouse_click.h"

#include <optional>
#include <vector>

namespace strokes::engine {

struct EngineUpdate {
    input::GestureUpdate gesture;
    std::optional<gestures::RecognitionResult> recognition;
    std::optional<actions::ActionSource> action_source;
    std::string profile_id;
    bool click_replayed{};
    bool action_attempted{};
    bool action_succeeded{};
};

class GestureEngine {
public:
    GestureEngine(
        gestures::Recognizer& recognizer,
        const std::vector<context::ApplicationProfile>& profiles,
        const actions::ActionResolver::GlobalActions& global_actions,
        const context::IApplicationContextProvider& application_context,
        const input::IModifierStateProvider& modifier_state,
        input::IMouseClick& mouse_click,
        actions::IKeyboardInput& keyboard_input,
        input::GestureStateMachine state_machine = {},
        IGestureFeedback* feedback = nullptr);

    [[nodiscard]] EngineUpdate process(const input::MouseInputEvent& event);
    [[nodiscard]] input::GestureState state() const noexcept { return state_machine_.state(); }
    [[nodiscard]] const std::optional<input::GestureSession>& session() const noexcept {
        return state_machine_.session();
    }

private:
    [[nodiscard]] bool execute(const actions::Action& action);

    gestures::Recognizer& recognizer_;
    const std::vector<context::ApplicationProfile>& profiles_;
    const actions::ActionResolver::GlobalActions& global_actions_;
    const context::IApplicationContextProvider& application_context_;
    const input::IModifierStateProvider& modifier_state_;
    input::IMouseClick& mouse_click_;
    actions::IKeyboardInput& keyboard_input_;
    input::GestureStateMachine state_machine_;
    IGestureFeedback* feedback_{};
};

}  // namespace strokes::engine
