#include "engine/Transformable.h"


void Transformable::setTransform(const glm::mat4& transform) {
    _localTransform = transform;
}

void Transformable::setTransform(glm::mat4&& transform) noexcept {
    _localTransform = std::move(transform);
}
