#include "engine/Camera.h"

#include <glm/gtc/matrix_transform.hpp>


std::shared_ptr<Camera> Camera::create() {
    return std::make_shared<Camera>();
}

void Camera::setLookAt(const glm::vec3& position, const glm::vec3& target, const glm::vec3& up) {
    setLookAt(lookAt(position, target, up));
}

void Camera::setLookAt(const glm::mat4& view) {
    _view = view;
    _cameraMatrix = _proj * _view;
}

void Camera::setProjection(const float left, const float right, const float bottom, const float top, const float near, const float far) {
    setProjection(glm::ortho(left, right, bottom, top, near, far));
}

void Camera::setProjection(const float fov, const float aspect, const float near, const float far) {
    setProjection(glm::perspective(fov, aspect, near, far));
}

void Camera::setProjection(const glm::mat4& proj) {
    _proj = proj;

    // GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted. The easiest
    // way to compensate for that is to flip the sign on the scaling factor of the Y axis in the projection matrix. If
    // we don’t do this, then the image will be rendered upside down.
    _proj[1][1] *= -1;

    _cameraMatrix = _proj * _view;
}

const glm::mat4& Camera::getCameraMatrix() const & {
    return _cameraMatrix;
}

glm::mat4 Camera::getCameraMatrix() const && {
    return _cameraMatrix;
}
