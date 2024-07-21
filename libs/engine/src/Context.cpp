#include "engine/Context.h"

#include <GLFW/glfw3.h>


std::unique_ptr<Context> Context::create(const std::string_view name, const int width, const int height) {
    return std::unique_ptr<Context>{ new Context(name, width, height) };
}

Context::Context(const std::string_view name, const int width, const int height) {
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
}

void Context::destroy() const noexcept {
    glfwDestroyWindow(_window);
    glfwTerminate();
}

void Context::loop(const std::function<void()> &onFrame) const {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        onFrame();
    }
}
