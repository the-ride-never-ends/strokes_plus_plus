#include "actions/lua_runtime.h"

#include "actions/symbolic_resolver.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

namespace strokes::actions {
namespace {

std::string lua_error_message(lua_State* state) {
  const char* message = lua_tostring(state, -1);
  return message ? message : "Unknown Lua error.";
}

ActionResult load(lua_State* state, std::string_view script) {
  if (luaL_loadbuffer(state, script.data(), script.size(), "gesture-action") == LUA_OK)
    return ActionResult::succeeded();
  auto message = lua_error_message(state);
  lua_pop(state, 1);
  return ActionResult::failed(ActionError::invalid_definition, "lua_syntax_error",
                              std::move(message));
}

int reject_context_write(lua_State* state) {
  return luaL_error(state, "context values are read-only");
}

void set_string(lua_State* state, const char* name, std::string_view value) {
  lua_pushlstring(state, value.data(), value.size());
  lua_setfield(state, -2, name);
}

void set_number(lua_State* state, const char* name, double value) {
  lua_pushnumber(state, value);
  lua_setfield(state, -2, name);
}

void make_read_only(lua_State* state) {
  lua_newtable(state);
  lua_newtable(state);
  lua_pushvalue(state, -3);
  lua_setfield(state, -2, "__index");
  lua_pushcfunction(state, reject_context_write);
  lua_setfield(state, -2, "__newindex");
  lua_setmetatable(state, -2);
  lua_remove(state, -2);
}

void push_point(lua_State* state, const gestures::Point& point) {
  lua_newtable(state);
  set_number(state, "x", point.x);
  set_number(state, "y", point.y);
  make_read_only(state);
}

double stroke_distance(const gestures::Stroke& points) {
  double result = 0.0;
  for (std::size_t index = 1; index < points.size(); ++index) {
    result += std::hypot(points[index].x - points[index - 1].x,
                         points[index].y - points[index - 1].y);
  }
  return result;
}

void install_context(lua_State* state, const ActionContext& context) {
  lua_newtable(state);
  if (context.recognition) {
    set_string(state, "id", context.recognition->gesture_id);
    set_string(state, "name", context.recognition->gesture_name);
    set_number(state, "score", context.recognition->score);
  }
  push_point(state, context.gesture.start_position);
  lua_setfield(state, -2, "start");
  push_point(state, context.gesture.current_position);
  lua_setfield(state, -2, "finish");
  const auto duration = std::chrono::duration<double, std::milli>(
                            context.gesture.last_movement_timestamp -
                            context.gesture.start_timestamp)
                            .count();
  set_number(state, "duration", (std::max)(0.0, duration));
  lua_pushinteger(state, static_cast<lua_Integer>(context.gesture.captured_points.size()));
  lua_setfield(state, -2, "point_count");
  set_number(state, "distance", stroke_distance(context.gesture.captured_points));
  make_read_only(state);
  lua_setglobal(state, "gesture");

  lua_newtable(state);
  set_string(state, "process", context.application.executable_name);
  lua_pushinteger(state, static_cast<lua_Integer>(context.application.process_id));
  lua_setfield(state, -2, "process_id");
  set_string(state, "title", context.application.window_title);
  set_string(state, "class", context.application.window_class);
  if (!context.application.executable_path.empty())
    set_string(state, "executable_path", context.application.executable_path);
  make_read_only(state);
  lua_setglobal(state, "application");
}

ActionServices& services(lua_State* state) {
  return *static_cast<ActionServices*>(lua_touserdata(state, lua_upvalueindex(1)));
}

constexpr char action_context_registry_key[] = "strokes.action_context";
constexpr char execution_guard_registry_key[] = "strokes.execution_guard";
constexpr char module_directory_registry_key[] = "strokes.module_directory";
constexpr char module_cache_registry_key[] = "strokes.module_cache";

struct ExecutionGuard {
  std::chrono::steady_clock::time_point deadline;
  const std::atomic_bool* cancelled{};
};

void execution_hook(lua_State* state, lua_Debug*) {
  lua_getfield(state, LUA_REGISTRYINDEX, execution_guard_registry_key);
  const auto* guard = static_cast<const ExecutionGuard*>(lua_touserdata(state, -1));
  lua_pop(state, 1);
  if (!guard) return;
  if (guard->cancelled->load(std::memory_order_relaxed))
    luaL_error(state, "script execution cancelled");
  if (std::chrono::steady_clock::now() >= guard->deadline)
    luaL_error(state, "script execution limit exceeded");
}

const ActionContext* action_context(lua_State* state) {
  lua_getfield(state, LUA_REGISTRYINDEX, action_context_registry_key);
  const auto* result = static_cast<const ActionContext*>(lua_touserdata(state, -1));
  lua_pop(state, 1);
  return result;
}

void set_action_context(lua_State* state, const ActionContext* context) {
  if (context)
    lua_pushlightuserdata(state, const_cast<ActionContext*>(context));
  else
    lua_pushnil(state);
  lua_setfield(state, LUA_REGISTRYINDEX, action_context_registry_key);
}

std::string_view string_argument(lua_State* state, int index) {
  if (lua_type(state, index) != LUA_TSTRING)
    luaL_argerror(state, index, "string expected");
  std::size_t size = 0;
  const char* value = lua_tolstring(state, index, &size);
  return {value, size};
}

int return_result(lua_State* state, const ActionResult& result) {
  if (!result.success)
    return luaL_error(state, "%s: %s", result.code.c_str(), result.message.c_str());
  lua_pushboolean(state, 1);
  return 1;
}

int keyboard_hotkey(lua_State* state) {
  const int count = lua_gettop(state);
  if (count < 1) return luaL_error(state, "keyboard.hotkey requires at least one key");
  std::string shortcut;
  for (int index = 1; index <= count; ++index) {
    if (!shortcut.empty()) shortcut += '+';
    shortcut += string_argument(state, index);
  }
  auto* keyboard = services(state).keyboard;
  if (!keyboard) return luaL_error(state, "keyboard service is unavailable");
  return return_result(state, keyboard->send_shortcut(shortcut));
}

int keyboard_press(lua_State* state) {
  if (lua_gettop(state) != 1) return luaL_error(state, "keyboard.press requires one key");
  auto* keyboard = services(state).keyboard;
  if (!keyboard) return luaL_error(state, "keyboard service is unavailable");
  return return_result(state, keyboard->send_shortcut(string_argument(state, 1)));
}

int keyboard_event(lua_State* state, bool key_down) {
  if (lua_gettop(state) != 1) return luaL_error(state, "keyboard key event requires one key");
  auto* keyboard = services(state).keyboard;
  if (!keyboard) return luaL_error(state, "keyboard service is unavailable");
  return return_result(state, keyboard->send_key(string_argument(state, 1), key_down));
}

int keyboard_down(lua_State* state) { return keyboard_event(state, true); }
int keyboard_up(lua_State* state) { return keyboard_event(state, false); }

int keyboard_is_down(lua_State* state) {
  if (lua_gettop(state) != 1) return luaL_error(state, "keyboard.is_down requires one key");
  auto* keyboard = services(state).keyboard;
  if (!keyboard) return luaL_error(state, "keyboard service is unavailable");
  const auto result = keyboard->is_key_down(string_argument(state, 1));
  if (!result) return luaL_error(state, "invalid or unavailable keyboard key");
  lua_pushboolean(state, *result);
  return 1;
}

int process_launch(lua_State* state) {
  const int count = lua_gettop(state);
  if (count < 1 || count > 3)
    return luaL_error(
        state, "process.launch requires path and optional arguments and working directory");
  ProcessParameters parameters;
  parameters.path = string_argument(state, 1);
  if (count >= 2) parameters.arguments = string_argument(state, 2);
  if (count >= 3) parameters.working_directory = string_argument(state, 3);
  if (parameters.path.empty()) return luaL_error(state, "process path must not be empty");
  auto* process = services(state).process;
  if (!process) return luaL_error(state, "process service is unavailable");
  return return_result(state, process->launch(parameters));
}

int shell_open(lua_State* state) {
  if (lua_gettop(state) != 1) return luaL_error(state, "shell.open requires one URI");
  const auto uri = string_argument(state, 1);
  if (uri.empty()) return luaL_error(state, "URI must not be empty");
  auto* shell = services(state).shell;
  if (!shell) return luaL_error(state, "shell service is unavailable");
  return return_result(state, shell->open_uri(uri));
}

int media_action(lua_State* state, MediaOperation operation) {
  if (lua_gettop(state) != 0) return luaL_error(state, "media function accepts no arguments");
  auto* media = services(state).media;
  if (!media) return luaL_error(state, "media service is unavailable");
  return return_result(state, media->perform(operation));
}

int media_play_pause(lua_State* state) {
  return media_action(state, MediaOperation::play_pause);
}
int media_next(lua_State* state) { return media_action(state, MediaOperation::next_track); }
int media_previous(lua_State* state) {
  return media_action(state, MediaOperation::previous_track);
}
int media_stop(lua_State* state) { return media_action(state, MediaOperation::stop); }

int desktop_action(lua_State* state, VirtualDesktopOperation operation) {
  if (lua_gettop(state) != 0) return luaL_error(state, "desktop function accepts no arguments");
  auto* desktop = services(state).virtual_desktop;
  if (!desktop) return luaL_error(state, "virtual desktop service is unavailable");
  return return_result(state, desktop->perform(operation));
}

int desktop_next(lua_State* state) {
  return desktop_action(state, VirtualDesktopOperation::next);
}
int desktop_previous(lua_State* state) {
  return desktop_action(state, VirtualDesktopOperation::previous);
}
int desktop_create(lua_State* state) {
  return desktop_action(state, VirtualDesktopOperation::create);
}
int desktop_close(lua_State* state) {
  return desktop_action(state, VirtualDesktopOperation::close);
}

double number_argument(lua_State* state, int index) {
  if (lua_type(state, index) != LUA_TNUMBER)
    luaL_argerror(state, index, "number expected");
  const double value = lua_tonumber(state, index);
  if (!std::isfinite(value)) luaL_argerror(state, index, "finite number expected");
  return value;
}

std::optional<MouseButton> mouse_button(std::string_view name) {
  std::string normalized(name);
  std::ranges::transform(normalized, normalized.begin(), [](unsigned char character) {
    return static_cast<char>(std::tolower(character));
  });
  if (normalized == "left") return MouseButton::left;
  if (normalized == "right") return MouseButton::right;
  if (normalized == "middle") return MouseButton::middle;
  if (normalized == "x1") return MouseButton::x_button_1;
  if (normalized == "x2") return MouseButton::x_button_2;
  return std::nullopt;
}

int mouse_position(lua_State* state) {
  if (lua_gettop(state) != 0) return luaL_error(state, "mouse.position accepts no arguments");
  auto* mouse = services(state).mouse;
  if (!mouse) return luaL_error(state, "mouse service is unavailable");
  const auto position = mouse->current_position();
  if (!position) return luaL_error(state, "current mouse position is unavailable");
  push_point(state, *position);
  return 1;
}

int mouse_move(lua_State* state) {
  if (lua_gettop(state) != 2) return luaL_error(state, "mouse.move requires X and Y");
  auto* mouse = services(state).mouse;
  if (!mouse) return luaL_error(state, "mouse service is unavailable");
  const gestures::Point position{number_argument(state, 1), number_argument(state, 2)};
  return return_result(state,
                       mouse->perform(MouseOperation::move, std::nullopt, position));
}

int mouse_button_action(lua_State* state, MouseOperation operation) {
  if (lua_gettop(state) != 1) return luaL_error(state, "mouse button action requires one button");
  const auto button = mouse_button(string_argument(state, 1));
  if (!button) return luaL_error(state, "unsupported mouse button");
  auto* mouse = services(state).mouse;
  if (!mouse) return luaL_error(state, "mouse service is unavailable");
  const auto position = mouse->current_position();
  if (!position) return luaL_error(state, "current mouse position is unavailable");
  return return_result(state, mouse->perform(operation, *button, *position));
}

int mouse_click(lua_State* state) {
  return mouse_button_action(state, MouseOperation::click);
}
int mouse_double_click(lua_State* state) {
  return mouse_button_action(state, MouseOperation::double_click);
}
int mouse_down(lua_State* state) {
  return mouse_button_action(state, MouseOperation::button_down);
}
int mouse_up(lua_State* state) {
  return mouse_button_action(state, MouseOperation::button_up);
}

std::optional<WindowTarget> window_target(lua_State* state, int index) {
  if (index > lua_gettop(state)) return WindowTarget::gesture_window;
  const auto name = string_argument(state, index);
  if (name == "gesture") return WindowTarget::gesture_window;
  if (name == "foreground") return WindowTarget::foreground_window;
  return std::nullopt;
}

int integer_argument(lua_State* state, int index, bool positive = false) {
  const double value = number_argument(state, index);
  if (value < static_cast<double>((std::numeric_limits<int>::min)()) ||
      value > static_cast<double>((std::numeric_limits<int>::max)()) ||
      (positive && value <= 0.0))
    luaL_argerror(state, index, positive ? "positive integer expected" : "integer out of range");
  return static_cast<int>(value);
}

std::optional<std::uintptr_t> lua_window(lua_State* state, WindowTarget target) {
  const auto* context = action_context(state);
  return context ? resolve_window(target, *context) : std::nullopt;
}

int window_perform(lua_State* state, WindowOperation operation, WindowParameters parameters,
                   int target_index) {
  const auto target = window_target(state, target_index);
  if (!target) return luaL_error(state, "unsupported window target");
  const auto window = lua_window(state, *target);
  if (!window || *window == 0) return luaL_error(state, "window target is unavailable");
  auto* service = services(state).window;
  if (!service) return luaL_error(state, "window service is unavailable");
  parameters.operation = operation;
  parameters.target = *target;
  return return_result(state, service->perform(operation, *window, parameters));
}

int window_simple(lua_State* state, WindowOperation operation) {
  if (lua_gettop(state) > 1) return luaL_error(state, "window action accepts one optional target");
  return window_perform(state, operation, {}, 1);
}

int window_close(lua_State* state) { return window_simple(state, WindowOperation::close); }
int window_minimize(lua_State* state) { return window_simple(state, WindowOperation::minimize); }
int window_maximize(lua_State* state) { return window_simple(state, WindowOperation::maximize); }
int window_restore(lua_State* state) { return window_simple(state, WindowOperation::restore); }
int window_activate(lua_State* state) { return window_simple(state, WindowOperation::activate); }

int window_move(lua_State* state) {
  const int count = lua_gettop(state);
  if (count < 2 || count > 3)
    return luaL_error(state, "window.move requires X, Y, and an optional target");
  WindowParameters parameters;
  parameters.x = integer_argument(state, 1);
  parameters.y = integer_argument(state, 2);
  return window_perform(state, WindowOperation::move, parameters, 3);
}

int window_resize(lua_State* state) {
  const int count = lua_gettop(state);
  if (count < 2 || count > 3)
    return luaL_error(state, "window.resize requires width, height, and an optional target");
  WindowParameters parameters;
  parameters.width = integer_argument(state, 1, true);
  parameters.height = integer_argument(state, 2, true);
  return window_perform(state, WindowOperation::resize, parameters, 3);
}

int window_move_resize(lua_State* state) {
  const int count = lua_gettop(state);
  if (count < 4 || count > 5)
    return luaL_error(state,
                      "window.move_resize requires X, Y, width, height, and an optional target");
  WindowParameters parameters;
  parameters.x = integer_argument(state, 1);
  parameters.y = integer_argument(state, 2);
  parameters.width = integer_argument(state, 3, true);
  parameters.height = integer_argument(state, 4, true);
  return window_perform(state, WindowOperation::move_resize, parameters, 5);
}

int window_bounds(lua_State* state) {
  if (lua_gettop(state) > 1) return luaL_error(state, "window.bounds accepts one optional target");
  const auto target = window_target(state, 1);
  if (!target) return luaL_error(state, "unsupported window target");
  const auto window = lua_window(state, *target);
  auto* service = services(state).window;
  if (!service) return luaL_error(state, "window service is unavailable");
  const auto bounds = window ? service->bounds(*window) : std::nullopt;
  if (!bounds) return luaL_error(state, "window bounds are unavailable");
  lua_newtable(state);
  set_number(state, "x", bounds->left);
  set_number(state, "y", bounds->top);
  set_number(state, "width", bounds->width);
  set_number(state, "height", bounds->height);
  make_read_only(state);
  return 1;
}

int window_exists(lua_State* state) {
  if (lua_gettop(state) > 1) return luaL_error(state, "window.exists accepts one optional target");
  const auto target = window_target(state, 1);
  if (!target) return luaL_error(state, "unsupported window target");
  const auto window = lua_window(state, *target);
  auto* service = services(state).window;
  if (!service) return luaL_error(state, "window service is unavailable");
  lua_pushboolean(state, window && service->exists(*window));
  return 1;
}

enum class WindowTextProperty { title, class_name, process_name };

int window_text(lua_State* state, WindowTextProperty property) {
  if (lua_gettop(state) > 1)
    return luaL_error(state, "window information accepts one optional target");
  const auto target = window_target(state, 1);
  if (!target) return luaL_error(state, "unsupported window target");
  const auto window = lua_window(state, *target);
  auto* service = services(state).window;
  if (!service) return luaL_error(state, "window service is unavailable");
  if (!window || !service->exists(*window))
    return luaL_error(state, "window target is unavailable");
  std::optional<std::string> value;
  switch (property) {
    case WindowTextProperty::title: value = service->title(*window); break;
    case WindowTextProperty::class_name: value = service->class_name(*window); break;
    case WindowTextProperty::process_name: value = service->process_name(*window); break;
  }
  if (!value) return luaL_error(state, "window information is unavailable");
  lua_pushlstring(state, value->data(), value->size());
  return 1;
}

int window_title(lua_State* state) { return window_text(state, WindowTextProperty::title); }
int window_class(lua_State* state) { return window_text(state, WindowTextProperty::class_name); }
int window_process(lua_State* state) {
  return window_text(state, WindowTextProperty::process_name);
}

int volume_change(lua_State* state, VolumeOperation operation) {
  if (lua_gettop(state) != 1) return luaL_error(state, "volume change requires one amount");
  const double amount = number_argument(state, 1);
  if (amount <= 0.0 || amount > 100.0)
    return luaL_error(state, "volume amount must be greater than 0 and at most 100");
  auto* audio = services(state).audio;
  if (!audio) return luaL_error(state, "audio service is unavailable");
  return return_result(state, audio->perform(operation, amount));
}

int volume_increase(lua_State* state) {
  return volume_change(state, VolumeOperation::increase);
}
int volume_decrease(lua_State* state) {
  return volume_change(state, VolumeOperation::decrease);
}
int volume_toggle_mute(lua_State* state) {
  if (lua_gettop(state) != 0)
    return luaL_error(state, "volume.toggle_mute accepts no arguments");
  auto* audio = services(state).audio;
  if (!audio) return luaL_error(state, "audio service is unavailable");
  return return_result(state, audio->perform(VolumeOperation::mute_toggle, std::nullopt));
}
int volume_get(lua_State* state) {
  if (lua_gettop(state) != 0) return luaL_error(state, "volume.get accepts no arguments");
  auto* audio = services(state).audio;
  if (!audio) return luaL_error(state, "audio service is unavailable");
  const auto value = audio->volume();
  if (!value) return luaL_error(state, "current volume is unavailable");
  lua_pushnumber(state, *value);
  return 1;
}
int volume_set(lua_State* state) {
  if (lua_gettop(state) != 1) return luaL_error(state, "volume.set requires one value");
  const double value = number_argument(state, 1);
  if (value < 0.0 || value > 100.0)
    return luaL_error(state, "volume must be between 0 and 100");
  auto* audio = services(state).audio;
  if (!audio) return luaL_error(state, "audio service is unavailable");
  return return_result(state, audio->set_volume(value));
}
int volume_is_muted(lua_State* state) {
  if (lua_gettop(state) != 0) return luaL_error(state, "volume.is_muted accepts no arguments");
  auto* audio = services(state).audio;
  if (!audio) return luaL_error(state, "audio service is unavailable");
  const auto muted = audio->is_muted();
  if (!muted) return luaL_error(state, "mute state is unavailable");
  lua_pushboolean(state, *muted);
  return 1;
}

int write_log(lua_State* state, std::string_view level) {
  if (lua_gettop(state) != 1)
    return luaL_error(state, "log.%s requires one message", std::string(level).c_str());
  const auto message = string_argument(state, 1);
  auto* diagnostics = services(state).diagnostics;
  if (!diagnostics) return luaL_error(state, "diagnostic service is unavailable");
  return return_result(state, diagnostics->write(level, message));
}

int log_debug(lua_State* state) { return write_log(state, "debug"); }
int log_info(lua_State* state) { return write_log(state, "info"); }
int log_warn(lua_State* state) { return write_log(state, "warn"); }
int log_error(lua_State* state) { return write_log(state, "error"); }

int user_feedback(lua_State* state, bool osd) {
  if (lua_gettop(state) != 1)
    return luaL_error(state, osd ? "ui.osd requires one message"
                                 : "ui.message requires one message");
  const auto message = string_argument(state, 1);
  auto* feedback = services(state).user_feedback;
  if (!feedback) return luaL_error(state, "user feedback service is unavailable");
  return return_result(state, osd ? feedback->osd(message) : feedback->message(message));
}

int ui_message(lua_State* state) { return user_feedback(state, false); }
int ui_osd(lua_State* state) { return user_feedback(state, true); }

bool valid_module_name(std::string_view name) {
  if (name.empty() || name.front() == '.' || name.back() == '.') return false;
  bool previous_dot = false;
  for (const unsigned char character : name) {
    if (character == '.') {
      if (previous_dot) return false;
      previous_dot = true;
      continue;
    }
    previous_dot = false;
    if (!std::isalnum(character) && character != '_') return false;
  }
  return true;
}

int require_module(lua_State* state) {
  if (lua_gettop(state) != 1) return luaL_error(state, "require requires one module name");
  const auto name = string_argument(state, 1);
  if (!valid_module_name(name)) return luaL_error(state, "module name is not approved");

  lua_getfield(state, LUA_REGISTRYINDEX, module_cache_registry_key);
  lua_getfield(state, -1, std::string(name).c_str());
  if (!lua_isnil(state, -1)) {
    lua_remove(state, -2);
    return 1;
  }
  lua_pop(state, 1);

  lua_getfield(state, LUA_REGISTRYINDEX, module_directory_registry_key);
  const char* directory = lua_tostring(state, -1);
  if (!directory) return luaL_error(state, "user module directory is unavailable");
  std::filesystem::path relative;
  std::string segment;
  for (const char character : name) {
    if (character == '.') {
      relative /= segment;
      segment.clear();
    } else {
      segment += character;
    }
  }
  relative /= segment + ".lua";
  const auto path = std::filesystem::path(directory) / relative;
  lua_pop(state, 1);

  std::ifstream input(path, std::ios::binary);
  if (!input) return luaL_error(state, "module '%s' was not found", std::string(name).c_str());
  const std::string source{std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>()};
  const std::string chunk_name = "@" + path.string();
  if (luaL_loadbuffer(state, source.data(), source.size(), chunk_name.c_str()) != LUA_OK) {
    lua_remove(state, -2);
    return lua_error(state);
  }
  if (lua_pcall(state, 0, 1, 0) != LUA_OK) {
    lua_remove(state, -2);
    return lua_error(state);
  }
  if (lua_isnil(state, -1)) {
    lua_pop(state, 1);
    lua_pushboolean(state, 1);
  }
  lua_pushvalue(state, -1);
  lua_setfield(state, -3, std::string(name).c_str());
  lua_remove(state, -2);
  return 1;
}

void add_service_function(lua_State* state, ActionServices* action_services, const char* name,
                          lua_CFunction function) {
  lua_pushlightuserdata(state, action_services);
  lua_pushcclosure(state, function, 1);
  lua_setfield(state, -2, name);
}

void install_keyboard(lua_State* state, ActionServices* action_services) {
  lua_newtable(state);
  const auto add = [&](const char* name, lua_CFunction function) {
    add_service_function(state, action_services, name, function);
  };
  add("hotkey", keyboard_hotkey);
  add("press", keyboard_press);
  add("down", keyboard_down);
  add("up", keyboard_up);
  add("is_down", keyboard_is_down);
  lua_setglobal(state, "keyboard");
}

void install_automation(lua_State* state, ActionServices* action_services) {
  lua_newtable(state);
  add_service_function(state, action_services, "launch", process_launch);
  lua_setglobal(state, "process");

  lua_newtable(state);
  add_service_function(state, action_services, "open", shell_open);
  lua_setglobal(state, "shell");

  lua_newtable(state);
  add_service_function(state, action_services, "play_pause", media_play_pause);
  add_service_function(state, action_services, "next", media_next);
  add_service_function(state, action_services, "previous", media_previous);
  add_service_function(state, action_services, "stop", media_stop);
  lua_setglobal(state, "media");

  lua_newtable(state);
  add_service_function(state, action_services, "next", desktop_next);
  add_service_function(state, action_services, "previous", desktop_previous);
  add_service_function(state, action_services, "create", desktop_create);
  add_service_function(state, action_services, "close", desktop_close);
  lua_setglobal(state, "desktop");

  lua_newtable(state);
  add_service_function(state, action_services, "position", mouse_position);
  add_service_function(state, action_services, "move", mouse_move);
  add_service_function(state, action_services, "click", mouse_click);
  add_service_function(state, action_services, "double_click", mouse_double_click);
  add_service_function(state, action_services, "down", mouse_down);
  add_service_function(state, action_services, "up", mouse_up);
  lua_setglobal(state, "mouse");

  lua_newtable(state);
  add_service_function(state, action_services, "close", window_close);
  add_service_function(state, action_services, "minimize", window_minimize);
  add_service_function(state, action_services, "maximize", window_maximize);
  add_service_function(state, action_services, "restore", window_restore);
  add_service_function(state, action_services, "activate", window_activate);
  add_service_function(state, action_services, "move", window_move);
  add_service_function(state, action_services, "resize", window_resize);
  add_service_function(state, action_services, "move_resize", window_move_resize);
  add_service_function(state, action_services, "bounds", window_bounds);
  add_service_function(state, action_services, "exists", window_exists);
  add_service_function(state, action_services, "title", window_title);
  add_service_function(state, action_services, "class", window_class);
  add_service_function(state, action_services, "process", window_process);
  lua_setglobal(state, "window");

  lua_newtable(state);
  add_service_function(state, action_services, "increase", volume_increase);
  add_service_function(state, action_services, "decrease", volume_decrease);
  add_service_function(state, action_services, "toggle_mute", volume_toggle_mute);
  add_service_function(state, action_services, "get", volume_get);
  add_service_function(state, action_services, "set", volume_set);
  add_service_function(state, action_services, "is_muted", volume_is_muted);
  lua_setglobal(state, "volume");

  lua_newtable(state);
  add_service_function(state, action_services, "debug", log_debug);
  add_service_function(state, action_services, "info", log_info);
  add_service_function(state, action_services, "warn", log_warn);
  add_service_function(state, action_services, "error", log_error);
  lua_setglobal(state, "log");

  lua_newtable(state);
  add_service_function(state, action_services, "message", ui_message);
  add_service_function(state, action_services, "osd", ui_osd);
  lua_setglobal(state, "ui");
}

lua_State* create_state(ActionServices* action_services) {
  lua_State* state = luaL_newstate();
  if (!state) return nullptr;
  luaL_requiref(state, LUA_GNAME, luaopen_base, 1);
  lua_pop(state, 1);
  lua_pushnil(state);
  lua_setglobal(state, "dofile");
  lua_pushnil(state);
  lua_setglobal(state, "loadfile");
  luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
  lua_pop(state, 1);
  luaL_requiref(state, LUA_UTF8LIBNAME, luaopen_utf8, 1);
  lua_pop(state, 1);
  lua_newtable(state);
  lua_setfield(state, LUA_REGISTRYINDEX, module_cache_registry_key);
  lua_pushcfunction(state, require_module);
  lua_setglobal(state, "require");
  install_keyboard(state, action_services);
  install_automation(state, action_services);
  return state;
}

}  // namespace

LuaRuntime::LuaRuntime(std::chrono::milliseconds execution_limit)
    : state_(create_state(&services_)), execution_limit_(execution_limit) {}

LuaRuntime::~LuaRuntime() {
  if (state_) lua_close(state_);
}

void LuaRuntime::set_services(ActionServices services) {
  std::lock_guard lock(mutex_);
  services_ = services;
}

void LuaRuntime::set_module_directory(const std::filesystem::path& directory) {
  std::lock_guard lock(mutex_);
  module_directory_ = directory;
  if (!state_) return;
  const auto value = directory.string();
  lua_pushlstring(state_, value.data(), value.size());
  lua_setfield(state_, LUA_REGISTRYINDEX, module_directory_registry_key);
}

void LuaRuntime::request_cancel() noexcept {
  cancel_requested_.store(true, std::memory_order_relaxed);
}

ActionResult LuaRuntime::validate_script(std::string_view script) {
  std::lock_guard lock(mutex_);
  if (!state_)
    return ActionResult::failed(ActionError::unsupported_operation, "lua_runtime_unavailable",
                                "The Lua runtime could not be created.");
  const auto result = load(state_, script);
  if (result.success) lua_pop(state_, 1);
  return result;
}

ActionResult LuaRuntime::initialize(std::string_view script) {
  if (script.empty()) return ActionResult::succeeded();
  return execute(script, {});
}

ActionResult LuaRuntime::reload(std::string_view initialization_script) {
  {
    std::lock_guard lock(mutex_);
    lua_State* replacement = create_state(&services_);
    if (!replacement)
      return ActionResult::failed(ActionError::unsupported_operation,
                                  "lua_runtime_unavailable",
                                  "The Lua runtime could not be recreated.");
    if (!module_directory_.empty()) {
      const auto value = module_directory_.string();
      lua_pushlstring(replacement, value.data(), value.size());
      lua_setfield(replacement, LUA_REGISTRYINDEX, module_directory_registry_key);
    }
    lua_State* previous = state_;
    state_ = replacement;
    if (previous) lua_close(previous);
  }
  return initialize(initialization_script);
}

ActionResult LuaRuntime::execute(std::string_view script, const ActionContext& context) {
  std::lock_guard lock(mutex_);
  if (!state_)
    return ActionResult::failed(ActionError::unsupported_operation, "lua_runtime_unavailable",
                                "The Lua runtime could not be created.");

  install_context(state_, context);
  set_action_context(state_, &context);
  cancel_requested_.store(false, std::memory_order_relaxed);
  auto result = load(state_, script);
  if (!result.success) {
    set_action_context(state_, nullptr);
    return result;
  }
  ExecutionGuard guard{std::chrono::steady_clock::now() + execution_limit_, &cancel_requested_};
  lua_pushlightuserdata(state_, &guard);
  lua_setfield(state_, LUA_REGISTRYINDEX, execution_guard_registry_key);
  lua_sethook(state_, execution_hook, LUA_MASKCOUNT, 1000);
  const int status = lua_pcall(state_, 0, 1, 0);
  lua_sethook(state_, nullptr, 0, 0);
  lua_pushnil(state_);
  lua_setfield(state_, LUA_REGISTRYINDEX, execution_guard_registry_key);
  if (status != LUA_OK) {
    auto message = lua_error_message(state_);
    lua_pop(state_, 1);
    set_action_context(state_, nullptr);
    return ActionResult::failed(ActionError::execution_exception, "lua_runtime_error",
                                std::move(message));
  }

  const bool explicit_failure = lua_isboolean(state_, -1) && !lua_toboolean(state_, -1);
  lua_pop(state_, 1);
  set_action_context(state_, nullptr);
  return explicit_failure
             ? ActionResult::failed(ActionError::platform_failure, "lua_returned_false",
                                    "The Lua script returned false.")
             : ActionResult::succeeded();
}

}  // namespace strokes::actions
