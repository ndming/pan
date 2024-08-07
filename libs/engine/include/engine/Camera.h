#pragma once

#include <glm/glm.hpp>

#include <memory>


class Camera final {
public:
    [[nodiscard]] static std::shared_ptr<Camera> create();

    void setLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up);
    void setLookAt(const glm::mat4& view);

    void setProjection(float left, float right, float bottom, float top, float near, float far);
    void setProjection(float fov, float aspect, float near, float far);
    void setProjection(const glm::mat4& proj);

    [[nodiscard]] const glm::mat4& getCameraMatrix() const &;
    [[nodiscard]] glm::mat4 getCameraMatrix() const &&;

private:
    glm::mat4 _cameraMatrix{ 1.0f };

    glm::mat4 _view{ 1.0f };
    glm::mat4 _proj{ 1.0f };
};
