#pragma once

#include "engine/Composable.h"

#include <glm/glm.hpp>


class Transformable : public Composable {
public:
    void setTransform(const glm::mat4& transform);
    void setTransform(glm::mat4&& transform) noexcept;

    Transformable(const Transformable&) = delete;
    Transformable& operator=(const Transformable&) = delete;

protected:
    struct ModelViewProjection {
        alignas(16) glm::mat4 cameraMat;
        alignas(16) glm::mat4 transform;
    };

    Transformable() = default;

    glm::mat4 _localTransform{ 1.0f };
};
