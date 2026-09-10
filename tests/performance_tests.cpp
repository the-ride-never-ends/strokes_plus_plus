#include <chrono>
#include <string>
#include <utility>

#include "gestures/recognizer.h"
#include "context/profile_matcher.h"
#include "input/mouse_input_router.h"
#include "test_support.h"

namespace strokes::tests {
namespace {
void recognition_performance() {
  gestures::Stroke stroke;
  stroke.reserve(128);
  for (int i = 0; i < 128; ++i)
    stroke.push_back({static_cast<double>(i), static_cast<double>((i * i) % 97)});
  gestures::Recognizer recognizer(.75);
  for (int sample = 0; sample < 32; ++sample) {
    auto varied = stroke;
    for (auto& point : varied) point.y += sample * .05;
    (void)recognizer.add_gesture(
        {"gesture-" + std::to_string(sample), "Gesture", true, {{"sample", std::move(varied)}}});
  }
  constexpr int iterations = 1000;
  const auto start = std::chrono::steady_clock::now();
  bool recognized = true;
  for (int i = 0; i < iterations; ++i)
    recognized = recognizer.recognize(stroke).has_value() && recognized;
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto average = std::chrono::duration<double, std::milli>(elapsed).count() / iterations;
  check(recognized, "benchmark stroke remains recognized");
  check(average < 10.0, "typical recognition remains below 10 ms on average");
}

void input_routing_performance() {
  std::size_t delivered = 0;
  std::size_t scale_reads = 0;
  input::MouseInputRouter router({input::ActivationButton::right, 8.0,
                                  [&] {
                                    ++scale_reads;
                                    return 1.5;
                                  }},
                                 [&](const input::MouseInputEvent&) {
                                   ++delivered;
                                   return true;
                                 });
  constexpr int iterations = 100000;
  const auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < iterations; ++i) {
    (void)router.route(
        {input::MouseEventType::button_down, {0, 0}, input::ActivationButton::right, {}});
    (void)router.route(
        {input::MouseEventType::pointer_moved, {20.0, 0}, input::ActivationButton::right, {}});
    (void)router.route(
        {input::MouseEventType::button_up, {20.0, 0}, input::ActivationButton::right, {}});
  }
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto average = std::chrono::duration<double, std::milli>(elapsed).count() / iterations;
  check(delivered == static_cast<std::size_t>(iterations * 3) && scale_reads == iterations,
        "hook-path benchmark includes threshold scaling and the full interaction");
  check(average < 0.1,
        "synchronous hook-routing hot path remains substantially below 1 ms per event");
}

void profile_matching_performance() {
  context::ApplicationProfile profile{"browser", "Browser", true, {}, {}};
  for (int index = 0; index < 16; ++index) {
    profile.criteria.push_back({context::ApplicationProperty::window_title,
                                context::MatchMode::contains, "example"});
  }
  context::ApplicationContext application;
  application.window_title = "Example document title";
  context::ProfileMatcher matcher;
  constexpr int iterations = 1000;
  const auto start = std::chrono::steady_clock::now();
  bool matched = true;
  for (int index = 0; index < iterations; ++index) matched = matcher.matches(profile, application);
  const auto elapsed = std::chrono::steady_clock::now() - start;
  const auto average = std::chrono::duration<double, std::milli>(elapsed).count() / iterations;
  check(matched && average < 10.0, "profile resolution remains within the recognition budget");
}
}  // namespace

void run_performance_tests() {
  recognition_performance();
  input_routing_performance();
  profile_matching_performance();
}
}  // namespace strokes::tests
