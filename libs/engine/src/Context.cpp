#include <GLFW/glfw3.h>

#include "engine/Context.h"


std::unique_ptr<Context> Context::create(const std::string_view name, const int width, const int height) {
    return std::unique_ptr<Context>(new Context{ name, width, height });
}

Context::Context(const std::string_view name, const int width, const int height) {
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _nativeWindow = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
}

void Context::destroy() const {
    glfwDestroyWindow(_nativeWindow);
    glfwTerminate();
}

void* Context::getNativeWindow() const {
    return _nativeWindow;
}
