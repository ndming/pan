#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "engine/DeviceFeature.h"
#include "engine/SwapChain.h"


struct GLFWwindow;

class Engine {
public:
    static std::unique_ptr<Engine> create();
    void destroy() const;

    void attachSurface(void* nativeWindow, const std::vector<DeviceFeature>& features = {});
    void detachSurface() const;

    [[nodiscard]] std::unique_ptr<SwapChain> createSwapChain(void* nativeWindow) const;
    void destroySwapChain(std::unique_ptr<SwapChain> swapChain);

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
    void pickPhysicalDevice(const vk::PhysicalDeviceFeatures& features);

    vk::Device _device{};
    vk::Queue _graphicsQueue{};
    vk::Queue _presentQueue{};
    vk::Queue _computeQueue{};
    void createLogicalDevice(const vk::PhysicalDeviceFeatures& features);
    void createAllocator() const;
};
