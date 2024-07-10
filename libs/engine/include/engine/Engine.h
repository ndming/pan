#pragma once

#include <memory>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


class Engine {
public:
    static std::unique_ptr<Engine> create();

    void bindSurface(GLFWwindow* window);

    void destroy() const;

private:
    Engine();

    vk::Instance _instance{};
#ifndef NDEBUG
    vk::DebugUtilsMessengerEXT _debugMessenger{};
#endif

    bool _framebufferResized = false;
    static void framebufferResizeCallback(GLFWwindow*, int, int);

    vk::SurfaceKHR _surface{};
};
