#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <optional>
#include <type_traits>

namespace strokes::input {

// Bounded SPSC queue. One hook thread produces and one engine thread consumes.
// No locks, allocations, or operating-system calls occur during push/pop.
template <typename T, std::size_t Capacity>
class InputQueue {
    static_assert(Capacity >= 2, "queue capacity must be at least two");
    static_assert(std::is_nothrow_copy_assignable_v<T>, "queue events must copy without throwing");

public:
    [[nodiscard]] bool try_push(const T& value) noexcept {
        const std::size_t write = write_index_.load(std::memory_order_relaxed);
        const std::size_t next = increment(write);
        if (next == read_index_.load(std::memory_order_acquire)) {
            return false;
        }
        storage_[write] = value;
        write_index_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        const std::size_t read = read_index_.load(std::memory_order_relaxed);
        if (read == write_index_.load(std::memory_order_acquire)) {
            return std::nullopt;
        }
        T value = storage_[read];
        read_index_.store(increment(read), std::memory_order_release);
        return value;
    }

    [[nodiscard]] bool empty() const noexcept {
        return read_index_.load(std::memory_order_acquire) ==
               write_index_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::size_t size_approx() const noexcept {
        const std::size_t write = write_index_.load(std::memory_order_acquire);
        const std::size_t read = read_index_.load(std::memory_order_acquire);
        return write >= read ? write - read : Capacity - read + write;
    }

    static constexpr std::size_t usable_capacity() noexcept { return Capacity - 1; }

private:
    static constexpr std::size_t increment(std::size_t index) noexcept {
        return (index + 1) % Capacity;
    }

    std::array<T, Capacity> storage_{};
    std::atomic<std::size_t> write_index_{0};
    std::atomic<std::size_t> read_index_{0};
};

}  // namespace strokes::input
