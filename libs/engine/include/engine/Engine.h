#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include "engine/DeviceFeature.h"
#include "engine/SwapChain.h"


class Engine {
public:
    static std::unique_ptr<Engine> create();
    void destroy() const noexcept;

    void attachSurface(GLFWwindow* window, const std::vector<DeviceFeature>& features = {});
    void detachSurface() const noexcept;

    [[nodiscard]] std::unique_ptr<SwapChain> createSwapChain(GLFWwindow* window) const;
    void destroySwapChain(std::unique_ptr<SwapChain>&& swapChain) const noexcept;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    Engine();

    vk::Instance _instance{};

#ifdef NDEBUG
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

    SwapChain* initializeSwapChain(GLFWwindow* window) const;
    void createSwapChainImageViews(SwapChain* swapChain) const;
};
