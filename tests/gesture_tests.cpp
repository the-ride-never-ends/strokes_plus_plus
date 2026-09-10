#include <cmath>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "gestures/normalizer.h"
#include "gestures/recognizer.h"
#include "test_support.h"

using namespace strokes::gestures;

namespace {

using strokes::tests::check;

Stroke line(double x1, double y1, double x2, double y2, int count = 20) {
  Stroke result;
  for (int i = 0; i < count; ++i) {
    const double t = static_cast<double>(i) / static_cast<double>(count - 1);
    result.push_back({x1 + (x2 - x1) * t, y1 + (y2 - y1) * t});
  }
  return result;
}

Stroke caret(double scale = 1.0, double offset_x = 0.0, double offset_y = 0.0) {
  Stroke result;
  for (int i = 0; i <= 10; ++i) {
    result.push_back({offset_x + scale * i, offset_y + scale * (10 - i)});
  }
  for (int i = 1; i <= 10; ++i) {
    result.push_back({offset_x + scale * (10 + i), offset_y + scale * i});
  }
  return result;
}

GestureDefinition gesture(std::string id, std::string name, Stroke points, bool enabled = true) {
  return {std::move(id), std::move(name), enabled, {{"template-1", std::move(points)}}};
}

void normalization_tests() {
  StrokeNormalizer normalizer;
  const auto normalized = normalizer.normalize(caret());
  check(normalized.has_value(), "valid stroke normalizes");
  check(normalized && normalized->size() == 64, "normalizer resamples to 64 points");
  check(!normalizer.normalize({}).has_value(), "empty stroke is rejected");
  check(!normalizer.normalize({{1, 1}}).has_value(), "one-point stroke is rejected");
  check(!normalizer.normalize({{1, 1}, {1, 1}, {1, 1}}).has_value(),
        "duplicate-only stroke is rejected");
  check(!normalizer.normalize({{0, 0}, {std::numeric_limits<double>::infinity(), 1}}).has_value(),
        "non-finite coordinates are rejected");
  const auto extreme = normalizer.normalize({{-1.0e12, 1.0e12}, {0, 0}, {1.0e12, -1.0e12}});
  check(extreme && extreme->size() == 64, "extreme finite coordinates normalize safely");
}

void recognition_tests() {
  Recognizer recognizer(0.80);
  check(recognizer.add_gesture(gesture("caret", "Caret", caret())), "gesture is added");

  auto result = recognizer.recognize(caret());
  check(result && result->gesture_id == "caret", "identical stroke is recognized");
  check(result && std::abs(result->score - 1.0) < 1e-9, "identical stroke scores one");

  result = recognizer.recognize(caret(3.5, -420.0, 900.0));
  check(result && result->gesture_id == "caret", "translated and scaled stroke is recognized");

  auto noisy = caret();
  for (std::size_t i = 1; i + 1 < noisy.size(); ++i) {
    noisy[i].x += (i % 2 == 0 ? 0.35 : -0.35);
    noisy[i].y += (i % 3 == 0 ? 0.25 : -0.25);
  }
  check(recognizer.recognize(noisy).has_value(), "moderately noisy stroke is recognized");
  check(!recognizer.recognize(line(20, 0, 0, 0)).has_value(), "incorrect stroke is rejected");
  check(!recognizer.recognize({}).has_value(), "empty candidate returns no match");
}

void orientation_and_library_tests() {
  Recognizer recognizer(0.80);
  check(recognizer.add_gesture(gesture("right", "Right", line(0, 0, 20, 0))),
        "right gesture is added");
  check(recognizer.add_gesture(gesture("up", "Up", line(0, 20, 0, 0))), "up gesture is added");
  const auto oriented = recognizer.recognize(line(10, 3, 110, 3));
  check(oriented && oriented->gesture_id == "right", "orientation is preserved");

  Recognizer disabled_only(0.0);
  check(disabled_only.add_gesture(gesture("off", "Off", caret(), false)),
        "disabled gesture is stored");
  check(!disabled_only.recognize(caret()).has_value(), "disabled gesture is excluded");

  GestureDefinition multi{
      "multi", "Multi", true, {{"caret", caret()}, {"right", line(0, 0, 20, 0)}}};
  Recognizer multiple(0.95);
  check(multiple.add_gesture(std::move(multi)), "multiple templates are added");
  const auto matched = multiple.recognize(line(0, 0, 80, 0));
  check(matched && matched->gesture_id == "multi", "any gesture template can match");
  GestureDefinition partially_valid{
      "partial", "Partial", true, {{"bad", {{4, 4}, {4, 4}}}, {"good", line(0, 0, 20, 0)}}};
  check(multiple.add_gesture(std::move(partially_valid)),
        "one degenerate template does not reject a gesture with a valid sibling");
  check(!multiple.add_gesture(gesture("multi", "Duplicate", caret())),
        "duplicate gesture id is rejected");
}

void threshold_tests() {
  bool threw = false;
  try {
    Recognizer invalid(1.1);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  check(threw, "invalid threshold is rejected");

  Recognizer permissive(0.0);
  check(permissive.add_gesture(gesture("caret", "Caret", caret())),
        "threshold test gesture is added");
  const auto result = permissive.recognize(line(0, 0, 20, 0));
  check(result.has_value(), "zero threshold accepts the best valid candidate");
  permissive.set_threshold(1.0);
  check(!permissive.recognize(line(0, 0, 20, 0)).has_value(),
        "higher threshold rejects weak match");
}

}  // namespace

int main() {
  normalization_tests();
  recognition_tests();
  orientation_and_library_tests();
  threshold_tests();
  strokes::tests::run_state_machine_tests();
  strokes::tests::run_profile_tests();
  strokes::tests::run_keyboard_action_tests();
  strokes::tests::run_input_queue_tests();
  strokes::tests::run_mouse_input_router_tests();
  strokes::tests::run_gesture_engine_tests();
  strokes::tests::run_json_tests();
  strokes::tests::run_configuration_tests();
  strokes::tests::run_configuration_store_tests();
  strokes::tests::run_logging_tests();
  strokes::tests::run_repository_tests();

  if (strokes::tests::failures != 0) {
    std::cerr << strokes::tests::failures << " test(s) failed\n";
    return EXIT_FAILURE;
  }
  std::cout << "All gesture core tests passed\n";
  return EXIT_SUCCESS;
}
