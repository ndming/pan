#pragma once

#include <memory>
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

    [[nodiscard]] SwapChain* createSwapChain(const Context* context, const std::vector<DeviceFeature>& features = {});
    void destroySwapChain(const SwapChain* swapChain) const noexcept;

    [[nodiscard]] Renderer* createRenderer(SwapChain* swapChain, Renderer::Pipeline pipeline) const;
    void destroyRenderer(const Renderer* renderer) const noexcept;

    void flushAndWait() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    Engine();

    vk::Instance _instance{};

#ifndef NDEBUG
    vk::DebugUtilsMessengerEXT _debugMessenger{};
#endif

    vk::Device _device{};
    vk::Queue _transferQueue{};
};
