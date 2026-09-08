#pragma once

#include "gestures/normalizer.h"

#include <optional>
#include <string>
#include <vector>

namespace strokes::gestures {

struct GestureTemplate {
    std::string id;
    Stroke points;
};

struct GestureDefinition {
    std::string id;
    std::string name;
    bool enabled{true};
    std::vector<GestureTemplate> templates;
};

struct RecognitionResult {
    std::string gesture_id;
    std::string gesture_name;
    double score{};
};

class Recognizer {
public:
    explicit Recognizer(
        double threshold = 0.80,
        StrokeNormalizer normalizer = StrokeNormalizer());

    [[nodiscard]] bool add_gesture(GestureDefinition gesture);
    void clear() noexcept;
    void set_threshold(double threshold);
    [[nodiscard]] double threshold() const noexcept { return threshold_; }
    [[nodiscard]] std::size_t gesture_count() const noexcept { return gestures_.size(); }
    [[nodiscard]] std::optional<RecognitionResult> recognize(const Stroke& stroke) const;

private:
    double threshold_;
    StrokeNormalizer normalizer_;
    std::vector<GestureDefinition> gestures_;
};

}  // namespace strokes::gestures
