#include "engine/Context.h"

#include <GLFW/glfw3.h>


static std::function<void(double, double)> mMouseClickCallback{ [](auto, auto) {} };

std::unique_ptr<Context> Context::create(const std::string_view name) {
    return std::unique_ptr<Context>{ new Context(name) };
}

Context::Context(const std::string_view name) {
    glfwInit();

    // Get the primary monitor and the video mode
    const auto monitor = glfwGetPrimaryMonitor();
    const auto mode = glfwGetVideoMode(monitor);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // don't create an OpenGL context

    // Create a window and maximize it
    _window = glfwCreateWindow(mode->width, mode->height, name.data(), nullptr, nullptr);
    glfwMaximizeWindow(_window);

    glfwSetMouseButtonCallback(_window, [](const auto window, const auto button, const auto action, auto mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                double xPos, yPos;
                glfwGetCursorPos(window, &xPos, &yPos);
                mMouseClickCallback(xPos, yPos);
            }
        }
    });
}

void Context::destroy() const noexcept {
    glfwDestroyWindow(_window);
    glfwTerminate();
}

void Context::setOnMouseClick(const std::function<void(double, double)>& callback) {
    mMouseClickCallback = callback;
}

void Context::setOnMouseClick(std::function<void(double, double)>&& callback) noexcept {
    mMouseClickCallback = std::move(callback);
}

Surface* Context::getSurface() const {
    return _window;
}

void Context::loop(const std::function<void()> &onFrame) const {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        onFrame();
    }
}
