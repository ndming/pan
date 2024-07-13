#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <plog/Log.h>

#include "Context.h"


std::unique_ptr<Context> Context::create(const std::string_view name, const int width, const int height) {
    return std::unique_ptr<Context>(new Context{ name, width, height });
}

Context::Context(const std::string_view name, const int width, const int height) {
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
}

void Context::destroy() const noexcept {
    glfwDestroyWindow(_window);
    glfwTerminate();
}

GLFWwindow* Context::getNativeWindow() const {
    return _window;
}
