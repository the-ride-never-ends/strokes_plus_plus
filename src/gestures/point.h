#pragma once

namespace strokes::gestures {

struct Point {
    double x{};
    double y{};

    friend constexpr bool operator==(const Point&, const Point&) = default;
};

}  // namespace strokes::gestures
