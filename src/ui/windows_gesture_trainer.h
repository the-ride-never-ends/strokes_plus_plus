#pragma once
#include "gestures/stroke.h"
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <optional>

namespace strokes::ui {
class WindowsGestureTrainer final {
public:
    [[nodiscard]] std::optional<gestures::Stroke> capture(HINSTANCE instance);
private:
    static LRESULT CALLBACK window_proc(HWND,UINT,WPARAM,LPARAM);
    LRESULT handle_message(UINT,WPARAM,LPARAM);
    void paint() noexcept;
    HWND window_{};
    HINSTANCE instance_{};
    gestures::Stroke points_;
    bool drawing_{};
    bool finished_{};
    bool accepted_{};
};
}  // namespace strokes::ui
