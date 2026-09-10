#pragma once

#include <optional>
#include <semaphore>

#include "input/input_queue.h"

namespace strokes::input {

/// Couples an SPSC queue with one counting-semaphore token per accepted item.
///
/// Every item added by `push` releases exactly one token, and every token is
/// consumed by exactly one `wait_pop` or `take`, so the token count never drifts
/// away from the queue depth across restarts of the consumer.
template <typename T, std::size_t Capacity>
class EventPump {
 public:
  [[nodiscard]] bool push(const T& value) noexcept {
    if (!queue_.try_push(value)) return false;
    available_.release();
    return true;
  }

  /// Blocks for one token and returns the item it stands for, if any.
  [[nodiscard]] std::optional<T> wait_pop() noexcept {
    available_.acquire();
    return queue_.try_pop();
  }

  /// Consumes one token and its item without blocking.
  ///
  /// Returns:
  ///   The next item, or nothing once no tokens remain. Because tokens are
  ///   never fewer than queued items, an empty result means the queue is drained.
  [[nodiscard]] std::optional<T> take() noexcept {
    if (!available_.try_acquire()) return std::nullopt;
    return queue_.try_pop();
  }

  [[nodiscard]] std::size_t size_approx() const noexcept { return queue_.size_approx(); }
  [[nodiscard]] constexpr std::size_t usable_capacity() const noexcept {
    return queue_.usable_capacity();
  }

  /// Wakes a waiting consumer during shutdown without enqueueing an item.
  ///
  /// The token is consumed either by the parked `wait_pop` it releases or by the
  /// consumer's final `take` drain, so it does not outlive the stop it signals.
  void wake() noexcept { available_.release(); }

 private:
  InputQueue<T, Capacity> queue_;
  std::counting_semaphore<Capacity> available_{0};
};

}  // namespace strokes::input
