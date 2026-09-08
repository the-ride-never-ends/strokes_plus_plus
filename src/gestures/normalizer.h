#pragma once

#include "gestures/stroke.h"

#include <cstddef>
#include <optional>

namespace strokes::gestures {

class StrokeNormalizer {
public:
    static constexpr std::size_t default_point_count = 64;
    static constexpr double default_square_size = 250.0;

    explicit StrokeNormalizer(
        std::size_t point_count = default_point_count,
        double square_size = default_square_size);

    [[nodiscard]] std::optional<Stroke> normalize(const Stroke& stroke) const;
    [[nodiscard]] std::size_t point_count() const noexcept { return point_count_; }
    [[nodiscard]] double square_size() const noexcept { return square_size_; }

private:
    std::size_t point_count_;
    double square_size_;
};

}  // namespace strokes::gestures
