#include "actions/action_executor.h"

#include <exception>
#include <utility>

#include "actions/action_factory.h"

namespace strokes::actions {

ActionExecutor::ActionExecutor(ActionServices services)
    : services_(services), worker_([this](std::stop_token stop) { run(stop); }) {}

ActionExecutor::~ActionExecutor() {
  worker_.request_stop();
  wake_.notify_all();
  worker_.join();
}

std::future<ActionResult> ActionExecutor::submit(ActionDefinition definition,
                                                 ActionContext context) {
  Work work{std::move(definition), std::move(context), {}};
  auto result = work.completion.get_future();
  {
    std::lock_guard lock(mutex_);
    queue_.push_back(std::move(work));
  }
  wake_.notify_one();
  return result;
}

ActionResult ActionExecutor::cancelled() {
  return ActionResult::failed(ActionError::unsupported_operation, "executor_stopped",
                              "Action execution stopped before the action could run.");
}

void ActionExecutor::run(std::stop_token stop) {
  for (;;) {
    Work work;
    {
      std::unique_lock lock(mutex_);
      wake_.wait(lock, stop, [this] { return !queue_.empty(); });
      if (stop.stop_requested()) {
        while (!queue_.empty()) {
          queue_.front().completion.set_value(cancelled());
          queue_.pop_front();
        }
        return;
      }
      work = std::move(queue_.front());
      queue_.pop_front();
    }

    ActionResult result;
    try {
      auto created = ActionFactory::create(work.definition, services_);
      if (!created) {
        result = ActionResult::failed(ActionError::invalid_definition, created.validation.code,
                                      created.validation.message);
      } else {
        result = created.action->execute(work.context);
      }
    } catch (const std::exception& error) {
      result = ActionResult::failed(ActionError::execution_exception, "action_exception",
                                    error.what());
    } catch (...) {
      result = ActionResult::failed(ActionError::execution_exception, "action_exception",
                                    "Unknown action exception.");
    }
    work.completion.set_value(std::move(result));
  }
}

}  // namespace strokes::actions
