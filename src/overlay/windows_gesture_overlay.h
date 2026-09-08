#pragma once

#include "gestures/stroke.h"
#include "engine/gesture_feedback.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <mutex>

namespace strokes::overlay {

class WindowsGestureOverlay final : public engine::IGestureFeedback {
public:
    struct Options {
        bool enabled{true};
        int line_width{4};
        BYTE opacity{217};
        COLORREF color{RGB(0, 160, 255)};
    };

    WindowsGestureOverlay() = default;
    ~WindowsGestureOverlay();
    WindowsGestureOverlay(const WindowsGestureOverlay&) = delete;
    WindowsGestureOverlay& operator=(const WindowsGestureOverlay&) = delete;

    [[nodiscard]] bool create(HINSTANCE instance, Options options = {});
    void destroy() noexcept;
    void show(const gestures::Stroke& points) override;
    void update(const gestures::Stroke& points) override;
    void hide() noexcept override;
    void configure(Options options) noexcept;

private:
    static LRESULT CALLBACK window_proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle_message(UINT, WPARAM, LPARAM);
    void paint() noexcept;

    HINSTANCE instance_{};
    HWND window_{};
    Options options_;
    std::mutex points_mutex_;
    gestures::Stroke points_;
};

}  // namespace strokes::overlay
