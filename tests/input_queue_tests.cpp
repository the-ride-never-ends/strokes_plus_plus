#include <atomic>
#include <thread>

#include "input/input_queue.h"
#include "test_support.h"

namespace strokes::tests {

void run_input_queue_tests() {
  input::InputQueue<int, 4> queue;
  check(queue.empty(), "new input queue is empty");
  check(queue.usable_capacity() == 3, "input queue reports bounded usable capacity");
  check(queue.try_push(10), "first input event is queued");
  check(queue.try_push(20), "second input event is queued");
  check(queue.try_push(30), "queue fills to its bound");
  check(queue.size_approx() == 3, "queue exposes approximate producer pressure");
  check(!queue.try_push(40), "full input queue rejects an event without allocating");
  check(queue.try_pop() == 10, "input events retain FIFO order");
  check(queue.try_pop() == 20, "second input event follows first");
  check(queue.try_push(40), "queue wraps and accepts after a pop");
  check(queue.try_pop() == 30, "wrapped queue preserves existing event order");
  check(queue.try_pop() == 40, "wrapped queue preserves new event order");
  check(!queue.try_pop().has_value(), "empty queue has no input event");
  check(queue.size_approx() == 0, "drained queue reports no pressure");

  constexpr int item_count = 100000;
  input::InputQueue<int, 256> concurrent;
  std::atomic<bool> producer_finished{false};
  std::atomic<bool> order_preserved{true};
  std::jthread producer([&] {
    for (int value = 0; value < item_count; ++value) {
      while (!concurrent.try_push(value)) std::this_thread::yield();
    }
    producer_finished.store(true, std::memory_order_release);
  });
  int expected = 0;
  while (!producer_finished.load(std::memory_order_acquire) || !concurrent.empty()) {
    if (const auto value = concurrent.try_pop()) {
      if (*value != expected) order_preserved.store(false, std::memory_order_relaxed);
      ++expected;
    } else {
      std::this_thread::yield();
    }
  }
  producer.join();
  check(expected == item_count && order_preserved.load(std::memory_order_relaxed),
        "two-thread SPSC traffic preserves FIFO order without loss");
}

}  // namespace strokes::tests
