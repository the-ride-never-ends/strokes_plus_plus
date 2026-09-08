#include "engine/gesture_engine.h"

#include "actions/keyboard_shortcut.h"

#include <utility>

namespace strokes::engine {

GestureEngine::GestureEngine(
    gestures::Recognizer& recognizer,
    const std::vector<context::ApplicationProfile>& profiles,
    const actions::ActionResolver::GlobalActions& global_actions,
    const context::IApplicationContextProvider& application_context,
    const input::IModifierStateProvider& modifier_state,
    input::IMouseClick& mouse_click,
    actions::IKeyboardInput& keyboard_input,
    input::GestureStateMachine state_machine,
    IGestureFeedback* feedback)
    : recognizer_(recognizer),
      profiles_(profiles),
      global_actions_(global_actions),
      application_context_(application_context),
      modifier_state_(modifier_state),
      mouse_click_(mouse_click),
      keyboard_input_(keyboard_input),
      state_machine_(std::move(state_machine)),
      feedback_(feedback) {}

EngineUpdate GestureEngine::process(const input::MouseInputEvent& event) {
    EngineUpdate result;

    switch (event.type) {
    case input::MouseEventType::button_down: {
        input::GestureStart start;
        start.activation_button = event.button;
        start.position = event.position;
        start.modifiers = modifier_state_.current_modifiers();
        if (const auto application = application_context_.foreground_application()) {
            start.application = *application;
        }
        result.gesture = state_machine_.button_down(std::move(start));
        break;
    }
    case input::MouseEventType::pointer_moved:
        result.gesture = state_machine_.pointer_moved(event.position);
        break;
    case input::MouseEventType::button_up:
        result.gesture = state_machine_.button_up(event.position);
        break;
    case input::MouseEventType::cancel:
        result.gesture = state_machine_.cancel();
        if (result.gesture.capture_cancelled) {
            (void)state_machine_.cancellation_finished();
        }
        break;
    }

    if(feedback_){
        if(result.gesture.capture_started&&state_machine_.session())
            feedback_->show(state_machine_.session()->captured_points);
        else if(event.type==input::MouseEventType::pointer_moved&&
                state_machine_.state()==input::GestureState::capturing&&state_machine_.session())
            feedback_->update(state_machine_.session()->captured_points);
        if(result.gesture.recognition_requested||result.gesture.capture_cancelled)feedback_->hide();
    }

    if (result.gesture.emulate_click) {
        result.click_replayed = mouse_click_.click(event.button);
        return result;
    }
    if (!result.gesture.recognition_requested) {
        return result;
    }

    result.recognition = recognizer_.recognize(state_machine_.captured_points());
    if (!result.recognition || !state_machine_.session()) {
        (void)state_machine_.recognition_finished(false);
        return result;
    }

    const auto resolved = actions::ActionResolver::resolve(
        result.recognition->gesture_id,
        state_machine_.session()->application,
        profiles_,
        global_actions_);
    if (!resolved) {
        (void)state_machine_.recognition_finished(false);
        return result;
    }

    result.action_source = resolved->source;
    result.profile_id = resolved->profile_id;
    result.action_attempted = true;
    (void)state_machine_.recognition_finished(true);
    result.action_succeeded = execute(resolved->action);
    (void)state_machine_.execution_finished();
    return result;
}

bool GestureEngine::execute(const actions::Action& action) {
    switch (action.type) {
    case actions::ActionType::keyboard_shortcut:
        if (const auto shortcut = actions::parse_keyboard_shortcut(action.value)) {
            return actions::KeyboardActionExecutor::execute(*shortcut, keyboard_input_);
        }
        return false;
    }
    return false;
}

}  // namespace strokes::engine
