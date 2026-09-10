#include <algorithm>
#include <span>
#include <vector>

#include "actions/keyboard_action.h"
#include "actions/keyboard_shortcut.h"
#include "test_support.h"

namespace strokes::tests {
namespace {

using actions::IKeyboardInput;
using actions::KeyboardActionExecutor;
using actions::KeyEvent;
using actions::VirtualKey;

class FakeKeyboardInput final : public IKeyboardInput {
 public:
  std::vector<VirtualKey> held_keys;
  std::vector<KeyEvent> sent_events;
  std::vector<std::vector<KeyEvent>> batches;
  bool send_result{true};
  bool release_after_send{};

  bool is_key_down(VirtualKey key) const override {
    return std::ranges::find(held_keys, key) != held_keys.end();
  }

  bool send(std::span<const KeyEvent> events) override {
    sent_events.assign(events.begin(), events.end());
    batches.emplace_back(events.begin(), events.end());
    if (release_after_send) held_keys.clear();
    return send_result;
  }
};

void parsing_tests() {
  auto shortcut = actions::parse_shortcut("ctrl + shift + tab");
  check(shortcut && shortcut->modifiers == std::vector{VirtualKey::control, VirtualKey::shift},
        "shortcut modifiers parse case-insensitively");
  check(shortcut && shortcut->key == VirtualKey::tab, "named shortcut key parses");
  check(actions::parse_shortcut("ALT+LEFT").has_value(), "arrow shortcut parses");
  check(actions::parse_shortcut("WIN+D").has_value(), "Windows-key shortcut parses");
  check(actions::parse_shortcut("CTRL+F24").has_value(), "function key parses");
  check(!actions::parse_shortcut("CTRL+").has_value(), "empty token is rejected");
  check(!actions::parse_shortcut("CTRL+CTRL+W").has_value(), "duplicate modifier is rejected");
  check(!actions::parse_shortcut("CTRL+W+T").has_value(), "multiple primary keys are rejected");
  check(!actions::parse_shortcut("CTRL+NOT_A_KEY").has_value(), "unsupported key is rejected");
  check(!actions::parse_shortcut("CTRL+SHIFT").has_value(), "modifier-only shortcut is rejected");
}

void ordering_tests() {
  const auto shortcut = actions::parse_shortcut("CTRL+SHIFT+TAB");
  FakeKeyboardInput input;
  check(shortcut && KeyboardActionExecutor::execute(*shortcut, input), "valid shortcut executes");
  const std::vector expected{
      KeyEvent{VirtualKey::control, true}, KeyEvent{VirtualKey::shift, true},
      KeyEvent{VirtualKey::tab, true},     KeyEvent{VirtualKey::tab, false},
      KeyEvent{VirtualKey::shift, false},  KeyEvent{VirtualKey::control, false}};
  check(input.sent_events == expected, "modifiers press before key and release in reverse order");
}

void held_modifier_and_failure_tests() {
  const auto shortcut = actions::parse_shortcut("CTRL+SHIFT+W");
  FakeKeyboardInput input;
  input.held_keys.push_back(VirtualKey::left_control);
  check(shortcut && KeyboardActionExecutor::execute(*shortcut, input),
        "held-modifier shortcut executes");
  check(std::ranges::find(input.sent_events, KeyEvent{VirtualKey::control, true}) ==
            input.sent_events.end(),
        "physically held modifier is not injected");
  check(std::ranges::find(input.sent_events, KeyEvent{VirtualKey::control, false}) ==
            input.sent_events.end(),
        "physically held modifier is not released");

  input.send_result = false;
  check(shortcut && !KeyboardActionExecutor::execute(*shortcut, input),
        "input injection failure is reported");
  check(input.batches.size() == 3 &&
            input.batches.back() == std::vector{KeyEvent{VirtualKey::shift, false}},
        "failed input injection makes a best-effort release of injected modifiers");

  const auto control_w = actions::parse_shortcut("CTRL+W");
  FakeKeyboardInput shifted;
  shifted.held_keys.push_back(VirtualKey::right_shift);
  check(control_w && KeyboardActionExecutor::execute(*control_w, shifted),
        "shortcut executes while an unrelated modifier is physically held");
  check(shifted.batches.size() == 2 &&
            shifted.batches.front().front() == KeyEvent{VirtualKey::right_shift, false} &&
            shifted.batches.back().back() == KeyEvent{VirtualKey::right_shift, true},
        "unrelated held modifier is neutralized and restored around the shortcut");

  FakeKeyboardInput released_shift;
  released_shift.held_keys.push_back(VirtualKey::right_shift);
  released_shift.release_after_send = true;
  check(control_w && KeyboardActionExecutor::execute(*control_w, released_shift) &&
            released_shift.batches.size() == 1,
        "a modifier physically released during injection is not re-pressed");

  FakeKeyboardInput right_windows;
  right_windows.held_keys.push_back(VirtualKey::right_windows);
  check(control_w && KeyboardActionExecutor::execute(*control_w, right_windows) &&
            right_windows.batches.front().front() == KeyEvent{VirtualKey::right_windows, false},
        "the right Windows key is neutralized independently");
}

}  // namespace

void run_keyboard_action_tests() {
  parsing_tests();
  ordering_tests();
  held_modifier_and_failure_tests();
}

}  // namespace strokes::tests
