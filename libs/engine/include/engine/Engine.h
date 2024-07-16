#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <vulkan/vulkan.hpp>

#include "engine/Context.h"
#include "engine/DeviceFeature.h"
#include "engine/Renderer.h"
#include "engine/SwapChain.h"


class Engine {
public:
    static std::unique_ptr<Engine> create();
    void destroy() const noexcept;

    void attachSurface(GLFWwindow* window, const std::vector<DeviceFeature>& features = {});
    void detachSurface() const noexcept;

    [[nodiscard]] SwapChain* createSwapChain(GLFWwindow* window, SwapChain::MSAA msaa = SwapChain::MSAA::x1) const;
    void destroySwapChain(SwapChain* swapChain) const noexcept;

    [[nodiscard]] SwapChain* createSwapChain(Context* context) const;

    [[nodiscard]] Renderer* createRenderer(SwapChain* swapChain, Renderer::Pipeline pipeline) const;
    void destroyRenderer(const Renderer* renderer) const noexcept;

    void flushAndWait() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    Engine();

public:
    vk::Instance _instance{};

#ifndef NDEBUG
    vk::DebugUtilsMessengerEXT _debugMessenger{};
#endif

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

    vk::CommandPool _graphicsCommandPool{};
    vk::CommandPool _transferCommandPool{};
    void createCommandPools();
    [[nodiscard]] vk::CommandBuffer beginSingleTimeTransferCommands() const;
    void endSingleTimeTransferCommands(const vk::CommandBuffer& commandBuffer) const;

    void createShaderStorageBuffers(Renderer* renderer) const;

    void createDescriptorPool(Renderer* renderer) const;
    void createComputeDescriptorSets(Renderer* renderer) const;

    void createCommandBuffers(Renderer* renderer) const;

    void createDrawingSyncObjects(Renderer* renderer) const;
    void createComputeSyncObjects(Renderer* renderer) const;
};
