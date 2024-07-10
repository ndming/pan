#include "DebugMessenger.h"
#include "InstanceBuilder.h"

#include "engine/Engine.h"


std::unique_ptr<Engine> Engine::create() {
    return std::unique_ptr<Engine>(new Engine{});
}

Engine::Engine() {
    // Create a Vulkan instance
    _instance = InstanceBuilder()
        .applicationName("pan")
        .applicationVersion(1, 0, 0)
        .apiVersion(1, 3, 0)
        .build();

#ifndef NDEBUG
    _debugMessenger = DebugMessenger::create(_instance);
#endif

}

void Engine::bindSurface(GLFWwindow* const window) {
    // Register a framebuffer callback
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // Create a surface
    if (static_cast<vk::Result>(glfwCreateWindowSurface(
        _instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface))) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create window surface!");
    }
}

void Engine::framebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] const int w, [[maybe_unused]] const int h) {
    const auto engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine->_framebufferResized = true;
}

void Engine::destroy() const {
    _instance.destroySurfaceKHR(_surface);
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}