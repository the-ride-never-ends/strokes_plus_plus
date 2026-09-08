#pragma once

#include "gestures/stroke.h"

namespace strokes::engine {

class IGestureFeedback {
public:
    virtual ~IGestureFeedback() = default;
    virtual void show(const gestures::Stroke& points) = 0;
    virtual void update(const gestures::Stroke& points) = 0;
    virtual void hide() noexcept = 0;
};

}  // namespace strokes::engine
