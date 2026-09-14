#include "actions/action_resolver.h"
#include "actions/windows_audio_service.h"
#include "actions/windows_keyboard_input.h"
#include "actions/windows_media_service.h"
#include "actions/windows_mouse_service.h"
#include "actions/windows_process_service.h"
#include "actions/windows_shell_service.h"
#include "actions/windows_window_service.h"
#include "actions/windows_virtual_desktop_service.h"
#include "config/configuration_store.h"
#include "config/windows_app_data.h"
#include "context/windows_application_context.h"
#include "engine/gesture_engine.h"
#include "gestures/recognizer.h"
#include "input/event_pump.h"
#include "input/mouse_input_router.h"
#include "input/windows_keyboard_hook.h"
#include "input/windows_modifier_state.h"
#include "input/windows_mouse_click.h"
#include "input/windows_mouse_hook.h"
#include "logging/structured_logger.h"
#include "overlay/windows_gesture_overlay.h"
#include "tray/windows_tray_icon.h"
#include "ui/windows_settings_window.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define PSAPI_VERSION 1
#include <Psapi.h>
#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cwchar>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace strokes;

std::filesystem::path application_directory() {
  std::vector<wchar_t> path(260);
  for (;;) {
    const DWORD length = ::GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    if (length == 0) return std::filesystem::current_path();
    if (length < path.size() - 1) return std::filesystem::path(path.data()).parent_path();
    path.resize(path.size() * 2);
  }
}

class EngineHost {
 public:
  explicit EngineHost(std::optional<std::filesystem::path> directory_override = std::nullopt)
      : router_([this](const input::MouseInputEvent& event) { return enqueue(event); }) {
    const bool has_directory_override = directory_override.has_value();
    const auto directory =
        directory_override ? std::move(directory_override) : config::configuration_directory();
    if (!directory) {
      ready_ = false;
      startup_error_ = "Unable to locate the Local AppData directory.";
      return;
    }
    configuration_directory_ = *directory;
    refresh_scale(::GetForegroundWindow());
    auto log_path = has_directory_override ? *directory / "log.txt"
                                           : std::filesystem::current_path() / "log.txt";
    logger_ = std::make_unique<logging::StructuredLogger>(
        log_path, logging::StructuredLogger::OpenMode::truncate);
    if (!logger_->ready() && !has_directory_override) {
      log_path = application_directory() / "log.txt";
      logger_ = std::make_unique<logging::StructuredLogger>(
          log_path, logging::StructuredLogger::OpenMode::truncate);
    }
    if (!logger_->ready()) {
      startup_error_ = "Logging could not be started. The application will continue without log.";
    }
    (void)logger_->log("application_start");
    config::ConfigurationStore store(*directory);
    auto loaded = store.load();
    if (loaded) {
      configuration_ = std::move(*loaded.value);
      if (!loaded.warnings.empty() && logger_)
        (void)logger_->log("configuration_warning", {{"message", loaded.warnings}});
    } else {
      configuration_ = config::ConfigurationStore::defaults();
      startup_error_ = loaded.error + "\n\nDefaults will be used for this session.";
      (void)logger_->log("configuration_error", {{"message", loaded.error}});
    }
    // Disabling is a per-run convenience; every new process starts enabled.
    configuration_.global.gestures_enabled = true;
    (void)logger_->log("configuration_load", {{"used_defaults", !loaded}});
    recognizer_.set_threshold(configuration_.global.recognition_threshold);
    for (const auto& gesture : configuration_.gestures.gestures) {
      (void)recognizer_.add_gesture(gesture);
    }
    apply_router();
  }

  [[nodiscard]] bool run(HINSTANCE instance, bool idle_test = false) {
    if (!ready_) {
      if (!idle_test) {
        ::MessageBoxA(nullptr, startup_error_.c_str(), "Strokes++ Startup Error",
                      MB_OK | MB_ICONERROR);
      }
      return false;
    }
    instance_ = instance;
    worker_ = std::jthread([this](std::stop_token stop) { engine_loop(stop); });
    const auto& overlay_config = configuration_.global.overlay;
    const auto color = overlay_config.color;
    overlay::WindowsGestureOverlay::Options overlay_options{
        overlay_config.enabled, overlay_config.line_width,
        static_cast<BYTE>(overlay_config.opacity * 255.0),
        RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)};
    if (!overlay_.create(instance, overlay_options)) {
      if (!idle_test)
        ::MessageBoxW(nullptr, L"The gesture overlay could not be created.",
                      L"Strokes++ Startup Error", MB_OK | MB_ICONERROR);
      worker_.request_stop();
      events_.wake();
      return false;
    }
    if (!tray_.create(instance, &EngineHost::handle_tray, this)) {
      if (!idle_test)
        ::MessageBoxW(nullptr, L"The notification-area icon could not be created.",
                      L"Strokes++ Startup Error", MB_OK | MB_ICONERROR);
      overlay_.destroy();
      worker_.request_stop();
      events_.wake();
      return false;
    }
    tray_.set_enabled(configuration_.global.gestures_enabled);
    if (!idle_test && !startup_error_.empty()) {
      ::MessageBoxA(nullptr, startup_error_.c_str(), "Strokes++ Configuration Warning",
                    MB_OK | MB_ICONWARNING);
    }
    active_host_.store(this, std::memory_order_release);
    foreground_hook_ = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
                                         &EngineHost::foreground_changed, 0, 0,
                                         WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    if (!hook_.start(&EngineHost::handle_mouse, this)) {
      if (!idle_test)
        ::MessageBoxW(nullptr, L"The global mouse hook could not be installed.",
                      L"Strokes++ Startup Error", MB_OK | MB_ICONERROR);
      if (logger_) (void)logger_->log("hook_failure");
      tray_.destroy();
      overlay_.destroy();
      worker_.request_stop();
      events_.wake();
      stop_foreground();
      return false;
    }
    if (!keyboard_hook_.start(&EngineHost::on_escape, this, physical_keys_)) {
      if (!idle_test)
        ::MessageBoxW(nullptr, L"The Escape-key cancellation hook could not be installed.",
                      L"Strokes++ Startup Error", MB_OK | MB_ICONERROR);
      hook_.stop();
      if (logger_) (void)logger_->log("hook_failure", {{"hook", "keyboard"}});
      tray_.destroy();
      worker_.request_stop();
      events_.wake();
      worker_.join();
      overlay_.destroy();
      stop_foreground();
      return false;
    }
    save_worker_ = std::jthread([this](std::stop_token stop) { save_loop(stop); });
    if (logger_) (void)logger_->log("hook_installation");

    MSG message{};
    std::jthread idle_monitor;
    if (idle_test) {
      (void)::PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
      const DWORD message_thread = ::GetCurrentThreadId();
      idle_monitor = std::jthread([this, message_thread] {
        FILETIME created{}, exited{}, kernel_before{}, user_before{}, kernel_after{}, user_after{};
        const bool before = ::GetProcessTimes(::GetCurrentProcess(), &created, &exited,
                                              &kernel_before, &user_before) != FALSE;
        ::Sleep(5000);
        const bool after = ::GetProcessTimes(::GetCurrentProcess(), &created, &exited,
                                             &kernel_after, &user_after) != FALSE;
        PROCESS_MEMORY_COUNTERS counters{};
        counters.cb = sizeof(counters);
        const bool memory =
            ::GetProcessMemoryInfo(::GetCurrentProcess(), &counters, sizeof(counters)) != FALSE;
        const auto ticks = [](const FILETIME& value) {
          return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32) | value.dwLowDateTime;
        };
        const double cpu_ms = before && after
                                  ? static_cast<double>(ticks(kernel_after) - ticks(kernel_before) +
                                                        ticks(user_after) - ticks(user_before)) /
                                        10000.0
                                  : 1000.0;
        idle_test_passed_.store(
            memory && cpu_ms < 50.0 && counters.WorkingSetSize < 50ull * 1024ull * 1024ull,
            std::memory_order_release);
        (void)::PostThreadMessageW(message_thread, WM_QUIT, 0, 0);
      });
    }
    while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
      ::TranslateMessage(&message);
      ::DispatchMessageW(&message);
    }
    keyboard_hook_.stop();
    hook_.stop();
    stop_foreground();
    tray_.destroy();
    worker_.request_stop();
    events_.wake();
    worker_.join();
    events_.clear();
    save_worker_.request_stop();
    save_wake_.notify_one();
    save_worker_.join();
    overlay_.destroy();
    if (logger_) (void)logger_->log("application_shutdown");
    return !idle_test || idle_test_passed_.load(std::memory_order_acquire);
  }

 private:
  bool enqueue(const input::MouseInputEvent& event) noexcept {
    if (event.type == input::MouseEventType::pointer_moved &&
        events_.size_approx() >= events_.usable_capacity() - 1) {
      return false;
    }
    return events_.push(event);
  }

  void refresh_scale(HWND window) noexcept {
    const UINT dpi = window != nullptr ? ::GetDpiForWindow(window) : ::GetDpiForSystem();
    threshold_scale_.store(dpi == 0 ? 1.0 : static_cast<double>(dpi) / 96.0,
                           std::memory_order_relaxed);
  }

  void apply_router() {
    router_.configure({configuration_.global.gesture_button,
                       configuration_.global.movement_threshold,
                       [this] { return threshold_scale_.load(std::memory_order_relaxed); }});
    router_.set_enabled(configuration_.global.gestures_enabled);
  }

  void stop_foreground() noexcept {
    active_host_.store(nullptr, std::memory_order_release);
    if (foreground_hook_ != nullptr) {
      ::UnhookWinEvent(foreground_hook_);
      foreground_hook_ = nullptr;
    }
  }

  static void CALLBACK foreground_changed(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD,
                                          DWORD) noexcept {
    if (auto* host = active_host_.load(std::memory_order_acquire); host != nullptr) {
      host->refresh_scale(window);
    }
  }

  static bool handle_mouse(const input::MouseInputEvent& event, void* context) noexcept {
    auto& host = *static_cast<EngineHost*>(context);
    if (event.type == input::MouseEventType::button_down ||
        event.type == input::MouseEventType::button_up) {
      POINT point{static_cast<LONG>(event.position.x), static_cast<LONG>(event.position.y)};
      HWND target = ::GetAncestor(::WindowFromPoint(point), GA_ROOT);
      wchar_t class_name[64]{};
      if (target != nullptr) ::GetClassNameW(target, class_name, 64);
      if (::lstrcmpW(class_name, L"Shell_TrayWnd") == 0 ||
          ::lstrcmpW(class_name, L"Shell_SecondaryTrayWnd") == 0 ||
          ::lstrcmpW(class_name, L"NotifyIconOverflowWindow") == 0) {
        return false;
      }
    }
    return host.router_.route(event).suppress_input;
  }

  static bool on_escape(void* context) noexcept {
    auto& host = *static_cast<EngineHost*>(context);
    if (!host.capturing_.load(std::memory_order_acquire)) return false;
    input::MouseInputEvent cancel;
    cancel.type = input::MouseEventType::cancel;
    if (!host.enqueue(cancel)) return false;
    (void)host.router_.cancel_interaction();
    return true;
  }

  static void handle_tray(tray::TrayCommand command, void* context) noexcept {
    auto& host = *static_cast<EngineHost*>(context);
    try {
      if (command == tray::TrayCommand::toggle) {
        const bool enabled = !host.configuration_.global.gestures_enabled;
        host.router_.set_enabled(enabled);
        host.tray_.set_enabled(enabled);
        host.configuration_.global.gestures_enabled = enabled;
        host.save_configuration();
      } else if (command == tray::TrayCommand::enable) {
        host.router_.set_enabled(true);
        host.tray_.set_enabled(true);
        host.configuration_.global.gestures_enabled = true;
        host.save_configuration();
      } else if (command == tray::TrayCommand::disable) {
        host.router_.set_enabled(false);
        host.tray_.set_enabled(false);
        host.configuration_.global.gestures_enabled = false;
        host.save_configuration();
      } else if (command == tray::TrayCommand::settings) {
        host.open_settings();
      } else if (command == tray::TrayCommand::suspend) {
        if (host.input_suspended_) return;
        host.input_suspended_ = true;
        host.router_.set_enabled(false);
        host.keyboard_hook_.stop();
        host.hook_.stop();
        host.overlay_.hide();
        if (host.logger_) (void)host.logger_->log("application_suspend");
      } else if (command == tray::TrayCommand::resume) {
        if (!host.input_suspended_) return;
        const bool mouse_started = host.hook_.start(&EngineHost::handle_mouse, &host);
        const bool keyboard_started =
            mouse_started &&
            host.keyboard_hook_.start(&EngineHost::on_escape, &host, host.physical_keys_);
        if (!keyboard_started) {
          host.keyboard_hook_.stop();
          host.hook_.stop();
        } else
          host.input_suspended_ = false;
        host.router_.set_enabled(keyboard_started && host.configuration_.global.gestures_enabled);
        if (host.logger_)
          (void)host.logger_->log(keyboard_started ? "application_resume" : "hook_failure",
                                  {{"phase", "resume"}});
      } else if (command == tray::TrayCommand::exit) {
        ::PostQuitMessage(0);
      }
    } catch (const std::exception& exception) {
      if (host.logger_) {
        (void)host.logger_->log("application_error", {{"message", exception.what()}});
      }
    } catch (...) {
      if (host.logger_) (void)host.logger_->log("application_error");
    }
  }

  void save_configuration() noexcept {
    try {
      {
        std::scoped_lock lock(save_mutex_);
        pending_save_ = configuration_;
      }
      save_wake_.notify_one();
    } catch (const std::exception& exception) {
      if (logger_) (void)logger_->log("configuration_error", {{"message", exception.what()}});
    } catch (...) {
      if (logger_) (void)logger_->log("configuration_error");
    }
  }

  void save_loop(std::stop_token stop) noexcept {
    for (;;) {
      std::optional<config::ConfigurationBundle> value;
      {
        std::unique_lock lock(save_mutex_);
        save_wake_.wait(lock, stop, [this] { return pending_save_.has_value(); });
        if (!pending_save_ && stop.stop_requested()) break;
        value = std::move(pending_save_);
        pending_save_.reset();
        save_active_ = true;
      }
      try {
        std::string error;
        config::ConfigurationStore store(configuration_directory_);
        if (!store.save(*value, error)) {
          if (logger_) (void)logger_->log("configuration_error", {{"message", error}});
        } else if (logger_) {
          (void)logger_->log("configuration_save");
        }
      } catch (const std::exception& exception) {
        if (logger_) (void)logger_->log("configuration_error", {{"message", exception.what()}});
      } catch (...) {
        if (logger_) (void)logger_->log("configuration_error");
      }
      {
        std::scoped_lock lock(save_mutex_);
        save_active_ = false;
      }
      save_wake_.notify_all();
    }
  }

  void flush_saves() noexcept {
    std::unique_lock lock(save_mutex_);
    save_wake_.wait(lock, [this] { return !pending_save_ && !save_active_; });
  }

  void open_settings() noexcept {
    if (settings_open_) return;
    settings_open_ = true;
    if (logger_) (void)logger_->log("settings_open_requested");
    try {
      router_.set_enabled(false);
      worker_.request_stop();
      events_.wake();
      worker_.join();
      events_.clear();
      overlay_.hide();
      flush_saves();

      ui::WindowsSettingsWindow settings;
      const bool accepted = settings.show(instance_, configuration_);
      if (logger_) (void)logger_->log("settings_closed", {{"accepted", accepted}});
      if (accepted) {
        std::string error;
        config::ConfigurationStore store(configuration_directory_);
        if (!store.save(configuration_, error)) {
          ::MessageBoxA(nullptr, error.c_str(), "Strokes++ Save Error", MB_OK | MB_ICONERROR);
          if (logger_) (void)logger_->log("configuration_error", {{"message", error}});
        } else if (logger_) {
          (void)logger_->log("configuration_save");
        }
        recognizer_.clear();
        recognizer_.set_threshold(configuration_.global.recognition_threshold);
        for (const auto& gesture : configuration_.gestures.gestures)
          (void)recognizer_.add_gesture(gesture);
        apply_router();
        const auto& o = configuration_.global.overlay;
        const auto color = o.color;
        overlay_.configure({o.enabled, o.line_width, static_cast<BYTE>(o.opacity * 255.0),
                            RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)});
      }
      router_.set_enabled(configuration_.global.gestures_enabled);
      tray_.set_enabled(configuration_.global.gestures_enabled);
      restart_engine();
    } catch (const std::exception& exception) {
      if (logger_) (void)logger_->log("settings_error", {{"message", exception.what()}});
      ::MessageBoxW(nullptr, L"Settings could not be applied.", L"Strokes++", MB_OK | MB_ICONERROR);
      router_.set_enabled(configuration_.global.gestures_enabled);
      restart_engine();
    } catch (...) {
      if (logger_) (void)logger_->log("settings_error");
      ::MessageBoxW(nullptr, L"Settings could not be applied.", L"Strokes++", MB_OK | MB_ICONERROR);
      router_.set_enabled(configuration_.global.gestures_enabled);
      restart_engine();
    }
    settings_open_ = false;
  }

  void restart_engine() noexcept {
    if (worker_.joinable()) return;
    worker_ = std::jthread([this](std::stop_token stop) { engine_loop(stop); });
  }

  void engine_loop(std::stop_token stop) {
    engine::GestureEngine engine(
        recognizer_, configuration_.profiles.profiles, configuration_.profiles.global_actions,
        application_context_, modifier_state_, mouse_click_, keyboard_input_,
        input::GestureStateMachine(
            {configuration_.global.minimum_point_distance, configuration_.global.maximum_points}),
        configuration_.global.overlay.enabled ? &overlay_ : nullptr,
        actions::ActionServices{.process = &process_service_,
                                .shell = &shell_service_,
                                .mouse = &mouse_service_,
                                .window = &window_service_,
                                .media = &media_service_,
                                .audio = &audio_service_,
                                .virtual_desktop = &desktop_service_});

    while (!stop.stop_requested()) {
      const auto event = events_.wait_pop();
      if (stop.stop_requested()) break;
      if (event) {
        try {
          process_event(engine, *event);
        } catch (const std::exception& exception) {
          capturing_.store(false, std::memory_order_release);
          overlay_.hide();
          if (logger_) (void)logger_->log("engine_error", {{"message", exception.what()}});
        } catch (...) {
          capturing_.store(false, std::memory_order_release);
          overlay_.hide();
          if (logger_) (void)logger_->log("engine_error");
        }
      }
    }
    // Input already accepted before shutdown is stale and must not delay exit
    // or be replayed when the settings window restarts this worker.
  }

  void process_event(engine::GestureEngine& engine, const input::MouseInputEvent& event) {
    const auto result = engine.process(event);
    if (result.gesture.capture_started) capturing_.store(true, std::memory_order_release);
    if (result.gesture.capture_cancelled || result.gesture.recognition_requested)
      capturing_.store(false, std::memory_order_release);
    if (result.gesture.capture_started && engine.session() && logger_) {
      const auto& app = engine.session()->application;
      (void)logger_->log("gesture_start", {{"process", app.executable_name},
                                           {"window_title", app.window_title},
                                           {"x", event.position.x},
                                           {"y", event.position.y}});
    }
    if (result.gesture.capture_cancelled && logger_) (void)logger_->log("gesture_cancellation");
    if (result.gesture.recognition_requested && logger_) {
      (void)logger_->log("gesture_completion");
      if (result.recognition) {
        (void)logger_->log("recognition_result",
                           {{"gesture_id", result.recognition->gesture_id},
                            {"gesture_name", result.recognition->gesture_name},
                            {"score", result.recognition->score}});
      } else {
        (void)logger_->log("recognition_result", {{"gesture_id", ""}, {"score", 0.0}});
      }
    }
    if (result.action_attempted && logger_) {
      const auto error = result.action_result ? result.action_result->error : actions::ActionError::none;
      const char* error_category = "none";
      switch (error) {
        case actions::ActionError::none: break;
        case actions::ActionError::invalid_definition: error_category = "invalid_definition"; break;
        case actions::ActionError::invalid_runtime_target:
          error_category = "invalid_runtime_target";
          break;
        case actions::ActionError::unsupported_operation:
          error_category = "unsupported_operation";
          break;
        case actions::ActionError::platform_failure: error_category = "platform_failure"; break;
        case actions::ActionError::execution_exception:
          error_category = "execution_exception";
          break;
      }
      (void)logger_->log(
          result.action_succeeded ? "action_execution" : "action_failure",
          {{"succeeded", result.action_succeeded},
           {"gesture_id", result.recognition ? result.recognition->gesture_id : ""},
           {"gesture_name", result.recognition ? result.recognition->gesture_name : ""},
           {"profile_id", result.profile_id},
           {"action_type", result.action_type},
           {"operation", result.action_operation},
           {"target", result.action_target},
           {"error_category", error_category},
           {"error_code", result.action_result ? result.action_result->code : ""},
           {"message", result.action_result ? result.action_result->message : ""},
           {"source", result.action_source == actions::ActionSource::application_profile
                          ? "application_profile"
                          : "global"}});
    }
  }

  config::ConfigurationBundle configuration_{config::ConfigurationStore::defaults()};
  std::filesystem::path configuration_directory_;
  HINSTANCE instance_{};
  std::unique_ptr<logging::StructuredLogger> logger_;
  bool ready_{true};
  bool input_suspended_{};
  bool settings_open_{};
  std::string startup_error_;
  std::atomic<double> threshold_scale_{1.0};
  HWINEVENTHOOK foreground_hook_{};
  inline static std::atomic<EngineHost*> active_host_{nullptr};
  std::atomic<bool> idle_test_passed_{false};
  std::atomic<bool> capturing_{false};
  gestures::Recognizer recognizer_;
  context::WindowsApplicationContextProvider application_context_;
  input::WindowsModifierStateProvider modifier_state_;
  input::WindowsMouseClick mouse_click_;
  actions::PhysicalKeyState physical_keys_;
  actions::WindowsKeyboardInput keyboard_input_{physical_keys_};
  actions::WindowsProcessService process_service_;
  actions::WindowsShellService shell_service_;
  actions::WindowsMouseService mouse_service_;
  actions::WindowsWindowService window_service_;
  actions::WindowsMediaService media_service_;
  actions::WindowsAudioService audio_service_;
  actions::WindowsVirtualDesktopService desktop_service_;
  input::EventPump<input::MouseInputEvent, 4096> events_;
  std::mutex save_mutex_;
  std::condition_variable_any save_wake_;
  std::optional<config::ConfigurationBundle> pending_save_;
  bool save_active_{};
  input::MouseInputRouter router_;
  input::WindowsMouseHook hook_;
  input::WindowsKeyboardHook keyboard_hook_;
  overlay::WindowsGestureOverlay overlay_;
  tray::WindowsTrayIcon tray_;
  std::jthread worker_;
  std::jthread save_worker_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR command_line, int) {
  (void)::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  const bool idle_test =
      command_line != nullptr && std::wcsstr(command_line, L"--idle-test") != nullptr;
  HANDLE instance_mutex = nullptr;
  if (!idle_test) {
    instance_mutex = ::CreateMutexW(nullptr, FALSE, L"Local\\StrokesPlusPlus.SingleInstance");
    if (instance_mutex == nullptr || ::GetLastError() == ERROR_ALREADY_EXISTS) {
      if (instance_mutex != nullptr) ::CloseHandle(instance_mutex);
      return EXIT_SUCCESS;
    }
  }
  std::optional<std::filesystem::path> test_directory;
  if (idle_test) {
    std::error_code error;
    const auto temporary = std::filesystem::temp_directory_path(error);
    if (error) return EXIT_FAILURE;
    test_directory =
        temporary / (L"strokes-plus-plus-idle-test-" + std::to_wstring(::GetCurrentProcessId()));
    std::filesystem::remove_all(*test_directory, error);
    if (error) return EXIT_FAILURE;
  }
  int result = EXIT_FAILURE;
  {
    EngineHost host(test_directory);
    result = host.run(instance, idle_test) ? EXIT_SUCCESS : EXIT_FAILURE;
  }
  if (test_directory) {
    std::error_code error;
    std::filesystem::remove_all(*test_directory, error);
  }
  if (instance_mutex != nullptr) ::CloseHandle(instance_mutex);
  return result;
}
