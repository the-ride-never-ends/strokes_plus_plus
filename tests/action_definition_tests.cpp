#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "actions/action_definition.h"
#include "actions/action_executor.h"
#include "actions/action_factory.h"
#include "actions/action_result.h"
#include "actions/lua_runtime.h"
#include "actions/keyboard_shortcut.h"
#include "actions/symbolic_resolver.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using namespace actions;

struct LuaModuleDirectory {
  std::filesystem::path path =
      std::filesystem::temp_directory_path() /
      ("strokes-plus-plus-lua-tests-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

  LuaModuleDirectory() { std::filesystem::create_directories(path); }
  ~LuaModuleDirectory() {
    std::error_code error;
    std::filesystem::remove_all(path, error);
  }
};

class RecordingServices final : public IKeyboardService,
                                public IProcessService,
                                public IShellService,
                                public IMouseService,
                                public IWindowService,
                                public IMediaService,
                                public IAudioService,
                                public IVirtualDesktopService,
                                public ILuaService {
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
  ActionResult execute(std::string_view script, const ActionContext& context) override {
    lua_script = script;
    lua_context = context;
    return called();
  }

  std::string lua_script;
  ActionContext lua_context;

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

class LuaKeyboard final : public IKeyboardService {
 public:
  std::vector<std::string> shortcuts;
  std::vector<std::pair<std::string, bool>> events;
  bool shift_down{true};

  ActionResult send_shortcut(std::string_view shortcut) override {
    if (!parse_shortcut_sequence(shortcut))
      return ActionResult::failed(ActionError::invalid_definition, "invalid_shortcut",
                                  "Invalid shortcut.");
    shortcuts.emplace_back(shortcut);
    return ActionResult::succeeded();
  }
  ActionResult send_key(std::string_view key, bool key_down) override {
    if (!parse_key(key))
      return ActionResult::failed(ActionError::invalid_definition, "invalid_key", "Invalid key.");
    events.emplace_back(key, key_down);
    return ActionResult::succeeded();
  }
  std::optional<bool> is_key_down(std::string_view key) const override {
    return key == "SHIFT" ? std::optional<bool>{shift_down} : std::nullopt;
  }
};

class LuaAutomationServices final : public IProcessService,
                                    public IShellService,
                                    public IMouseService,
                                    public IWindowService,
                                    public IMediaService,
                                    public IAudioService,
                                    public IVirtualDesktopService,
                                    public IDiagnosticService,
                                    public IUserFeedbackService {
 public:
  std::vector<ProcessParameters> launches;
  std::vector<std::string> uris;
  std::vector<MediaOperation> media;
  std::vector<VirtualDesktopOperation> desktops;
  std::vector<MouseOperation> mouse_operations;
  std::vector<std::optional<MouseButton>> mouse_buttons;
  std::vector<gestures::Point> mouse_positions;
  std::optional<gestures::Point> cursor{gestures::Point{-50, 75}};
  std::vector<WindowOperation> window_operations;
  std::vector<std::uintptr_t> windows;
  std::vector<WindowParameters> window_parameters;
  bool fail_process{};
  bool fail_shell{};
  bool fail_desktop{};
  bool fail_activation{};
  double current_volume{35.0};
  bool muted{true};
  std::vector<VolumeOperation> volume_operations;
  std::vector<std::optional<double>> volume_amounts;
  std::vector<std::pair<std::string, std::string>> diagnostics;
  bool fail_diagnostics{};
  std::vector<std::string> messages;
  std::vector<std::string> on_screen_messages;

  ActionResult launch(const ProcessParameters& parameters) override {
    launches.push_back(parameters);
    return fail_process
               ? ActionResult::failed(ActionError::platform_failure, "launch_failed",
                                      "Injected process failure.")
               : ActionResult::succeeded();
  }
  ActionResult open_uri(std::string_view uri) override {
    uris.emplace_back(uri);
    return fail_shell
               ? ActionResult::failed(ActionError::platform_failure, "shell_failed",
                                      "Injected shell failure.")
               : ActionResult::succeeded();
  }
  std::optional<gestures::Point> current_position() const override { return cursor; }
  ActionResult perform(MouseOperation operation, std::optional<MouseButton> button,
                       gestures::Point position) override {
    mouse_operations.push_back(operation);
    mouse_buttons.push_back(button);
    mouse_positions.push_back(position);
    return ActionResult::succeeded();
  }
  ActionResult perform(WindowOperation operation, std::uintptr_t window,
                       const WindowParameters& parameters) override {
    window_operations.push_back(operation);
    windows.push_back(window);
    window_parameters.push_back(parameters);
    if (fail_activation && operation == WindowOperation::activate)
      return ActionResult::failed(ActionError::platform_failure, "activation_denied",
                                  "Injected activation failure.");
    return ActionResult::succeeded();
  }
  std::optional<Bounds> bounds(std::uintptr_t window) const override {
    return exists(window) ? std::optional<Bounds>{{-100, 20, 1100, 820, 1200, 800}}
                          : std::nullopt;
  }
  std::optional<MonitorInfo> monitor(std::uintptr_t) const override { return std::nullopt; }
  bool exists(std::uintptr_t window) const override { return window == 100 || window == 200; }
  std::optional<std::string> title(std::uintptr_t window) const override {
    return exists(window) ? std::optional<std::string>{window == 100 ? "Gesture Window"
                                                                     : "Foreground Window"}
                          : std::nullopt;
  }
  std::optional<std::string> class_name(std::uintptr_t window) const override {
    return exists(window) ? std::optional<std::string>{"TestWindowClass"} : std::nullopt;
  }
  std::optional<std::string> process_name(std::uintptr_t window) const override {
    return exists(window) ? std::optional<std::string>{"test.exe"} : std::nullopt;
  }
  ActionResult perform(MediaOperation operation) override {
    media.push_back(operation);
    return ActionResult::succeeded();
  }
  ActionResult perform(VolumeOperation operation, std::optional<double> amount) override {
    volume_operations.push_back(operation);
    volume_amounts.push_back(amount);
    if (operation == VolumeOperation::mute_toggle) muted = !muted;
    return ActionResult::succeeded();
  }
  std::optional<double> volume() const override { return current_volume; }
  ActionResult set_volume(double value) override {
    current_volume = value;
    return ActionResult::succeeded();
  }
  std::optional<bool> is_muted() const override { return muted; }
  ActionResult perform(VirtualDesktopOperation operation) override {
    desktops.push_back(operation);
    return fail_desktop
               ? ActionResult::failed(ActionError::unsupported_operation, "desktop_unsupported",
                                      "Injected desktop failure.")
               : ActionResult::succeeded();
  }
  ActionResult write(std::string_view level, std::string_view message) override {
    if (fail_diagnostics)
      return ActionResult::failed(ActionError::platform_failure, "diagnostic_write_failed",
                                  "Injected diagnostic failure.");
    diagnostics.emplace_back(level, message);
    return ActionResult::succeeded();
  }
  ActionResult message(std::string_view text) override {
    messages.emplace_back(text);
    return ActionResult::succeeded();
  }
  ActionResult osd(std::string_view text) override {
    on_screen_messages.emplace_back(text);
    return ActionResult::succeeded();
  }
};

class BlockingLua final : public ILuaService {
 public:
  ActionResult execute(std::string_view script, const ActionContext&) override {
    std::unique_lock lock(mutex_);
    scripts_.emplace_back(script);
    if (scripts_.size() == 1) {
      first_started_ = true;
      changed_.notify_all();
      changed_.wait(lock, [this] { return release_first_; });
    }
    return ActionResult::succeeded();
  }

  void wait_until_started() {
    std::unique_lock lock(mutex_);
    changed_.wait(lock, [this] { return first_started_; });
  }

  void release_first() {
    std::lock_guard lock(mutex_);
    release_first_ = true;
    changed_.notify_all();
  }

  std::vector<std::string> scripts() const {
    std::lock_guard lock(mutex_);
    return scripts_;
  }

 private:
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  bool first_started_{};
  bool release_first_{};
  std::vector<std::string> scripts_;
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
  check(validate(ActionDefinition::lua("return true")).valid,
        "non-empty Lua definitions validate");
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
  check(!validate(ActionDefinition::lua({})).valid, "empty Lua scripts are rejected");
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
  ActionServices services{&recording, &recording, &recording, &recording, &recording,
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
       VirtualDesktopParameters{VirtualDesktopOperation::next}},
      ActionDefinition::lua("return true")};

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
  check(recording.lua_script == "return true" && recording.lua_context.gesture_window == 123,
        "Lua actions delegate the script and immutable action context to the Lua service");

  context.current_cursor_position = gestures::Point{999, 999};
  auto current_cursor = ActionFactory::create(
      {1, ActionType::mouse,
       MouseParameters{MouseOperation::click, MouseButton::left,
                       {PositionTarget::current_cursor, std::nullopt}}},
      services);
  check(current_cursor.action->execute(context).success &&
            recording.point == gestures::Point{70, 80},
        "current_cursor is sampled from the mouse service when the action executes");

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
  const auto unavailable_lua = ActionFactory::create(ActionDefinition::lua("return true"), {});
  check(!unavailable_lua.action->execute(context).success,
        "an unavailable Lua runtime produces a structured failure");

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

  BlockingLua lua;
  ActionExecutor bounded(ActionServices{.lua = &lua}, 1);
  auto running = bounded.submit(ActionDefinition::lua("first()"), context);
  lua.wait_until_started();
  auto queued = bounded.submit(ActionDefinition::lua("second()"), context);
  check(queued.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout,
        "a second Lua action waits while the first action is executing");
  const auto overflow = bounded.submit(ActionDefinition::lua("third()"), context).get();
  check(!overflow.success && overflow.code == "action_queue_full",
        "the bounded action queue rejects work beyond its supported limit");
  lua.release_first();
  check(running.get().success && queued.get().success &&
            lua.scripts() == std::vector<std::string>{"first()", "second()"},
        "queued Lua actions execute sequentially in submission order");
}

void lua_runtime_tests() {
  LuaRuntime runtime;
  LuaModuleDirectory modules;
  {
    std::ofstream module(modules.path / "shared.lua", std::ios::binary);
    module << "module_loads = (module_loads or 0) + 1; return { value = 42 }";
  }
  runtime.set_module_directory(modules.path);
  LuaKeyboard keyboard;
  LuaAutomationServices automation;
  runtime.set_services(ActionServices{.keyboard = &keyboard,
                                      .process = &automation,
                                      .shell = &automation,
                                      .mouse = &automation,
                                      .window = &automation,
                                      .media = &automation,
                                      .audio = &automation,
                                      .virtual_desktop = &automation,
                                      .diagnostics = &automation,
                                      .user_feedback = &automation});
  ActionContext context;
  context.recognition = gestures::RecognitionResult{"right", "Right", 0.97};
  context.gesture.start_position = {-20, 10};
  context.gesture.current_position = {30, 50};
  context.gesture.captured_points = {{-20, 10}, {10, 10}, {10, 50}};
  context.gesture.start_timestamp = input::GestureClock::time_point{};
  context.gesture.last_movement_timestamp =
      context.gesture.start_timestamp + std::chrono::milliseconds(125);
  context.application.process_id = 42;
  context.application.executable_name = "chrome.exe";
  context.application.executable_path = "C:\\Program Files\\Chrome\\chrome.exe";
  context.application.window_title = "Lua context test";
  context.application.window_class = "Chrome_WidgetWin_1";
  context.gesture_window = 100;
  context.foreground_window = 200;

  check(runtime.validate_script("return true").success,
        "valid Lua syntax passes validation");
  const auto syntax_error = runtime.validate_script("if true return false end");
  check(!syntax_error.success && syntax_error.code == "lua_syntax_error" &&
            syntax_error.message.find("gesture-action") != std::string::npos,
        "invalid Lua syntax reports its source location");

  check(runtime.execute("return true", context).success,
        "an explicit true Lua result succeeds");
  check(runtime.execute("local value = 1 + 1", context).success,
        "a Lua script with no return value succeeds");
  const auto explicit_failure = runtime.execute("return false", context);
  check(!explicit_failure.success && explicit_failure.code == "lua_returned_false",
        "an explicit false Lua result fails");

  const auto runtime_error = runtime.execute("error('expected failure')", context);
  check(!runtime_error.success && runtime_error.code == "lua_runtime_error" &&
            runtime_error.message.find("expected failure") != std::string::npos,
        "Lua runtime errors are contained and reported");
  check(runtime.execute("return true", context).success,
        "the persistent Lua runtime remains usable after a script error");

  check(runtime.execute("phase_three_counter = 41", context).success &&
            runtime.execute("return phase_three_counter == 41", context).success,
        "Lua globals persist between actions in the current runtime");
  check(runtime.initialize({}).success,
        "the Lua runtime starts normally without initialization code");
  check(runtime
            .initialize("shared_calls = 0; function close_tab() "
                        "shared_calls = shared_calls + 1 end")
            .success &&
            runtime.execute("close_tab(); return shared_calls == 1", context).success &&
            runtime.execute("close_tab(); return shared_calls == 2", context).success,
        "initialization code defines reusable functions shared by multiple Lua actions");
  const auto initialization_syntax_error = runtime.initialize("function broken(");
  check(!initialization_syntax_error.success &&
            initialization_syntax_error.code == "lua_syntax_error" &&
            runtime.execute("return close_tab ~= nil", context).success,
        "initialization syntax errors are reported without disabling the runtime");
  const auto initialization_runtime_error =
      runtime.initialize("error('initialization failed')");
  check(!initialization_runtime_error.success &&
            initialization_runtime_error.code == "lua_runtime_error" &&
            runtime.execute("return close_tab ~= nil", context).success,
        "initialization runtime errors are reported without disabling the runtime");
  check(runtime
            .execute("local first = require('shared'); local second = require('shared'); "
                     "return first == second and first.value == 42 and module_loads == 1",
                     context)
            .success,
        "approved user modules load once and are reused by later require calls");
  check(!runtime.execute("require('missing')", context).success,
        "requiring a missing user module reports a Lua error");
  check(!runtime.execute("require('../outside')", context).success &&
            !runtime.execute("require([[folder\\outside]])", context).success &&
            !runtime.execute("require('folder/outside')", context).success &&
            !runtime.execute("require([[C:\\modules\\outside]])", context).success &&
            !runtime.execute("require('.shared')", context).success &&
            !runtime.execute("require('shared.')", context).success &&
            !runtime.execute("require('shared..extra')", context).success,
        "the supported module loader rejects every name outside the approved module directory");
  check(runtime.execute("runtime_only_value = 99", context).success,
        "runtime-only state can be created before reload");
  {
    std::ofstream module(modules.path / "shared.lua", std::ios::binary | std::ios::trunc);
    module << "module_loads = (module_loads or 0) + 1; return { value = 84 }";
  }
  check(runtime.reload("function reloaded_value() return 7 end").success &&
            runtime
                .execute("local module = require('shared'); "
                         "return runtime_only_value == nil and reloaded_value() == 7 and "
                         "module.value == 84 and module_loads == 1",
                         context)
                .success,
        "reload resets runtime state, reruns initialization, and refreshes cached modules");
  const auto failed_reload = runtime.reload("error('reload failed')");
  check(!failed_reload.success && failed_reload.code == "lua_runtime_error" &&
            runtime.execute("return reloaded_value == nil", context).success,
        "a failed reload reports initialization failure and leaves a usable clean runtime");
  LuaRuntime restarted;
  check(restarted.execute("return runtime_only_value == nil", context).success,
        "runtime-only globals do not persist across application runtime instances");
  check(runtime.execute("return keyboard.hotkey('CTRL', 'W')", context).success &&
            runtime.execute("return keyboard.hotkey('CTRL', 'SHIFT', 'T')", context).success &&
            keyboard.shortcuts == std::vector<std::string>{"CTRL+W", "CTRL+SHIFT+T"},
        "Lua hotkey calls reuse the keyboard shortcut service");
  check(runtime.execute("return keyboard.press('F5')", context).success &&
            keyboard.events ==
                std::vector<std::pair<std::string, bool>>{{"F5", true}, {"F5", false}} &&
            keyboard.shortcuts.size() == 2,
        "Lua press sends one key down and up instead of a shortcut sequence");
  check(!runtime.execute("keyboard.press('ALT+SPACE,N')", context).success &&
            !runtime.execute("keyboard.hotkey('CTRL', 'W,X')", context).success &&
            keyboard.shortcuts.size() == 2,
        "Lua keyboard bindings reject chords and sequences where one key is required");
  keyboard.events.clear();
  check(runtime.execute("keyboard.down('CTRL'); keyboard.up('CTRL')", context).success &&
            keyboard.events ==
                std::vector<std::pair<std::string, bool>>{{"CTRL", true}, {"CTRL", false}},
        "Lua key down and up calls reuse individual keyboard injection");
  check(runtime.execute("return keyboard.is_down('SHIFT')", context).success,
        "Lua can observe a physically pressed modifier");
  keyboard.shift_down = false;
  check(runtime.execute("return keyboard.is_down('SHIFT') == false", context).success,
        "Lua observes a released modifier as false");
  check(!runtime.execute("keyboard.press('NOT_A_KEY')", context).success &&
            !runtime.execute("keyboard.hotkey('CTRL', 7)", context).success,
        "Lua keyboard bindings reject invalid key names and argument types");
  check(runtime.execute("process.launch('wt.exe')", context).success &&
            runtime.execute("process.launch('tool.exe', '--flag')", context).success &&
            runtime.execute("process.launch('tool.exe', '--flag', 'C:/Tools')", context).success &&
            automation.launches.size() == 3 && automation.launches[0].path == "wt.exe" &&
            automation.launches[1].arguments == "--flag" &&
            automation.launches[2].working_directory == "C:/Tools",
        "Lua process launching retains optional arguments and working directories");
  check(runtime.execute("shell.open('https://example.com')", context).success &&
            runtime.execute("shell.open('ms-settings:display')", context).success &&
            runtime.execute("shell.open('mailto:test@example.com')", context).success &&
            automation.uris == std::vector<std::string>{"https://example.com",
                                                        "ms-settings:display",
                                                        "mailto:test@example.com"},
        "Lua shell opening delegates HTTPS, settings, and mail URIs");
  check(runtime
            .execute("media.play_pause(); media.next(); media.previous(); media.stop()", context)
            .success &&
            automation.media ==
                std::vector<MediaOperation>{MediaOperation::play_pause, MediaOperation::next_track,
                                            MediaOperation::previous_track, MediaOperation::stop},
        "Lua exposes every Phase 2 media operation");
  check(runtime
            .execute("desktop.next(); desktop.previous(); desktop.create(); desktop.close()",
                     context)
            .success &&
            automation.desktops ==
                std::vector<VirtualDesktopOperation>{VirtualDesktopOperation::next,
                                                     VirtualDesktopOperation::previous,
                                                     VirtualDesktopOperation::create,
                                                     VirtualDesktopOperation::close},
        "Lua exposes every Phase 2 virtual desktop operation");
  automation.fail_process = true;
  const auto process_failure = runtime.execute("process.launch('missing.exe')", context);
  check(!process_failure.success &&
            process_failure.message.find("launch_failed") != std::string::npos,
        "Lua converts process service failures into catchable runtime errors");
  automation.fail_shell = true;
  automation.fail_desktop = true;
  check(!runtime.execute("shell.open('unknown:test')", context).success &&
            !runtime.execute("desktop.create()", context).success,
        "Lua reports shell failures and unsupported desktop operations");
  check(!runtime.execute("process.launch()", context).success &&
            !runtime.execute("process.launch(7)", context).success &&
            !runtime.execute("shell.open('')", context).success,
        "Lua process and shell bindings validate required arguments");
  check(runtime.execute("local p = mouse.position(); return p.x == -50 and p.y == 75", context)
            .success &&
            runtime.execute("mouse.move(800, 500); mouse.move(-500, 300)", context).success &&
            automation.mouse_positions[0] == gestures::Point{800, 500} &&
            automation.mouse_positions[1] == gestures::Point{-500, 300},
        "Lua reads and moves the pointer in virtual-desktop coordinates");
  const auto all_mouse_buttons = runtime.execute(
      "for _, button in ipairs({'left', 'right', 'middle', 'x1', 'x2'}) do "
      "mouse.click(button); mouse.double_click(button); mouse.down(button); mouse.up(button) end",
      context);
  check(all_mouse_buttons.success && automation.mouse_operations.size() == 22 &&
            automation.mouse_buttons.back() == MouseButton::x_button_2,
        "Lua supports click, double-click, down, and up for every documented mouse button");
  check(!runtime.execute("mouse.click('banana')", context).success &&
            !runtime.execute("mouse.move(0 / 0, 5)", context).success &&
            !runtime.execute("mouse.move('x', 5)", context).success,
        "Lua mouse bindings reject invalid buttons and coordinates");
  automation.cursor.reset();
  check(!runtime.execute("mouse.position()", context).success &&
            !runtime.execute("mouse.click('left')", context).success,
        "Lua mouse operations report unavailable cursor state");
  check(runtime
            .execute("window.close(); window.minimize(); window.maximize('foreground'); "
                     "window.restore(); window.activate(); window.move(100, 200); "
                     "window.resize(1200, 800); window.move_resize(-100, 20, 1200, 800)",
                     context)
            .success &&
            automation.window_operations ==
                std::vector<WindowOperation>{WindowOperation::close, WindowOperation::minimize,
                                             WindowOperation::maximize, WindowOperation::restore,
                                             WindowOperation::activate, WindowOperation::move,
                                             WindowOperation::resize,
                                             WindowOperation::move_resize} &&
            automation.windows[0] == 100 && automation.windows[2] == 200 &&
            automation.window_parameters.back().x == -100 &&
            automation.window_parameters.back().height == 800,
        "Lua exposes window actions with default and explicit contextual targets");
  check(runtime
            .execute("local b = window.bounds(); return b.x == -100 and b.y == 20 and "
                     "b.width == 1200 and b.height == 800 and window.exists() and "
                     "window.exists('foreground')",
                     context)
            .success,
        "Lua reads window bounds and existence through the window service");
  check(runtime
            .execute("return window.title() == 'Gesture Window' and "
                     "window.title('foreground') == 'Foreground Window' and "
                     "window.class() == 'TestWindowClass' and window.process() == 'test.exe'",
                     context)
            .success,
        "Lua reads target-window title, class, and process metadata");
  check(!runtime.execute("window.resize(-5, 800)", context).success &&
            !runtime.execute("window.maximize('unknown')", context).success,
        "Lua rejects invalid window dimensions and target identifiers");
  automation.fail_activation = true;
  check(!runtime.execute("window.activate()", context).success,
        "Lua exposes window activation failures");
  check(runtime
            .execute("volume.increase(5); volume.decrease(2.5); volume.toggle_mute(); "
                     "volume.set(50); return volume.get() == 50 and not volume.is_muted()",
                     context)
            .success &&
            automation.volume_operations ==
                std::vector<VolumeOperation>{VolumeOperation::increase,
                                             VolumeOperation::decrease,
                                             VolumeOperation::mute_toggle} &&
            automation.volume_amounts[0] == 5.0 && automation.volume_amounts[1] == 2.5,
        "Lua exposes volume changes, absolute levels, and mute state");
  check(!runtime.execute("volume.set(101)", context).success &&
            !runtime.execute("volume.increase(0)", context).success,
        "Lua validates absolute and relative volume ranges");
  check(runtime
            .execute("log.debug('one'); log.info('two'); log.warn('three'); "
                     "log.error('four')",
                     context)
            .success &&
            automation.diagnostics ==
                std::vector<std::pair<std::string, std::string>>{
                    {"debug", "one"}, {"info", "two"}, {"warn", "three"}, {"error", "four"}},
        "Lua writes debug, info, warning, and error diagnostics through the application logger");
  check(!runtime.execute("log.info()", context).success &&
            !runtime.execute("log.warn(42)", context).success,
        "Lua logging validates its message argument");
  automation.fail_diagnostics = true;
  check(!runtime.execute("log.error('unwritable')", context).success,
        "Lua logging reports application diagnostic failures");
  check(runtime.execute("ui.message('Gesture executed'); ui.osd('Volume: 50%')", context)
                .success &&
            automation.messages == std::vector<std::string>{"Gesture executed"} &&
            automation.on_screen_messages == std::vector<std::string>{"Volume: 50%"},
        "Lua displays user and auto-dismiss on-screen messages through the feedback service");
  check(!runtime.execute("ui.message()", context).success &&
            !runtime.execute("ui.osd(50)", context).success,
        "Lua user feedback validates message arguments");
  check(runtime
            .execute("return gesture.id == 'right' and gesture.name == 'Right' and "
                     "gesture.score == 0.97 and gesture.start.x == -20 and "
                     "gesture.start.y == 10 and gesture.finish.x == 30 and "
                     "gesture.finish.y == 50 and gesture.duration == 125 and "
                     "gesture.point_count == 3 and gesture.distance == 70",
                     context)
            .success,
        "Lua receives complete gesture recognition and stroke context");
  check(runtime
            .execute("return application.process == 'chrome.exe' and "
                     "application.process_id == 42 and application.title == 'Lua context test' "
                     "and application.class == 'Chrome_WidgetWin_1' and "
                     "application.executable_path == 'C:\\\\Program Files\\\\Chrome\\\\chrome.exe'",
                     context)
            .success,
        "Lua receives the captured application context");
  const auto gesture_write = runtime.execute("gesture.name = 'fake'", context);
  const auto application_write = runtime.execute("application.process = 'fake.exe'", context);
  const auto point_write = runtime.execute("gesture.start.x = 999", context);
  check(!gesture_write.success && !application_write.success && !point_write.success &&
            context.recognition->gesture_name == "Right" &&
            context.application.executable_name == "chrome.exe" &&
            context.gesture.start_position.x == -20,
        "Lua context objects and nested positions reject writes without mutating C++ state");
  check(runtime
            .execute("return dofile == nil and loadfile == nil and os == nil and io == nil and "
                     "package == nil and rawset == nil and rawget == nil and rawequal == nil and "
                     "rawlen == nil",
                     context)
            .success,
        "the supported Lua environment exposes no file, package or raw-access functions");
  check(!runtime.execute("rawset(gesture, 'name', 'fake')", context).success &&
            runtime.execute("return gesture.name == 'Right'", context).success,
        "the read-only context cannot be rewritten through a raw accessor");
  automation.fail_diagnostics = false;
  automation.diagnostics.clear();
  check(runtime.execute("print('one', 2)", context).success &&
            automation.diagnostics ==
                std::vector<std::pair<std::string, std::string>>{{"info", "one\t2"}},
        "print writes to the application log instead of an absent console");

  ActionContext uncaptured;
  uncaptured.captured = false;
  check(runtime.execute("return gesture == nil and application == nil", uncaptured).success &&
            !runtime.execute("return gesture.start.x", uncaptured).success,
        "a script that no gesture produced sees no gesture or application values");
}

void lua_execution_limit_tests() {
  ActionContext context;
  LuaRuntime limited(std::chrono::milliseconds(25));
  check(limited.execute("local total = 0; for i = 1, 100 do total = total + i end", context)
            .success,
        "short-running Lua scripts complete within the execution limit");
  const auto infinite = limited.execute("while true do end", context);
  check(!infinite.success && infinite.message.find("execution limit exceeded") != std::string::npos,
        "an infinite Lua loop is interrupted by the execution limit");
  const auto excessive = limited.execute(
      "local value = 0; while true do value = value + 1 end", context);
  check(!excessive.success &&
            excessive.message.find("execution limit exceeded") != std::string::npos,
        "an excessively long Lua script reports the execution limit");

  LuaRuntime cancellable(std::chrono::seconds(5));
  auto running = std::async(std::launch::async,
                            [&] { return cancellable.execute("while true do end", context); });
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  cancellable.request_cancel();
  const auto cancelled = running.get();
  check(!cancelled.success && cancelled.message.find("execution cancelled") != std::string::npos,
        "an executing Lua script responds to shutdown cancellation");

  const auto after_cancel = cancellable.execute("return true", context);
  check(!after_cancel.success && after_cancel.code == "lua_cancelled",
        "a cancelled runtime refuses later scripts instead of clearing the request");
  check(cancellable.reload({}).success && cancellable.execute("return true", context).success,
        "reloading the runtime clears the cancellation");
}

void display_name_tests() {
  check(action_display_name(ActionDefinition::keyboard("CTRL+N")) == "CTRL+N",
        "keyboard action summaries expose the actual key sequence");
  check(action_display_name(
            {1, ActionType::process,
             ProcessParameters{ProcessOperation::launch, "tool.exe", "--flag", {}}}) ==
            "Launch tool.exe --flag",
        "process action summaries expose the executable and arguments");
  check(action_display_name({1, ActionType::url, UrlParameters{"https://example.com"}}) ==
            "Open https://example.com",
        "URL action summaries expose the destination");
  check(action_display_name(
            {1, ActionType::mouse,
             MouseParameters{MouseOperation::double_click, MouseButton::left,
                             {PositionTarget::gesture_start, std::nullopt}}}) ==
            "Left double-click at gesture start",
        "mouse action summaries expose the operation, button, and position");
  check(action_display_name(
            {1, ActionType::window,
             WindowParameters{WindowOperation::move_resize, WindowTarget::gesture_window, 10, 20,
                              800, 600}}) ==
            "Move and resize gesture window to (10, 20) to 800x600",
        "window action summaries expose the operation, target, position, and dimensions");
  check(action_display_name(
            {1, ActionType::media, MediaParameters{MediaOperation::next_track}}) ==
            "Media Next Track",
        "media action summaries expose the operation");
  check(action_display_name(
            {1, ActionType::volume, VolumeParameters{VolumeOperation::increase, 12.5}}) ==
            "Volume Up 12.5%",
        "volume action summaries expose configured amounts");
  check(action_display_name(
            {1, ActionType::virtual_desktop,
             VirtualDesktopParameters{VirtualDesktopOperation::previous}}) ==
            "Previous Desktop",
        "virtual-desktop action summaries expose the operation");
  check(action_display_name(ActionDefinition::lua("window.maximize()\nreturn true")) ==
            "window.maximize()" &&
            action_label(ActionDefinition::lua("return true")) == "Lua Script",
        "Lua actions receive concise user-facing summaries");
  check(action_label(ActionDefinition::keyboard("ALT+SPACE,N")) == "Minimize" &&
            action_label(ActionDefinition::keyboard("win+up")) == "Maximize" &&
            action_label(ActionDefinition::keyboard("CTRL+N")) == "New",
        "common keyboard actions receive semantic list labels");
  check(action_label(ActionDefinition::keyboard("CTRL+SHIFT+F12")) == "Keyboard Shortcut",
        "unknown keyboard combinations retain a concise generic list label");
  check(action_label(ActionDefinition::keyboard("PAGEDOWN")) == "Page Down" &&
            action_label(ActionDefinition::keyboard("CTRL+HOME")) == "Start of Document" &&
            action_label(ActionDefinition::keyboard("F5")) == "Refresh",
        "navigation keyboard defaults receive semantic list labels");
  check(action_label(
            {1, ActionType::window,
             WindowParameters{WindowOperation::resize, WindowTarget::gesture_window, {}, {}, 800,
                              600}}) == "Resize Window" &&
            action_label({1, ActionType::process,
                          ProcessParameters{ProcessOperation::launch, "tool.exe", {}, {}}}) ==
                "Launch Program",
        "non-keyboard actions receive operation-level list labels");
}

}  // namespace

void run_action_definition_tests() {
  supported_definition_tests();
  invalid_definition_tests();
  symbolic_resolution_tests();
  action_result_tests();
  factory_tests();
  executor_tests();
  lua_runtime_tests();
  lua_execution_limit_tests();
  display_name_tests();
}

}  // namespace strokes::tests
