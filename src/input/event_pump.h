#pragma once

#include <optional>
#include <semaphore>

#include "input/input_queue.h"

namespace strokes::input {

/// Couples an SPSC queue with one counting-semaphore token per accepted item.
template <typename T, std::size_t Capacity>
class EventPump {
 public:
  [[nodiscard]] bool push(const T& value) noexcept {
    if (!queue_.try_push(value)) return false;
    available_.release();
    return true;
  }

  [[nodiscard]] std::optional<T> wait_pop() noexcept {
    available_.acquire();
    return queue_.try_pop();
  }

  [[nodiscard]] std::optional<T> try_pop() noexcept { return queue_.try_pop(); }
  [[nodiscard]] std::size_t size_approx() const noexcept { return queue_.size_approx(); }
  [[nodiscard]] constexpr std::size_t usable_capacity() const noexcept {
    return queue_.usable_capacity();
  }

  /// Wakes a waiting consumer during shutdown without enqueueing an item.
  void wake() noexcept { available_.release(); }

 private:
  InputQueue<T, Capacity> queue_;
  std::counting_semaphore<Capacity> available_{0};
};

}  // namespace strokes::input
