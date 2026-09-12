#include <span>
#include <vector>

#include "config/configuration_codec.h"
#include "config/json.h"
#include "engine/gesture_engine.h"
#include "input/mouse_input_router.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using namespace actions;
using namespace context;
using namespace engine;
using namespace gestures;
using namespace input;

class FixedContext final : public IApplicationContextProvider {
 public:
  ApplicationContext value;
  bool available{true};
  mutable std::uintptr_t requested_window{};
  std::optional<ApplicationContext> foreground_application() const override {
    return available ? std::optional<ApplicationContext>{value} : std::nullopt;
  }
  std::optional<ApplicationContext> window_application(std::uintptr_t window) const override {
    requested_window = window;
    return available ? std::optional<ApplicationContext>{value} : std::nullopt;
  }
};

class FixedModifiers final : public IModifierStateProvider {
 public:
  ModifierState value;
  ModifierState current_modifiers() const override { return value; }
};

class FakeClick final : public IMouseClick {
 public:
  int calls{};
  ActivationButton button{};
  Point position{};
  bool click(ActivationButton value, Point click_position) override {
    ++calls;
    button = value;
    position = click_position;
    return true;
  }
};

class FakeKeyboard final : public IKeyboardInput {
 public:
  std::vector<KeyEvent> events;
  int sends{};
  bool result{true};
  bool is_key_down(VirtualKey) const override { return false; }
  bool send(std::span<const KeyEvent> value) override {
    ++sends;
    events.assign(value.begin(), value.end());
    return result;
  }
};

class FakeProcess final : public IProcessService {
 public:
  int calls{};
  ProcessParameters last;
  ActionResult result{ActionResult::succeeded()};
  ActionResult launch(const ProcessParameters& value) override {
    ++calls;
    last = value;
    return result;
  }
};

class FakeShell final : public IShellService {
 public:
  int calls{};
  std::string uri;
  ActionResult open_uri(std::string_view value) override {
    ++calls;
    uri = value;
    return ActionResult::succeeded();
  }
};

class FailingServices final : public IProcessService,
                              public IShellService,
                              public IMouseService,
                              public IWindowService,
                              public IMediaService,
                              public IAudioService,
                              public IVirtualDesktopService {
 public:
  bool fail{true};
  ActionResult result() const {
    return fail ? ActionResult::failed(ActionError::platform_failure, "test_failure",
                                       "Injected failure.")
                : ActionResult::succeeded();
  }
  ActionResult launch(const ProcessParameters&) override { return result(); }
  ActionResult open_uri(std::string_view) override { return result(); }
  std::optional<Point> current_position() const override { return Point{5, 5}; }
  ActionResult perform(MouseOperation, std::optional<MouseButton>, Point) override {
    return result();
  }
  ActionResult perform(WindowOperation, std::uintptr_t, const WindowParameters&) override {
    return result();
  }
  std::optional<Bounds> bounds(std::uintptr_t) const override { return Bounds{}; }
  std::optional<MonitorInfo> monitor(std::uintptr_t) const override { return MonitorInfo{}; }
  ActionResult perform(MediaOperation) override { return result(); }
  ActionResult perform(VolumeOperation, std::optional<double>) override { return result(); }
  ActionResult perform(VirtualDesktopOperation) override { return result(); }
};

class RecordingServices final : public IProcessService,
                                public IShellService,
                                public IMouseService,
                                public IWindowService,
                                public IMediaService,
                                public IAudioService,
                                public IVirtualDesktopService {
 public:
  ProcessParameters process;
  std::string uri;
  MouseOperation mouse_operation{};
  std::optional<MouseButton> mouse_button;
  Point mouse_position;
  WindowOperation window_operation{};
  std::uintptr_t window{};
  WindowParameters window_parameters;
  MediaOperation media_operation{};
  VolumeOperation volume_operation{};
  std::optional<double> volume_amount;
  VirtualDesktopOperation desktop_operation{};

  ActionResult launch(const ProcessParameters& value) override {
    process = value;
    return ActionResult::succeeded();
  }
  ActionResult open_uri(std::string_view value) override {
    uri = value;
    return ActionResult::succeeded();
  }
  std::optional<Point> current_position() const override { return Point{-25, 50}; }
  ActionResult perform(MouseOperation operation, std::optional<MouseButton> button,
                       Point position) override {
    mouse_operation = operation;
    mouse_button = button;
    mouse_position = position;
    return ActionResult::succeeded();
  }
  ActionResult perform(WindowOperation operation, std::uintptr_t target,
                       const WindowParameters& parameters) override {
    window_operation = operation;
    window = target;
    window_parameters = parameters;
    return ActionResult::succeeded();
  }
  std::optional<Bounds> bounds(std::uintptr_t) const override { return Bounds{}; }
  std::optional<MonitorInfo> monitor(std::uintptr_t) const override { return MonitorInfo{}; }
  ActionResult perform(MediaOperation operation) override {
    media_operation = operation;
    return ActionResult::succeeded();
  }
  ActionResult perform(VolumeOperation operation, std::optional<double> amount) override {
    volume_operation = operation;
    volume_amount = amount;
    return ActionResult::succeeded();
  }
  ActionResult perform(VirtualDesktopOperation operation) override {
    desktop_operation = operation;
    return ActionResult::succeeded();
  }
};

class FakeFeedback final : public IGestureFeedback {
 public:
  int shows{}, updates{}, hides{};
  bool visible{};
  Stroke last_points;
  void show(const Stroke& points) override {
    ++shows;
    visible = true;
    last_points = points;
  }
  void update(const Stroke& points) override {
    ++updates;
    last_points = points;
  }
  void hide() noexcept override {
    ++hides;
    visible = false;
    last_points.clear();
  }
};

Stroke right_line() { return {{0, 0}, {10, 0}, {20, 0}, {30, 0}}; }

MouseInputEvent mouse(MouseEventType type, double x, double y, std::uintptr_t target = 0) {
  return {type, {x, y}, ActivationButton::right, {}, target};
}

struct Fixture {
  Recognizer recognizer{0.90};
  std::vector<ApplicationProfile> profiles;
  ActionResolver::GlobalActions globals;
  FixedContext context;
  FixedModifiers modifiers;
  FakeClick click;
  FakeKeyboard keyboard;

  Fixture() {
    (void)recognizer.add_gesture({"right", "Right", true, {{"sample", right_line()}}});
    context.value = {10, 20, "chrome.exe", "Chrome", "ChromeClass"};
  }

  GestureEngine engine(ActionServices services = {}) {
    return {recognizer, profiles, globals, context, modifiers, click, keyboard,
            GestureStateMachine{}, nullptr, services};
  }
};

void ordinary_click_pipeline() {
  Fixture fixture;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 1, 1));
  check(result.click_replayed && fixture.click.calls == 1,
        "below-threshold interaction replays an ordinary click");
  check(fixture.click.position == Point{0, 0}, "ordinary click is replayed at its press position");
  check(engine.state() == GestureState::idle, "click pipeline finishes idle");
}

void global_action_pipeline() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+LEFT"));
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 10, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 30, 0));
  check(result.recognition && result.recognition->gesture_id == "right",
        "completed stroke is recognized by the engine pipeline");
  check(result.action_source == ActionSource::global, "global action is selected");
  check(result.action_attempted && result.action_succeeded, "resolved keyboard action executes");
  check(fixture.keyboard.events.size() == 4, "keyboard action emits its complete key sequence");
  check(engine.state() == GestureState::idle, "action pipeline finishes idle");
}

void universal_minimize_pipeline() {
  Fixture fixture;
  (void)fixture.recognizer.add_gesture(
      {"minimize", "Minimize", true, {{"sample", {{100, 0}, {50, 50}, {0, 100}}}}});
  fixture.globals.emplace("minimize",
                          ActionDefinition::keyboard("ALT+SPACE,N"));
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 100, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 50, 50));
  const auto result = engine.process(mouse(MouseEventType::button_up, 0, 100));
  check(result.recognition && result.recognition->gesture_id == "minimize" &&
            result.action_succeeded && fixture.keyboard.sends == 2 &&
            fixture.keyboard.events.size() == 2 &&
            fixture.keyboard.events[0].key == static_cast<VirtualKey>('N'),
        "top-right to bottom-left gesture executes its configurable minimize shortcut");
}

void universal_maximize_pipeline() {
  Fixture fixture;
  (void)fixture.recognizer.add_gesture(
      {"maximize", "Maximize", true, {{"sample", {{0, 100}, {50, 50}, {100, 0}}}}});
  fixture.globals.emplace("maximize", ActionDefinition::keyboard("WIN+UP"));
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 100));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 50, 50));
  const auto result = engine.process(mouse(MouseEventType::button_up, 100, 0));
  check(result.recognition && result.recognition->gesture_id == "maximize" &&
            result.action_succeeded && fixture.keyboard.events.size() == 4 &&
            fixture.keyboard.events[0].key == VirtualKey::left_windows &&
            fixture.keyboard.events[1].key == VirtualKey::up,
        "bottom-left to top-right gesture executes its configurable maximize shortcut");
}

void profile_override_and_context_snapshot() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+LEFT"));
  fixture.profiles.push_back({"chrome",
                              "Chrome",
                              true,
                              {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"}},
                              {{"right", ActionDefinition::keyboard("CTRL+W")}}});
  fixture.modifiers.value.control = true;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0, 0x1234));
  check(fixture.context.requested_window == 0x1234,
        "gesture activation resolves context for the captured target window");
  fixture.context.value.executable_name = "notepad.exe";
  (void)engine.process(mouse(MouseEventType::pointer_moved, 10, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 30, 0));
  check(result.action_source == ActionSource::application_profile,
        "profile action uses application captured at gesture start");
  check(result.profile_id == "chrome", "engine reports the matched profile identity");
}

void generic_action_pipeline_and_precedence() {
  Fixture fixture;
  FakeProcess process;
  FakeShell shell;
  fixture.globals.emplace(
      "right", ActionDefinition{1, ActionType::url, UrlParameters{"https://example.com"}});
  fixture.profiles.push_back({
      "chrome", "Chrome", true,
      {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"}},
      {{"right", ActionDefinition{1, ActionType::process,
                                   ProcessParameters{ProcessOperation::launch, "tool.exe",
                                                     "--flag", "C:\\Tools"}}}}});
  auto engine = fixture.engine(ActionServices{.process = &process, .shell = &shell});
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.action_succeeded && result.action_result && result.action_result->success,
        "recognized generic action returns a structured successful result");
  check(process.calls == 1 && process.last.path == "tool.exe" &&
            process.last.arguments == "--flag" && process.last.working_directory == "C:\\Tools",
        "recognized process action reaches the process service with all parameters");
  check(shell.calls == 0 && result.action_source == ActionSource::application_profile,
        "application process action overrides a global URL action");
}

void generic_action_failure_recovers() {
  Fixture fixture;
  FakeProcess process;
  process.result = ActionResult::failed(ActionError::invalid_runtime_target,
                                        "executable_not_found", "Missing executable.");
  fixture.globals.emplace(
      "right", ActionDefinition{1, ActionType::process,
                                 ProcessParameters{ProcessOperation::launch, "missing.exe", {}, {}}});
  auto engine = fixture.engine(ActionServices{.process = &process});
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto failed = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(failed.action_result && !failed.action_result->success &&
            failed.action_result->code == "executable_not_found" &&
            failed.action_result->error == ActionError::invalid_runtime_target,
        "generic service failures retain their structured diagnostic");
  check(engine.state() == GestureState::idle,
        "generic service failure always returns the gesture engine to Idle");
  process.result = ActionResult::succeeded();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  check(engine.process(mouse(MouseEventType::button_up, 40, 0)).action_succeeded,
        "a subsequent generic action executes after a service failure");
}

void every_action_category_failure_recovers() {
  const std::vector<ActionDefinition> definitions{
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
    Fixture fixture;
    FailingServices services;
    fixture.globals.emplace("right", definition);
    auto engine = fixture.engine(ActionServices{.process = &services,
                                                .shell = &services,
                                                .mouse = &services,
                                                .window = &services,
                                                .media = &services,
                                                .audio = &services,
                                                .virtual_desktop = &services});
    (void)engine.process(mouse(MouseEventType::button_down, 0, 0, 0x1234));
    (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
    const auto failed = engine.process(mouse(MouseEventType::button_up, 40, 0));
    check(failed.action_result && !failed.action_result->success &&
              engine.state() == GestureState::idle,
          "each failed action category returns the engine to Idle");
    services.fail = false;
    (void)engine.process(mouse(MouseEventType::button_down, 0, 0, 0x1234));
    (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
    check(engine.process(mouse(MouseEventType::button_up, 40, 0)).action_succeeded,
          "each action category succeeds on a gesture following failure");
  }
}

void recognized_gesture_dispatches_every_generic_action() {
  RecordingServices services;
  const ActionServices action_services{.process = &services,
                                       .shell = &services,
                                       .mouse = &services,
                                       .window = &services,
                                       .media = &services,
                                       .audio = &services,
                                       .virtual_desktop = &services};
  const auto execute = [&](ActionDefinition definition) {
    Fixture fixture;
    fixture.context.value.window_handle = 0x1234;
    fixture.globals.emplace("right", std::move(definition));
    auto engine = fixture.engine(action_services);
    (void)engine.process(mouse(MouseEventType::button_down, 0, 0, 0x1234));
    (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0, 0x1234));
    return engine.process(mouse(MouseEventType::button_up, 40, 0, 0x1234));
  };

  check(execute({1, ActionType::process,
                 ProcessParameters{ProcessOperation::launch, "tool.exe", "--flag", "C:\\Work"}})
            .action_succeeded &&
            services.process.path == "tool.exe" && services.process.arguments == "--flag" &&
            services.process.working_directory == "C:\\Work",
        "recognized gesture dispatches a complete process launch");
  check(execute({1, ActionType::url, UrlParameters{"https://example.com/path"}}).action_succeeded &&
            services.uri == "https://example.com/path",
        "recognized gesture dispatches a URI through its registered-handler service");
  check(execute({1, ActionType::mouse,
                 MouseParameters{MouseOperation::click, MouseButton::left,
                                 {PositionTarget::gesture_start, std::nullopt}}})
            .action_succeeded &&
            services.mouse_operation == MouseOperation::click &&
            services.mouse_position == Point{0, 0},
        "recognized gesture clicks at its recorded start position");
  check(execute({1, ActionType::window,
                 WindowParameters{WindowOperation::maximize, WindowTarget::gesture_window}})
            .action_succeeded &&
            services.window == 0x1234 && services.window_operation == WindowOperation::maximize,
        "recognized gesture maximizes its original target window");
  check(execute({1, ActionType::window,
                 WindowParameters{WindowOperation::minimize, WindowTarget::gesture_window}})
            .action_succeeded &&
            services.window_operation == WindowOperation::minimize,
        "recognized gesture minimizes its original target window");
  check(execute({1, ActionType::window,
                 WindowParameters{WindowOperation::move_resize, WindowTarget::gesture_window,
                                  -100, 75, 900, 650}})
            .action_succeeded &&
            services.window_parameters.x == -100 && services.window_parameters.y == 75 &&
            services.window_parameters.width == 900 && services.window_parameters.height == 650,
        "recognized gesture dispatches configured window position and dimensions");
  check(execute({1, ActionType::media, MediaParameters{MediaOperation::play_pause}})
            .action_succeeded &&
            services.media_operation == MediaOperation::play_pause,
        "recognized gesture dispatches Play/Pause");
  check(execute({1, ActionType::volume, VolumeParameters{VolumeOperation::increase, 7.5}})
            .action_succeeded &&
            services.volume_operation == VolumeOperation::increase &&
            services.volume_amount == 7.5,
        "recognized gesture dispatches a configured volume adjustment");
  check(execute({1, ActionType::volume,
                 VolumeParameters{VolumeOperation::decrease, std::nullopt}})
            .action_succeeded &&
            services.volume_operation == VolumeOperation::decrease &&
            !services.volume_amount.has_value(),
        "recognized gesture dispatches a default-amount volume decrease");
  check(execute({1, ActionType::volume,
                 VolumeParameters{VolumeOperation::mute_toggle, std::nullopt}})
            .action_succeeded &&
            services.volume_operation == VolumeOperation::mute_toggle,
        "recognized gesture dispatches mute toggle");
  check(execute({1, ActionType::virtual_desktop,
                 VirtualDesktopParameters{VirtualDesktopOperation::next}})
            .action_succeeded &&
            services.desktop_operation == VirtualDesktopOperation::next,
        "recognized gesture dispatches next virtual desktop");
}

void legacy_keyboard_configuration_executes() {
  const auto parsed = config::json::parse(
      R"({"version":1,"profiles":[],"global_actions":{"right":{"type":"keyboard","shortcut":"CTRL+W"}}})");
  const auto decoded = parsed ? config::decode_profiles(*parsed.value)
                              : config::DecodeResult<config::ProfileFile>{{}, "invalid JSON"};
  check(static_cast<bool>(decoded), "Phase 1 keyboard configuration decodes for execution");
  if (!decoded) return;
  Fixture fixture;
  fixture.globals = decoded.value->global_actions;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.action_succeeded && fixture.keyboard.events.size() == 4,
        "Phase 1 keyboard configuration executes after compatibility decoding");
}

void no_match_executes_nothing() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+LEFT"));
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 0, 10));
  const auto result = engine.process(mouse(MouseEventType::button_up, 0, 30));
  check(!result.recognition && !result.action_attempted && fixture.keyboard.events.empty(),
        "unknown stroke executes no action");
  check(engine.state() == GestureState::idle, "no-match pipeline returns idle");
  check(feedback.hides == 1 && !feedback.visible, "recognition rejection hides gesture feedback");
}

void disabled_feedback_does_not_disable_recognition() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+RIGHT"));
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, nullptr};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.recognition && result.action_succeeded,
        "recognition and actions work when gesture feedback is disabled at the engine boundary");
}

void action_failure_does_not_stop_engine() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+RIGHT"));
  fixture.keyboard.result = false;
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto failed = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(failed.action_attempted && !failed.action_succeeded,
        "action injection failure is reported");
  check(engine.state() == GestureState::idle, "action failure leaves engine operational");
  fixture.keyboard.result = true;
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  check(engine.process(mouse(MouseEventType::button_up, 40, 0)).action_succeeded,
        "gesture after an action failure still executes");
}

void cancellation_executes_nothing() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+RIGHT"));
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto cancelled = engine.process(mouse(MouseEventType::cancel, 20, 0));
  check(cancelled.gesture.capture_cancelled && !cancelled.action_attempted &&
            fixture.keyboard.events.empty(),
        "cancellation executes no action");
  check(feedback.hides == 1 && !feedback.visible && feedback.last_points.empty(),
        "cancellation hides and clears gesture feedback");
  check(engine.state() == GestureState::idle, "cancelled engine pipeline returns idle");
}

void shutdown_cleans_active_capture() {
  Fixture fixture;
  FakeFeedback feedback;
  {
    GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                         fixture.context,    fixture.modifiers,     fixture.click,
                         fixture.keyboard,   GestureStateMachine{}, &feedback};
    (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
    (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
    check(feedback.visible && engine.session().has_value(),
          "shutdown fixture begins with active capture state");
  }
  check(!feedback.visible && feedback.last_points.empty() && feedback.hides == 1,
        "engine shutdown hides and clears active gesture feedback");
}

void closed_target_uses_safe_snapshot() {
  Fixture fixture;
  fixture.profiles.push_back({"chrome",
                              "Chrome",
                              true,
                              {{ApplicationProperty::process_name, MatchMode::exact, "chrome.exe"}},
                              {{"right", ActionDefinition::keyboard("CTRL+W")}}});
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, 0, 0));
  fixture.context.available = false;
  (void)engine.process(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto result = engine.process(mouse(MouseEventType::button_up, 40, 0));
  check(result.action_succeeded && result.profile_id == "chrome",
        "closing the original target does not invalidate captured context or action data");
}

void cross_monitor_coordinates_pipeline() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+RIGHT"));
  auto engine = fixture.engine();
  (void)engine.process(mouse(MouseEventType::button_down, -300, 40));
  (void)engine.process(mouse(MouseEventType::pointer_moved, -100, 40));
  (void)engine.process(mouse(MouseEventType::pointer_moved, 100, 40));
  const auto result = engine.process(mouse(MouseEventType::button_up, 300, 40));
  check(result.recognition && result.action_succeeded,
        "stroke crossing negative and positive screen coordinates is recognized and executed");
}

void router_engine_feedback_integration() {
  Fixture fixture;
  fixture.globals.emplace("right", ActionDefinition::keyboard("ALT+RIGHT"));
  FakeFeedback feedback;
  GestureEngine engine{fixture.recognizer, fixture.profiles,      fixture.globals,
                       fixture.context,    fixture.modifiers,     fixture.click,
                       fixture.keyboard,   GestureStateMachine{}, &feedback};
  MouseInputRouter router([&](const MouseInputEvent& value) {
    (void)engine.process(value);
    return true;
  });
  check(router.route(mouse(MouseEventType::button_down, -20, 0)).suppress_input,
        "abstract hook router suppresses configured activation input");
  (void)router.route(mouse(MouseEventType::pointer_moved, 0, 0));
  (void)router.route(mouse(MouseEventType::pointer_moved, 20, 0));
  (void)router.route(mouse(MouseEventType::button_up, 40, 0));
  check(feedback.shows == 1 && feedback.updates >= 1 && feedback.hides == 1,
        "router-to-engine pipeline owns the complete overlay lifecycle");
  check(fixture.keyboard.events.size() == 4 && engine.state() == GestureState::idle,
        "router-to-engine pipeline captures context, recognizes, and executes its action");
  for (int attempt = 0; attempt < 50; ++attempt) {
    (void)router.route(mouse(MouseEventType::button_down, -20, 0));
    (void)router.route(mouse(MouseEventType::pointer_moved, 0, 0));
    (void)router.route(mouse(MouseEventType::button_up, 40, 0));
  }
  check(!feedback.visible && feedback.last_points.empty() && feedback.shows == feedback.hides,
        "rapid gestures leave no visible or retained overlay state");

  router.set_enabled(false);
  check(!router.route(mouse(MouseEventType::button_down, 0, 0)).suppress_input &&
            !router.route(mouse(MouseEventType::pointer_moved, 20, 0)).suppress_input &&
            !router.route(mouse(MouseEventType::button_up, 40, 0)).suppress_input,
        "disabled router passes the complete mouse interaction through");
  check(!feedback.visible, "disabled end-to-end path produces no overlay");
  router.set_enabled(true);
  (void)router.route(mouse(MouseEventType::button_down, 0, 0));
  (void)router.route(mouse(MouseEventType::pointer_moved, 20, 0));
  const auto resumed = router.route(mouse(MouseEventType::button_up, 40, 0));
  check(resumed.suppress_input && engine.state() == GestureState::idle,
        "re-enabled router resumes complete gesture processing");
}

}  // namespace

void run_gesture_engine_tests() {
  ordinary_click_pipeline();
  global_action_pipeline();
  universal_minimize_pipeline();
  universal_maximize_pipeline();
  profile_override_and_context_snapshot();
  generic_action_pipeline_and_precedence();
  generic_action_failure_recovers();
  every_action_category_failure_recovers();
  recognized_gesture_dispatches_every_generic_action();
  legacy_keyboard_configuration_executes();
  no_match_executes_nothing();
  disabled_feedback_does_not_disable_recognition();
  action_failure_does_not_stop_engine();
  cancellation_executes_nothing();
  shutdown_cleans_active_capture();
  closed_target_uses_safe_snapshot();
  cross_monitor_coordinates_pipeline();
  router_engine_feedback_integration();
}

}  // namespace strokes::tests
