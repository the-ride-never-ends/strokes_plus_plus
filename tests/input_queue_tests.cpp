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
}

}  // namespace strokes::tests
