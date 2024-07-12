#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "engine/DeviceFeature.h"


class Engine {
public:
    static std::unique_ptr<Engine> create();

    void bindSurface(GLFWwindow* window, const std::vector<DeviceFeature>& features = {}, bool asyncCompute = false);

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

    vk::PhysicalDevice _physicalDevice{};
    std::optional<uint32_t> _graphicsFamily{};
    std::optional<uint32_t> _presentFamily{};
    std::optional<uint32_t> _computeFamily{};
    void pickPhysicalDevice(const vk::PhysicalDeviceFeatures& features, bool asyncCompute);

    vk::Device _device{};
    vk::Queue _graphicsQueue{};
    vk::Queue _presentQueue{};
    vk::Queue _computeQueue{};
    void createLogicalDevice(const vk::PhysicalDeviceFeatures& features);
};
