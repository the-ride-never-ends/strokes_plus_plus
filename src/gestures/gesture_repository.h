#pragma once
#include <string>
#include <vector>

#include "gestures/recognizer.h"

namespace strokes::gestures {
/// Applies validated identity-safe edits to gesture definitions and templates.
class GestureRepository {
 public:
  explicit GestureRepository(std::vector<GestureDefinition>& gestures) : gestures_(gestures) {}
  [[nodiscard]] bool create(std::string id, std::string name);
  [[nodiscard]] bool rename(const std::string& id, std::string name);
  [[nodiscard]] bool set_enabled(const std::string& id, bool enabled);
  [[nodiscard]] bool erase(const std::string& id);
  [[nodiscard]] bool add_template(const std::string& gesture_id, GestureTemplate value);
  [[nodiscard]] bool remove_template(const std::string& gesture_id, const std::string& template_id);
  [[nodiscard]] GestureDefinition* find(const std::string& id) noexcept;

 private:
  std::vector<GestureDefinition>& gestures_;
};
}  // namespace strokes::gestures
