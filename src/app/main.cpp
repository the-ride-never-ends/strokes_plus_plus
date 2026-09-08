#include "actions/action_resolver.h"
#include "actions/windows_keyboard_input.h"
#include "context/windows_application_context.h"
#include "config/configuration_store.h"
#include "config/windows_app_data.h"
#include "engine/gesture_engine.h"
#include "gestures/recognizer.h"
#include "input/input_queue.h"
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
#include <Windows.h>

#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <memory>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace {

using namespace strokes;

class EngineHost {
public:
    EngineHost()
        : router_([this](const input::MouseInputEvent& event) {
              if (event.type == input::MouseEventType::pointer_moved &&
                  queue_.size_approx() >= queue_.usable_capacity() - 1) return false;
              const bool queued = queue_.try_push(event);
              if (queued) {
                  wake_.notify_one();
              }
              return queued;
          }) {
        const auto directory = config::windows_configuration_directory();
        if (!directory) {
            ready_ = false;
            startup_error_ = "Unable to locate the Local AppData directory.";
            return;
        }
        configuration_directory_ = *directory;
        logger_ = std::make_unique<logging::StructuredLogger>(*directory / "logs" / "strokes.log");
        (void)logger_->log("application_start");
        config::ConfigurationStore store(*directory);
        auto loaded = store.load_or_create();
        if (loaded) {
            configuration_ = std::move(*loaded.value);
            if(!loaded.error.empty()&&logger_)(void)logger_->log("configuration_warning",{{"message",loaded.error}});
        } else {
            configuration_ = config::ConfigurationStore::defaults();
            startup_error_ = loaded.error + "\n\nDefaults will be used for this session.";
            (void)logger_->log("configuration_error", {{"message", loaded.error}});
        }
        (void)logger_->log("configuration_load", {{"used_defaults", !loaded}});
        recognizer_.set_threshold(configuration_.global.recognition_threshold);
        for (const auto& gesture : configuration_.gestures.gestures) {
            (void)recognizer_.add_gesture(gesture);
        }
        router_ = input::MouseInputRouter(
            {configuration_.global.gesture_button, configuration_.global.movement_threshold,
             []{const HWND window=::GetForegroundWindow();const UINT dpi=window!=nullptr ? ::GetDpiForWindow(window) : ::GetDpiForSystem();
                 return static_cast<double>(dpi)/96.0;}},
            [this](const input::MouseInputEvent& event) {
                if (event.type == input::MouseEventType::pointer_moved &&
                    queue_.size_approx() >= queue_.usable_capacity() - 1) return false;
                const bool queued = queue_.try_push(event);
                if (queued) wake_.notify_one();
                return queued;
            });
        router_.set_enabled(configuration_.global.gestures_enabled);
    }

    [[nodiscard]] bool run(HINSTANCE instance) {
        if (!ready_) return false;
        instance_ = instance;
        worker_ = std::jthread([this](std::stop_token stop) { engine_loop(stop); });
        const auto& overlay_config = configuration_.global.overlay;
        const auto color = overlay_config.color;
        overlay::WindowsGestureOverlay::Options overlay_options{
            overlay_config.enabled,
            overlay_config.line_width,
            static_cast<BYTE>(overlay_config.opacity * 255.0),
            RGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF)};
        if (!overlay_.create(instance, overlay_options)) {
            worker_.request_stop();
            wake_.notify_one();
            return false;
        }
        if (!tray_.create(instance, &EngineHost::on_tray_command, this)) {
            overlay_.destroy();
            worker_.request_stop();
            wake_.notify_one();
            return false;
        }
        tray_.set_enabled(configuration_.global.gestures_enabled);
        if (!startup_error_.empty()) {
            ::MessageBoxA(nullptr, startup_error_.c_str(), "Strokes++ Configuration Warning",
                          MB_OK | MB_ICONWARNING);
        }
        if (!hook_.start(&EngineHost::on_mouse_event, this)) {
            if (logger_) (void)logger_->log("hook_failure");
            tray_.destroy();
            overlay_.destroy();
            worker_.request_stop();
            wake_.notify_one();
            return false;
        }
        if (!keyboard_hook_.start(&EngineHost::on_escape, this)) {
            hook_.stop();
            if (logger_) (void)logger_->log("hook_failure", {{"hook", "keyboard"}});
            tray_.destroy();
            worker_.request_stop();
            wake_.notify_one();
            worker_.join();
            overlay_.destroy();
            return false;
        }
        if (logger_) (void)logger_->log("hook_installation");

        MSG message{};
        while (::GetMessageW(&message, nullptr, 0, 0) > 0) {
            ::TranslateMessage(&message);
            ::DispatchMessageW(&message);
        }
        keyboard_hook_.stop();
        hook_.stop();
        tray_.destroy();
        worker_.request_stop();
        wake_.notify_one();
        worker_.join();
        overlay_.destroy();
        if (logger_) (void)logger_->log("application_shutdown");
        return true;
    }

private:
    static bool on_mouse_event(const input::MouseInputEvent& event, void* context) noexcept {
        auto& host = *static_cast<EngineHost*>(context);
        return host.router_.route(event).suppress_input;
    }

    static bool on_escape(void* context) noexcept {
        auto& host = *static_cast<EngineHost*>(context);
        if (!host.router_.interaction_active()) return false;
        input::MouseInputEvent cancel;
        cancel.type = input::MouseEventType::cancel;
        if (!host.queue_.try_push(cancel)) return false;
        (void)host.router_.cancel_interaction();
        host.wake_.notify_one();
        return true;
    }

    static void on_tray_command(tray::TrayCommand command, void* context) noexcept {
        auto& host = *static_cast<EngineHost*>(context);
        if (command == tray::TrayCommand::enable) {
            host.router_.set_enabled(true);
            host.tray_.set_enabled(true);
            host.configuration_.global.gestures_enabled=true;
            host.save_configuration();
        } else if (command == tray::TrayCommand::disable) {
            host.router_.set_enabled(false);
            input::MouseInputEvent cancel;
            cancel.type = input::MouseEventType::cancel;
            if (host.queue_.try_push(cancel)) host.wake_.notify_one();
            host.tray_.set_enabled(false);
            host.configuration_.global.gestures_enabled=false;
            host.save_configuration();
        } else if (command == tray::TrayCommand::settings) {
            host.open_settings();
        } else if (command == tray::TrayCommand::suspend) {
            if(host.input_suspended_)return;
            host.input_suspended_=true;
            host.router_.set_enabled(false);
            (void)host.router_.cancel_interaction();
            input::MouseInputEvent cancel; cancel.type=input::MouseEventType::cancel;
            if(host.queue_.try_push(cancel))host.wake_.notify_one();
            host.keyboard_hook_.stop();
            host.hook_.stop();
            host.overlay_.hide();
            if(host.logger_)(void)host.logger_->log("application_suspend");
        } else if (command == tray::TrayCommand::resume) {
            if(!host.input_suspended_)return;
            const bool mouse_started=host.hook_.start(&EngineHost::on_mouse_event,&host);
            const bool keyboard_started=mouse_started&&host.keyboard_hook_.start(&EngineHost::on_escape,&host);
            if(!keyboard_started){host.keyboard_hook_.stop();host.hook_.stop();}
            else host.input_suspended_=false;
            host.router_.set_enabled(keyboard_started&&host.configuration_.global.gestures_enabled);
            if(host.logger_)(void)host.logger_->log(keyboard_started?"application_resume":"hook_failure",
                {{"phase","resume"}});
        } else if (command == tray::TrayCommand::exit) {
            ::PostQuitMessage(0);
        }
    }

    void save_configuration() noexcept {
        std::string error;config::ConfigurationStore store(configuration_directory_);
        if(!store.save(configuration_,error)){
            if(logger_)(void)logger_->log("configuration_error",{{"message",error}});
        }else if(logger_){(void)logger_->log("configuration_save");}
    }

    void open_settings() noexcept {
        try {
            router_.set_enabled(false);
            keyboard_hook_.stop();hook_.stop();
            input::MouseInputEvent cancel; cancel.type=input::MouseEventType::cancel;
            if(queue_.try_push(cancel))wake_.notify_one();
            worker_.request_stop();wake_.notify_one();worker_.join();overlay_.hide();

            ui::WindowsSettingsWindow settings;
            if(settings.show(instance_,configuration_)){
                std::string error;config::ConfigurationStore store(configuration_directory_);
                if(!store.save(configuration_,error)){
                    ::MessageBoxA(nullptr,error.c_str(),"Strokes++ Save Error",MB_OK|MB_ICONERROR);
                    if(logger_)(void)logger_->log("configuration_error",{{"message",error}});
                }else if(logger_){(void)logger_->log("configuration_save");}
                recognizer_.clear();recognizer_.set_threshold(configuration_.global.recognition_threshold);
                for(const auto& gesture:configuration_.gestures.gestures)(void)recognizer_.add_gesture(gesture);
                router_=input::MouseInputRouter(
                    {configuration_.global.gesture_button,configuration_.global.movement_threshold,
                     []{const HWND window=::GetForegroundWindow();const UINT dpi=window!=nullptr ? ::GetDpiForWindow(window) : ::GetDpiForSystem();
                         return static_cast<double>(dpi)/96.0;}},
                    [this](const input::MouseInputEvent& event){
                        if(event.type==input::MouseEventType::pointer_moved&&
                           queue_.size_approx()>=queue_.usable_capacity()-1)return false;
                        const bool queued=queue_.try_push(event);if(queued)wake_.notify_one();return queued;});
                const auto& o=configuration_.global.overlay;const auto color=o.color;
                overlay_.configure({o.enabled,o.line_width,static_cast<BYTE>(o.opacity*255.0),
                    RGB((color>>16)&0xFF,(color>>8)&0xFF,color&0xFF)});
            }
            router_.set_enabled(configuration_.global.gestures_enabled);
            tray_.set_enabled(configuration_.global.gestures_enabled);
            worker_=std::jthread([this](std::stop_token stop){engine_loop(stop);});
            if(!hook_.start(&EngineHost::on_mouse_event,this)||!keyboard_hook_.start(&EngineHost::on_escape,this)){
                keyboard_hook_.stop();hook_.stop();router_.set_enabled(false);
                if(logger_)(void)logger_->log("hook_failure",{{"phase","settings_resume"}});
            }
        } catch (...) {
            ::MessageBoxW(nullptr,L"Settings could not be applied.",L"Strokes++",MB_OK|MB_ICONERROR);
            router_.set_enabled(configuration_.global.gestures_enabled);
            worker_=std::jthread([this](std::stop_token stop){engine_loop(stop);});
            if(!hook_.start(&EngineHost::on_mouse_event,this)||!keyboard_hook_.start(&EngineHost::on_escape,this)){
                keyboard_hook_.stop();hook_.stop();router_.set_enabled(false);
            }
        }
    }

    void engine_loop(std::stop_token stop) {
        engine::GestureEngine engine(
            recognizer_, configuration_.profiles.profiles, configuration_.profiles.global_actions,
            application_context_, modifier_state_, mouse_click_, keyboard_input_,
            input::GestureStateMachine({configuration_.global.movement_threshold,
                                        configuration_.global.minimum_point_distance,
                                        configuration_.global.maximum_points}),
            &overlay_);

        while (!stop.stop_requested()) {
            while (const auto event = queue_.try_pop()) {
                process_event(engine, *event);
            }
            std::unique_lock lock(wake_mutex_);
            wake_.wait(lock, stop, [this] { return !queue_.empty(); });
        }
        while (const auto event = queue_.try_pop()) {
            process_event(engine, *event);
        }
    }

    void process_event(engine::GestureEngine& engine, const input::MouseInputEvent& event) {
        const auto result = engine.process(event);
        if (event.type == input::MouseEventType::button_down && engine.session() && logger_) {
            const auto& app = engine.session()->application;
            (void)logger_->log("gesture_start", {{"process", app.executable_name},
                                                  {"window_title", app.window_title},
                                                  {"x", event.position.x}, {"y", event.position.y}});
        }
        if (result.gesture.capture_cancelled && logger_) (void)logger_->log("gesture_cancellation");
        if (result.gesture.recognition_requested && logger_) {
            (void)logger_->log("gesture_completion");
            if (result.recognition) {
                (void)logger_->log("recognition_result", {{"gesture_id", result.recognition->gesture_id},
                                                           {"gesture_name", result.recognition->gesture_name},
                                                           {"score", result.recognition->score}});
            } else {
                (void)logger_->log("recognition_result", {{"gesture_id", ""}, {"score", 0.0}});
            }
        }
        if (result.action_attempted && logger_) {
            (void)logger_->log(result.action_succeeded ? "action_execution" : "action_failure",
                               {{"succeeded", result.action_succeeded},
                                {"profile_id", result.profile_id},
                                {"source", result.action_source == actions::ActionSource::application_profile
                                               ? "application_profile" : "global"}});
        }
    }

    config::ConfigurationBundle configuration_{config::ConfigurationStore::defaults()};
    std::filesystem::path configuration_directory_;
    HINSTANCE instance_{};
    std::unique_ptr<logging::StructuredLogger> logger_;
    bool ready_{true};
    bool input_suspended_{};
    std::string startup_error_;
    gestures::Recognizer recognizer_;
    context::WindowsApplicationContextProvider application_context_;
    input::WindowsModifierStateProvider modifier_state_;
    input::WindowsMouseClick mouse_click_;
    actions::WindowsKeyboardInput keyboard_input_;
    input::InputQueue<input::MouseInputEvent, 4096> queue_;
    std::mutex wake_mutex_;
    std::condition_variable_any wake_;
    input::MouseInputRouter router_;
    input::WindowsMouseHook hook_;
    input::WindowsKeyboardHook keyboard_hook_;
    overlay::WindowsGestureOverlay overlay_;
    tray::WindowsTrayIcon tray_;
    std::jthread worker_;
};

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    (void)::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    EngineHost host;
    return host.run(instance) ? EXIT_SUCCESS : EXIT_FAILURE;
}
