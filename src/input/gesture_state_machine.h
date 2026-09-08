#pragma once

#include "input/gesture_session.h"

#include <cstddef>
#include <optional>

namespace strokes::input {

struct GestureUpdate {
    bool suppress_input{};
    bool emulate_click{};
    bool capture_started{};
    bool recognition_requested{};
    bool capture_cancelled{};
};

class GestureStateMachine {
public:
    struct Options {
        double movement_threshold{8.0};
        double minimum_point_distance{2.0};
        std::size_t maximum_points{4096};
    };

    GestureStateMachine();
    explicit GestureStateMachine(Options options);

    [[nodiscard]] GestureUpdate button_down(gestures::Point position);
    [[nodiscard]] GestureUpdate button_down(GestureStart start);
    [[nodiscard]] GestureUpdate pointer_moved(gestures::Point position);
    [[nodiscard]] GestureUpdate button_up(gestures::Point position);
    [[nodiscard]] GestureUpdate cancel();
    [[nodiscard]] GestureUpdate cancellation_finished();
    [[nodiscard]] GestureUpdate set_enabled(bool enabled);
    [[nodiscard]] GestureUpdate recognition_finished(bool has_action);
    [[nodiscard]] GestureUpdate execution_finished();

    [[nodiscard]] GestureState state() const noexcept { return state_; }
    [[nodiscard]] bool enabled() const noexcept { return enabled_; }
    [[nodiscard]] const gestures::Stroke& captured_points() const noexcept;
    [[nodiscard]] const std::optional<GestureSession>& session() const noexcept { return session_; }

private:
    void reset() noexcept;
    void transition_to(GestureState state) noexcept;
    void add_point(gestures::Point position);

    Options options_;
    GestureState state_{GestureState::idle};
    bool enabled_{true};
    std::optional<GestureSession> session_;
    gestures::Stroke empty_stroke_;
};

}  // namespace strokes::input
