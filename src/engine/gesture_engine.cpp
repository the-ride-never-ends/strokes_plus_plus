#include "engine/gesture_engine.h"

#include <utility>

namespace strokes::engine {

GestureEngine::GestureEngine(gestures::Recognizer& recognizer,
                             const std::vector<context::ApplicationProfile>& profiles,
                             const actions::ActionResolver::GlobalActions& global_actions,
                             const context::IApplicationContextProvider& application_context,
                             const input::IModifierStateProvider& modifier_state,
                             input::IMouseClick& mouse_click,
                             actions::IKeyboardInput& keyboard_input,
                             input::GestureStateMachine state_machine, IGestureFeedback* feedback,
                             actions::ActionServices services)
    : recognizer_(recognizer),
      profiles_(profiles),
      global_actions_(global_actions),
      application_context_(application_context),
      modifier_state_(modifier_state),
      mouse_click_(mouse_click),
      keyboard_service_(keyboard_input),
      services_(services),
      state_machine_(std::move(state_machine)),
      feedback_(feedback) {
  services_.keyboard = &keyboard_service_;
  action_executor_ = std::make_unique<actions::ActionExecutor>(services_);
}

GestureEngine::~GestureEngine() {
  if (feedback_) feedback_->hide();
  if (state_machine_.state() == input::GestureState::button_pending ||
      state_machine_.state() == input::GestureState::capturing) {
    (void)state_machine_.cancel();
    (void)state_machine_.cancellation_finished();
  }
}

EngineUpdate GestureEngine::process(const input::MouseInputEvent& event) {
  EngineUpdate result;
  const auto click_position =
      state_machine_.session() ? state_machine_.session()->start_position : event.position;

  switch (event.type) {
    case input::MouseEventType::button_down: {
      input::GestureStart start;
      start.activation_button = event.button;
      start.position = event.position;
      start.modifiers = modifier_state_.current_modifiers();
      const auto application = event.target_window != 0
                                   ? application_context_.window_application(event.target_window)
                                   : application_context_.foreground_application();
      if (application) {
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

  if (feedback_) {
    if (result.gesture.capture_started && state_machine_.session())
      feedback_->show(state_machine_.session()->captured_points);
    else if (event.type == input::MouseEventType::pointer_moved &&
             state_machine_.state() == input::GestureState::capturing && state_machine_.session())
      feedback_->update(state_machine_.session()->captured_points);
    if (result.gesture.recognition_requested || result.gesture.capture_cancelled) feedback_->hide();
  }

  if (result.gesture.emulate_click) {
    result.click_replayed = mouse_click_.click(event.button, click_position);
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

  const auto resolved = actions::ActionResolver::resolve(result.recognition->gesture_id,
                                                         state_machine_.session()->application,
                                                         profiles_, global_actions_);
  if (!resolved) {
    (void)state_machine_.recognition_finished(false);
    return result;
  }

  result.action_source = resolved->source;
  result.profile_id = resolved->profile_id;
  result.action_type = actions::action_type_name(resolved->action.type);
  result.action_operation = actions::action_operation_name(resolved->action);
  result.action_target = actions::action_target_name(resolved->action);
  result.action_attempted = true;
  (void)state_machine_.recognition_finished(true);
  result.action_result = execute(resolved->action);
  result.action_succeeded = result.action_result->success;
  (void)state_machine_.execution_finished();
  return result;
}

actions::ActionResult GestureEngine::execute(const actions::ActionDefinition& definition) {
  const auto& session = *state_machine_.session();
  actions::ActionContext context;
  context.gesture = session;
  context.application = session.application;
  context.current_cursor_position =
      services_.mouse ? services_.mouse->current_position() : std::nullopt;
  if (session.application.window_handle != 0) {
    context.gesture_window = session.application.window_handle;
    context.window_at_gesture_start = session.application.window_handle;
  }
  if (const auto foreground = application_context_.foreground_application();
      foreground && foreground->window_handle != 0)
    context.foreground_window = foreground->window_handle;
  if (session.application.process_id != 0) context.target_process_id = session.application.process_id;

  return action_executor_->submit(definition, std::move(context)).get();
}

}  // namespace strokes::engine
