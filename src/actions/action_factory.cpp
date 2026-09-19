#include "actions/action_factory.h"

#include <functional>
#include <utility>

#include "actions/symbolic_resolver.h"

namespace strokes::actions {
namespace {

class FunctionalAction final : public IAction {
 public:
  explicit FunctionalAction(std::function<ActionResult(const ActionContext&)> execute)
      : execute_(std::move(execute)) {}

  ActionResult execute(const ActionContext& context) override { return execute_(context); }

 private:
  std::function<ActionResult(const ActionContext&)> execute_;
};

ActionResult unavailable(std::string service) {
  return ActionResult::failed(ActionError::unsupported_operation, "service_unavailable",
                              std::move(service) + " service is unavailable.");
}

ActionResult missing_target(std::string target) {
  return ActionResult::failed(ActionError::invalid_runtime_target, "target_unavailable",
                              std::move(target) + " is unavailable.");
}

template <typename Callable>
std::unique_ptr<IAction> action(Callable callable) {
  return std::make_unique<FunctionalAction>(std::move(callable));
}

}  // namespace

ActionFactoryResult ActionFactory::create(const ActionDefinition& definition,
                                          ActionServices services) {
  const auto validation = validate(definition);
  if (!validation.valid) return {nullptr, validation};

  switch (definition.type) {
    case ActionType::keyboard_shortcut: {
      const auto parameters = std::get<KeyboardParameters>(definition.parameters);
      return {action([parameters, service = services.keyboard](const ActionContext&) {
                return service ? service->send_shortcut(parameters.shortcut)
                               : unavailable("Keyboard");
              }),
              validation};
    }
    case ActionType::process: {
      const auto parameters = std::get<ProcessParameters>(definition.parameters);
      return {action([parameters, service = services.process](const ActionContext&) {
                return service ? service->launch(parameters) : unavailable("Process");
              }),
              validation};
    }
    case ActionType::url: {
      const auto parameters = std::get<UrlParameters>(definition.parameters);
      return {action([parameters, service = services.shell](const ActionContext&) {
                return service ? service->open_uri(parameters.uri) : unavailable("Shell");
              }),
              validation};
    }
    case ActionType::mouse: {
      const auto parameters = std::get<MouseParameters>(definition.parameters);
      return {action([parameters, service = services.mouse](const ActionContext& context) {
                if (!service) return unavailable("Mouse");
                const auto position = parameters.position.target == PositionTarget::current_cursor
                                          ? service->current_position()
                                          : resolve_position(parameters.position, context);
                if (!position) return missing_target("Mouse position");
                return service->perform(parameters.operation, parameters.button, *position);
              }),
              validation};
    }
    case ActionType::window: {
      const auto parameters = std::get<WindowParameters>(definition.parameters);
      return {action([parameters, service = services.window](const ActionContext& context) {
                if (!service) return unavailable("Window");
                const auto window = resolve_window(parameters.target, context);
                if (!window || *window == 0) return missing_target("Window target");
                return service->perform(parameters.operation, *window, parameters);
              }),
              validation};
    }
    case ActionType::media: {
      const auto parameters = std::get<MediaParameters>(definition.parameters);
      return {action([parameters, service = services.media](const ActionContext&) {
                return service ? service->perform(parameters.operation) : unavailable("Media");
              }),
              validation};
    }
    case ActionType::volume: {
      const auto parameters = std::get<VolumeParameters>(definition.parameters);
      return {action([parameters, service = services.audio](const ActionContext&) {
                return service ? service->perform(parameters.operation, parameters.amount)
                               : unavailable("Audio");
              }),
              validation};
    }
    case ActionType::virtual_desktop: {
      const auto parameters = std::get<VirtualDesktopParameters>(definition.parameters);
      return {action([parameters, service = services.virtual_desktop](const ActionContext&) {
                return service ? service->perform(parameters.operation)
                               : unavailable("Virtual desktop");
              }),
              validation};
    }
    case ActionType::lua: {
      const auto parameters = std::get<LuaParameters>(definition.parameters);
      return {action([parameters, service = services.lua](const ActionContext& context) {
                return service ? service->execute(parameters.script, context)
                               : unavailable("Lua");
              }),
              validation};
    }
  }
  return {nullptr, {false, "unknown_action_type", "The action type is unsupported."}};
}

}  // namespace strokes::actions
