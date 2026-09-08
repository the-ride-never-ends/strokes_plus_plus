#pragma once
#include "gestures/recognizer.h"
#include <string>
#include <vector>

namespace strokes::gestures {
class GestureRepository {
public:
    explicit GestureRepository(std::vector<GestureDefinition>& gestures) : gestures_(gestures) {}
    [[nodiscard]] bool create(std::string id,std::string name);
    [[nodiscard]] bool rename(const std::string& id,std::string name);
    [[nodiscard]] bool set_enabled(const std::string& id,bool enabled);
    [[nodiscard]] bool erase(const std::string& id);
    [[nodiscard]] bool add_template(const std::string& gesture_id,GestureTemplate value);
    [[nodiscard]] bool remove_template(const std::string& gesture_id,const std::string& template_id);
    [[nodiscard]] GestureDefinition* find(const std::string& id) noexcept;
private:
    [[nodiscard]] bool unique_id(const std::string& id) const;
    [[nodiscard]] bool unique_name(const std::string& name,const std::string* except_id=nullptr) const;
    std::vector<GestureDefinition>& gestures_;
};
}  // namespace strokes::gestures
