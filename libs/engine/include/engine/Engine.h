#pragma once

#include "engine/Context.h"
#include "engine/DeviceFeature.h"
#include "engine/Renderer.h"
#include "engine/SwapChain.h"


class Engine {
public:
    static std::unique_ptr<Engine> create();
    void destroy() const noexcept;

    [[nodiscard]] std::unique_ptr<SwapChain> createSwapChain(const std::unique_ptr<Context>& context, const std::vector<DeviceFeature>& features = {});
    void destroySwapChain(const std::unique_ptr<SwapChain>& swapChain) const noexcept;

    // [[nodiscard]] Renderer* createRenderer(SwapChain* swapChain, Renderer::Pipeline pipeline) const;
    // void destroyRenderer(const Renderer* renderer) const noexcept;

    void flushAndWait() const;

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

private:
    Engine();

    // The instance is the connection between our application and the Vulkan library
    vk::Instance _instance{};

#ifndef NDEBUG
    // The validation layers will print debug messages to the standard output by default,
    // but we can also handle them ourselves by providing an explicit callback in our program
    vk::DebugUtilsMessengerEXT _debugMessenger{};
#endif

    vk::Device _device{};
    vk::Queue _transferQueue{};
};
