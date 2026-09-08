#pragma once
#include "config/configuration_store.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

namespace strokes::ui {
class WindowsSettingsWindow final {
public:
    [[nodiscard]] bool show(HINSTANCE instance,config::ConfigurationBundle& configuration);
private:
    static LRESULT CALLBACK window_proc(HWND,UINT,WPARAM,LPARAM);
    LRESULT handle_message(UINT,WPARAM,LPARAM);
    void create_controls();
    void load_values();
    void refresh_gestures();
    [[nodiscard]] int selected_gesture() const noexcept;
    void add_gesture();
    void rename_gesture();
    void delete_gesture();
    void train_gesture();
    void toggle_gesture();
    void add_profile();
    void update_profile();
    void delete_profile();
    void assign_profile_action();
    void assign_global_action();
    void load_selected_gesture();
    void load_selected_profile();
    [[nodiscard]] int selected_profile() const noexcept;
    [[nodiscard]] bool save_values();
    HINSTANCE instance_{};
    HWND window_{};
    config::ConfigurationBundle* configuration_{};
    config::ConfigurationBundle* destination_{};
    config::ConfigurationBundle working_;
    bool accepted_{};
    bool finished_{};
};
}  // namespace strokes::ui
