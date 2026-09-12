#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "actions/action_definition.h"
#include "actions/action_executor.h"
#include "actions/action_factory.h"
#include "actions/action_result.h"
#include "actions/symbolic_resolver.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using namespace actions;

class RecordingServices final : public IKeyboardService,
                                public IProcessService,
                                public IShellService,
                                public IMouseService,
                                public IWindowService,
                                public IMediaService,
                                public IAudioService,
                                public IVirtualDesktopService {
 public:
  int calls{};
  gestures::Point point;
  std::uintptr_t window{};

  std::optional<gestures::Point> current_position() const override {
    return gestures::Point{70, 80};
  }
  ActionResult send_shortcut(std::string_view) override { return called(); }
  ActionResult launch(const ProcessParameters&) override { return called(); }
  ActionResult open_uri(std::string_view) override { return called(); }
  ActionResult perform(MouseOperation, std::optional<MouseButton>, gestures::Point value) override {
    point = value;
    return called();
  }
  ActionResult perform(WindowOperation, std::uintptr_t value,
                       const WindowParameters&) override {
    window = value;
    return called();
  }
  std::optional<Bounds> bounds(std::uintptr_t) const override { return Bounds{}; }
  std::optional<MonitorInfo> monitor(std::uintptr_t) const override { return MonitorInfo{}; }
  ActionResult perform(MediaOperation) override { return called(); }
  ActionResult perform(VolumeOperation, std::optional<double>) override { return called(); }
  ActionResult perform(VirtualDesktopOperation) override { return called(); }

 private:
  ActionResult called() {
    ++calls;
    return ActionResult::succeeded();
  }
};

class QueueKeyboard final : public IKeyboardService {
 public:
  bool throws{};
  std::vector<std::string> shortcuts;
  std::mutex mutex;

  ActionResult send_shortcut(std::string_view shortcut) override {
    if (throws) throw std::runtime_error("injected service exception");
    std::lock_guard lock(mutex);
    shortcuts.emplace_back(shortcut);
    return ActionResult::succeeded();
  }
};

void supported_definition_tests() {
  check(validate({1, ActionType::keyboard_shortcut, KeyboardParameters{"CTRL+W"}}).valid,
        "keyboard definitions validate");
  check(validate({1, ActionType::process, ProcessParameters{ProcessOperation::launch, "tool.exe",
                                                            "--flag", "C:\\Tools"}})
            .valid,
        "process definitions retain optional launch parameters");
  check(validate({1, ActionType::url, UrlParameters{"ms-settings:display"}}).valid,
        "registered non-HTTP URI definitions validate");
  check(validate({1, ActionType::mouse,
                  MouseParameters{MouseOperation::click, MouseButton::x_button_2,
                                  {PositionTarget::gesture_start, std::nullopt}}})
            .valid,
        "contextual mouse definitions validate");
  check(validate({1, ActionType::mouse,
                  MouseParameters{MouseOperation::move, std::nullopt,
                                  {PositionTarget::absolute, gestures::Point{-500, 200}}}})
            .valid,
        "absolute mouse definitions accept negative virtual-desktop coordinates");
  check(validate({1, ActionType::window,
                  WindowParameters{WindowOperation::move_resize,
                                   WindowTarget::window_at_gesture_start, -1200, 20, 800, 600}})
            .valid,
        "complete move-resize definitions validate");
  check(validate({1, ActionType::media, MediaParameters{MediaOperation::next_track}}).valid,
        "media definitions validate");
  check(validate({1, ActionType::volume,
                  VolumeParameters{VolumeOperation::increase, 5.0}})
            .valid,
        "volume definitions validate");
  check(validate({1, ActionType::virtual_desktop,
                  VirtualDesktopParameters{VirtualDesktopOperation::previous}})
            .valid,
        "virtual desktop definitions validate");
}

void invalid_definition_tests() {
  check(!validate({2, ActionType::keyboard_shortcut, KeyboardParameters{"CTRL+W"}}).valid,
        "unknown schema versions are rejected");
  check(!validate({1, ActionType::keyboard_shortcut, KeyboardParameters{"CTRL+"}}).valid,
        "invalid shortcuts are rejected");
  check(!validate({1, ActionType::process, ProcessParameters{}}).valid,
        "process definitions require an executable");
  check(!validate({1, ActionType::url, UrlParameters{"example.com"}}).valid,
        "URI definitions require a scheme");
  check(!validate({1, ActionType::mouse,
                   MouseParameters{MouseOperation::click, std::nullopt, {}}})
             .valid,
        "mouse clicks require a button");
  check(!validate({1, ActionType::mouse,
                   MouseParameters{MouseOperation::move, std::nullopt,
                                   {PositionTarget::absolute, std::nullopt}}})
             .valid,
        "absolute positions require coordinates");
  check(!validate({1, ActionType::window,
                   WindowParameters{WindowOperation::resize, WindowTarget::gesture_window,
                                    std::nullopt, std::nullopt, 0, 600}})
             .valid,
        "window dimensions must be positive");
  check(!validate({1, ActionType::volume,
                   VolumeParameters{VolumeOperation::increase,
                                    std::numeric_limits<double>::infinity()}})
             .valid,
        "volume amounts must be finite and bounded");
  check(!validate({1, ActionType::media, KeyboardParameters{"CTRL+W"}}).valid,
        "action types reject mismatched parameter variants");
  check(!validate({1, ActionType::media,
                   MediaParameters{static_cast<MediaOperation>(999)}})
             .valid,
        "unknown operations are rejected even when an enum value is forged");
  check(!validate({1, static_cast<ActionType>(999), KeyboardParameters{"CTRL+W"}}).valid,
        "unknown action types are rejected even when an enum value is forged");
}

void symbolic_resolution_tests() {
  ActionContext context;
  context.gesture.start_position = {-100, 20};
  context.gesture.current_position = {400, 500};
  context.current_cursor_position = {30, 40};
  context.gesture_window = 10;
  context.foreground_window = 20;
  context.window_at_gesture_start = 30;

  check(resolve_position({PositionTarget::gesture_start, std::nullopt}, context) ==
            gestures::Point{-100, 20},
        "gesture-start positions resolve from the immutable action context");
  check(resolve_position({PositionTarget::gesture_end, std::nullopt}, context) ==
            gestures::Point{400, 500},
        "gesture-end positions resolve from the action context");
  check(resolve_position({PositionTarget::current_cursor, std::nullopt}, context) ==
            gestures::Point{30, 40},
        "current cursor positions resolve from execution-time context");
  context.current_cursor_position.reset();
  check(!resolve_position({PositionTarget::current_cursor, std::nullopt}, context),
        "an unavailable current cursor does not substitute the gesture endpoint");
  check(resolve_position({PositionTarget::absolute, gestures::Point{-900, 12}}, context) ==
            gestures::Point{-900, 12},
        "absolute positions resolve without altering virtual-desktop coordinates");
  check(!resolve_position({PositionTarget::absolute, std::nullopt}, context),
        "missing absolute positions do not fall back to a contextual point");

  check(resolve_window(WindowTarget::gesture_window, context) == 10,
        "gesture window resolves exactly");
  check(resolve_window(WindowTarget::foreground_window, context) == 20,
        "foreground window resolves exactly");
  check(resolve_window(WindowTarget::window_at_gesture_start, context) == 30,
        "window-at-start resolves exactly");
  context.gesture_window.reset();
  check(!resolve_window(WindowTarget::gesture_window, context),
        "missing gesture window does not substitute the foreground window");
  check(!resolve_window(static_cast<WindowTarget>(999), context),
        "unknown window symbols fail safely");
}

void action_result_tests() {
  const auto success = ActionResult::succeeded();
  check(success.success && success.error == ActionError::none, "success results are structured");
  const auto failure = ActionResult::failed(ActionError::platform_failure, "send_failed",
                                            "Windows rejected the operation.");
  check(!failure.success && failure.error == ActionError::platform_failure &&
            failure.code == "send_failed" && !failure.message.empty(),
        "failure results retain category, code, and readable message");
}

void factory_tests() {
  RecordingServices recording;
  ActionServices services{&recording, &recording, &recording, &recording,
                          &recording, &recording, &recording, &recording};
  ActionContext context;
  context.gesture.start_position = {-10, 5};
  context.gesture_window = 123;
  const std::vector<ActionDefinition> definitions{
      ActionDefinition::keyboard("CTRL+W"),
      {1, ActionType::process,
       ProcessParameters{ProcessOperation::launch, "tool.exe", {}, {}}},
      {1, ActionType::url, UrlParameters{"https://example.com"}},
      {1, ActionType::mouse,
       MouseParameters{MouseOperation::click, MouseButton::left,
                       {PositionTarget::gesture_start, std::nullopt}}},
      {1, ActionType::window,
       WindowParameters{WindowOperation::maximize, WindowTarget::gesture_window}},
      {1, ActionType::media, MediaParameters{MediaOperation::play_pause}},
      {1, ActionType::volume, VolumeParameters{VolumeOperation::increase, 5.0}},
      {1, ActionType::virtual_desktop,
       VirtualDesktopParameters{VirtualDesktopOperation::next}}};

  for (const auto& definition : definitions) {
    auto created = ActionFactory::create(definition, services);
    check(created && created.validation.valid, "factory creates every valid action type");
    check(created.action->execute(context).success, "created action delegates to its service");
  }
  check(recording.calls == static_cast<int>(definitions.size()),
        "each executable action invokes exactly one service operation");
  check(recording.point == gestures::Point{-10, 5},
        "mouse actions resolve contextual positions before service dispatch");
  check(recording.window == 123, "window actions resolve contextual targets before dispatch");

  auto invalid_definition = ActionDefinition::keyboard("CTRL+");
  const auto invalid = ActionFactory::create(invalid_definition, services);
  check(!invalid && !invalid.validation.valid,
        "factory rejects invalid definitions before creating an action");
  const std::vector<ActionDefinition> invalid_definitions{
      {1, ActionType::process, ProcessParameters{}},
      {1, ActionType::url, UrlParameters{}},
      {1, ActionType::mouse,
       MouseParameters{MouseOperation::click, std::nullopt,
                       {PositionTarget::current_cursor, std::nullopt}}},
      {1, ActionType::window,
       WindowParameters{WindowOperation::resize, WindowTarget::gesture_window}},
      {1, ActionType::media, MediaParameters{static_cast<MediaOperation>(999)}},
      {1, static_cast<ActionType>(999), KeyboardParameters{"CTRL+W"}}};
  for (const auto& definition : invalid_definitions) {
    check(!ActionFactory::create(definition, services),
          "factory rejects missing parameters, invalid operations, and unknown action types");
  }

  auto unavailable_action = ActionFactory::create(ActionDefinition::keyboard("CTRL+W"), {});
  const auto unavailable_result = unavailable_action.action->execute(context);
  check(!unavailable_result.success &&
            unavailable_result.error == ActionError::unsupported_operation,
        "a missing platform service produces a structured failure");
  auto unavailable_audio = ActionFactory::create(
      {1, ActionType::volume, VolumeParameters{VolumeOperation::mute_toggle, std::nullopt}}, {});
  const auto unavailable_audio_result = unavailable_audio.action->execute(context);
  check(!unavailable_audio_result.success &&
            unavailable_audio_result.error == ActionError::unsupported_operation,
        "an unavailable audio service produces a structured failure");

  context.gesture_window.reset();
  auto missing_window = ActionFactory::create(definitions[4], services);
  const auto missing_window_result = missing_window.action->execute(context);
  check(!missing_window_result.success &&
            missing_window_result.error == ActionError::invalid_runtime_target &&
            recording.window == 123,
        "a missing contextual window never dispatches to a substitute target");
}

void executor_tests() {
  QueueKeyboard keyboard;
  ActionExecutor executor(ActionServices{.keyboard = &keyboard});
  ActionContext context;
  auto first = executor.submit(ActionDefinition::keyboard("CTRL+A"), context);
  auto second = executor.submit(ActionDefinition::keyboard("CTRL+B"), context);
  check(first.get().success && second.get().success,
        "queued actions return their structured results");
  check(keyboard.shortcuts == std::vector<std::string>{"CTRL+A", "CTRL+B"},
        "action executor preserves submission order");

  keyboard.throws = true;
  const auto exception = executor.submit(ActionDefinition::keyboard("CTRL+C"), context).get();
  check(!exception.success && exception.error == ActionError::execution_exception,
        "action executor contains service exceptions");
  keyboard.throws = false;
  check(executor.submit(ActionDefinition::keyboard("CTRL+D"), context).get().success,
        "action executor remains usable after a contained exception");
}

}  // namespace

void run_action_definition_tests() {
  supported_definition_tests();
  invalid_definition_tests();
  symbolic_resolution_tests();
  action_result_tests();
  factory_tests();
  executor_tests();
}

}  // namespace strokes::tests
