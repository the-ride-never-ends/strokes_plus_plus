#include "gestures/normalizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace strokes::gestures {
namespace {

constexpr double epsilon = 1e-9;

double distance(const Point& a, const Point& b) {
    return std::hypot(b.x - a.x, b.y - a.y);
}

bool finite(const Point& point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

Stroke remove_duplicates(const Stroke& input) {
    Stroke result;
    result.reserve(input.size());
    for (const auto& point : input) {
        if (!finite(point)) {
            return {};
        }
        if (result.empty() || distance(result.back(), point) > epsilon) {
            result.push_back(point);
        }
    }
    return result;
}

std::optional<Stroke> resample(const Stroke& input, std::size_t count) {
    double total_length = 0.0;
    for (std::size_t i = 1; i < input.size(); ++i) {
        total_length += distance(input[i - 1], input[i]);
    }
    if (total_length <= epsilon) {
        return std::nullopt;
    }

    Stroke output;
    output.reserve(count);
    output.push_back(input.front());

    const double interval = total_length / static_cast<double>(count - 1);
    double target_distance = interval;
    double traversed = 0.0;

    for (std::size_t i = 1; i < input.size() && output.size() < count - 1; ++i) {
        const Point start = input[i - 1];
        const Point end = input[i];
        const double segment_length = distance(start, end);
        if (segment_length <= epsilon) {
            continue;
        }

        while (traversed + segment_length >= target_distance && output.size() < count - 1) {
            const double ratio = (target_distance - traversed) / segment_length;
            output.push_back({start.x + ratio * (end.x - start.x),
                              start.y + ratio * (end.y - start.y)});
            target_distance += interval;
        }
        traversed += segment_length;
    }

    while (output.size() < count) {
        output.push_back(input.back());
    }
    return output;
}

void scale_and_translate(Stroke& stroke, double square_size) {
    double min_x = stroke.front().x;
    double max_x = min_x;
    double min_y = stroke.front().y;
    double max_y = min_y;
    Point centroid{};

    for (const auto& point : stroke) {
        min_x = std::min(min_x, point.x);
        max_x = std::max(max_x, point.x);
        min_y = std::min(min_y, point.y);
        max_y = std::max(max_y, point.y);
        centroid.x += point.x;
        centroid.y += point.y;
    }
    centroid.x /= static_cast<double>(stroke.size());
    centroid.y /= static_cast<double>(stroke.size());

    const double width = max_x - min_x;
    const double height = max_y - min_y;
    const double scale = square_size / std::max(width, height);
    for (auto& point : stroke) {
        point.x = (point.x - centroid.x) * scale;
        point.y = (point.y - centroid.y) * scale;
    }
}

}  // namespace

StrokeNormalizer::StrokeNormalizer(std::size_t point_count, double square_size)
    : point_count_(point_count), square_size_(square_size) {
    if (point_count < 2 || !std::isfinite(square_size) || square_size <= 0.0) {
        throw std::invalid_argument("normalizer parameters must be positive");
    }
}

std::optional<Stroke> StrokeNormalizer::normalize(const Stroke& stroke) const {
    if (stroke.size() < 2) {
        return std::nullopt;
    }
    auto filtered = remove_duplicates(stroke);
    if (filtered.size() < 2) {
        return std::nullopt;
    }
    auto normalized = resample(filtered, point_count_);
    if (!normalized) {
        return std::nullopt;
    }
    scale_and_translate(*normalized, square_size_);
    return normalized;
}

}  // namespace strokes::gestures
