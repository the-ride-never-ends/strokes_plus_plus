#include "gestures/recognizer.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace strokes::gestures {
namespace {

double path_distance(const Stroke& lhs, const Stroke& rhs) {
  double total = 0.0;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    total += std::hypot(lhs[i].x - rhs[i].x, lhs[i].y - rhs[i].y);
  }
  return total / static_cast<double>(lhs.size());
}

}  // namespace

Recognizer::Recognizer(double threshold, StrokeNormalizer normalizer)
    : threshold_(threshold), normalizer_(std::move(normalizer)) {
  set_threshold(threshold);
}

void Recognizer::set_threshold(double threshold) {
  if (!std::isfinite(threshold) || threshold < 0.0 || threshold > 1.0) {
    throw std::invalid_argument("recognition threshold must be between 0 and 1");
  }
  threshold_ = threshold;
}

bool Recognizer::add_gesture(GestureDefinition gesture) {
  if (gesture.id.empty() || gesture.name.empty() || gesture.templates.empty() ||
      std::ranges::any_of(gestures_,
                          [&](const auto& existing) { return existing.id == gesture.id; })) {
    return false;
  }

  std::erase_if(gesture.templates, [&](auto& gesture_template) {
    auto normalized = normalizer_.normalize(gesture_template.points);
    if (!normalized) return true;
    gesture_template.points = std::move(*normalized);
    return false;
  });
  if (gesture.templates.empty()) return false;
  gestures_.push_back(std::move(gesture));
  return true;
}

void Recognizer::clear() noexcept { gestures_.clear(); }

std::optional<RecognitionResult> Recognizer::recognize(const Stroke& stroke) const {
  const auto candidate = normalizer_.normalize(stroke);
  if (!candidate) {
    return std::nullopt;
  }

  const double half_diagonal = 0.5 * std::sqrt(2.0) * normalizer_.square_size();
  double best_score = -std::numeric_limits<double>::infinity();
  const GestureDefinition* best_gesture = nullptr;

  for (const auto& gesture : gestures_) {
    if (!gesture.enabled) {
      continue;
    }
    for (const auto& gesture_template : gesture.templates) {
      const double score = std::clamp(
          1.0 - path_distance(*candidate, gesture_template.points) / half_diagonal, 0.0, 1.0);
      if (score > best_score) {
        best_score = score;
        best_gesture = &gesture;
      }
    }
  }

  if (best_gesture == nullptr || best_score < threshold_) {
    return std::nullopt;
  }
  return RecognitionResult{best_gesture->id, best_gesture->name, best_score};
}

}  // namespace strokes::gestures
