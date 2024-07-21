#pragma once

#include "engine/Context.h"
#include "engine/DeviceFeature.h"
#include "engine/Renderer.h"
#include "engine/SwapChain.h"


class Engine {
public:
    static std::unique_ptr<Engine> create();
    void destroy() const noexcept;

    [[nodiscard]] std::unique_ptr<SwapChain> createSwapChain(
        const std::unique_ptr<Context>& context,
        const std::vector<DeviceFeature>& features = {},
        SwapChain::MSAA msaa = SwapChain::MSAA::x1);
    void destroySwapChain(const std::unique_ptr<SwapChain>& swapChain) const noexcept;

    // [[nodiscard]] Renderer* createRenderer(SwapChain* swapChain, Renderer::Pipeline pipeline) const;
    // void destroyRenderer(const Renderer* renderer) const noexcept;

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
