#pragma once

#include <condition_variable>
#include <deque>
#include <future>
#include <mutex>
#include <cstddef>
#include <stop_token>
#include <thread>

#include "actions/action_context.h"
#include "actions/action_definition.h"
#include "actions/action_result.h"
#include "actions/action_services.h"

namespace strokes::actions {

/// Executes short actions sequentially away from input and gesture-processing callbacks.
class ActionExecutor {
 public:
  explicit ActionExecutor(ActionServices services, std::size_t max_pending = 64);
  ~ActionExecutor();
  ActionExecutor(const ActionExecutor&) = delete;
  ActionExecutor& operator=(const ActionExecutor&) = delete;

  [[nodiscard]] std::future<ActionResult> submit(ActionDefinition definition,
                                                 ActionContext context);

 private:
  struct Work {
    ActionDefinition definition;
    ActionContext context;
    std::promise<ActionResult> completion;
  };

  void run(std::stop_token stop);
  static ActionResult cancelled();

  ActionServices services_;
  std::size_t max_pending_;
  std::mutex mutex_;
  std::condition_variable_any wake_;
  std::deque<Work> queue_;
  std::jthread worker_;
};

}  // namespace strokes::actions
