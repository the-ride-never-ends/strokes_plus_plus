#include <atomic>
#include <thread>

#include "input/input_queue.h"
#include "input/event_pump.h"
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

  input::EventPump<int, 256> pump;
  std::atomic<bool> pump_ordered{true};
  std::jthread pump_consumer([&] {
    for (int value = 0; value < item_count; ++value) {
      const auto received = pump.wait_pop();
      if (!received || *received != value) pump_ordered.store(false, std::memory_order_relaxed);
    }
  });
  for (int value = 0; value < item_count; ++value) {
    while (!pump.push(value)) std::this_thread::yield();
  }
  pump_consumer.join();
  check(pump_ordered.load(std::memory_order_relaxed),
        "event pump wakes for every accepted item without stranding work");

  input::EventPump<int, 8> drained;
  check(drained.push(1) && drained.push(2), "event pump accepts items for a drain");
  drained.wake();
  check(drained.take() == 1 && drained.take() == 2, "draining preserves item order");
  check(!drained.take().has_value(), "draining reports an empty queue");
  check(drained.push(3) && drained.wait_pop() == 3,
        "no surplus wake token survives a drain to strand the next item");
}

}  // namespace strokes::tests
