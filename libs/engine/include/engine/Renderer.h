#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <functional>
#include <memory>


class SwapChain;
class View;
class Overlay;


class Renderer {
public:
    void render(
        const std::unique_ptr<View>& view,
        const std::shared_ptr<SwapChain>& swapChain,
        const std::function<void(uint32_t)>& onFrameBegin = [](const uint32_t) {});

    void render(
        const std::unique_ptr<View>& view,
        const std::shared_ptr<Overlay>& overlay,
        const std::shared_ptr<SwapChain>& swapChain,
        const std::function<void(uint32_t)>& onFrameBegin = [](const uint32_t) {});

    static constexpr int getMaxFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

private:
    Renderer(
        const vk::CommandPool& graphicsCommandPool,
        const vk::Queue& graphicsQueue,
        const vk::Device& device,
        PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonMode);

    void renderView(const std::unique_ptr<View>& view) const;
    void renderOverlay(const std::shared_ptr<Overlay>& overlay) const;

    bool beginFrame(
        const std::shared_ptr<SwapChain>& swapChain,
        const std::function<void(uint32_t)>& onFrameBegin,
        uint32_t* imageIndex) const;
    void endFrame(uint32_t imageIndex, const std::shared_ptr<SwapChain>& swapChain) const;

    vk::CommandPool _graphicsCommandPool;
    vk::Queue _graphicsQueue;

    // Instead of asking the Engine to provide the device for each rendering interation,
    // the Renderer better off get its own copy of it
    vk::Device _device;

    // Each in-flight frame will has its own command buffer, semaphore set, and in-flight fence
    static constexpr auto MAX_FRAMES_IN_FLIGHT = 2;
    std::array<vk::CommandBuffer, MAX_FRAMES_IN_FLIGHT> _drawingCommandBuffers;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _imageAvailableSemaphores;
    std::array<vk::Semaphore, MAX_FRAMES_IN_FLIGHT> _renderFinishedSemaphores;
    std::array<vk::Fence, MAX_FRAMES_IN_FLIGHT> _drawingFences;

    // Which in-flight frame we are current at (frame index)
    uint32_t _currentFrame{ 0 };

    // Allow us to record dynamic polygon mode state to the command buffer. Because the vkCmdSetPolygonModeEXT
    // is an extension function, we have to manually load it ourselves
    PFN_vkCmdSetPolygonModeEXT _vkCmdSetPolygonMode;

    // The clear values per render pass attachment. The order should be identical to the order we specified
    // the render pass attachments
    static constexpr auto CLEAR_VALUES = std::array<vk::ClearValue, 2>{
        vk::ClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f },  // for the multi-sampled color attachment
        vk::ClearDepthStencilValue{ /* depth */ 1.0f, /* stencil */ 0 },  // for the depth/stencil attachment
    };

    // Would be better if we have an "internal" access specifier
    friend class Engine;
};
