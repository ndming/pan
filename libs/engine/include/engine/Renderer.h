#pragma once

#include <vulkan/vulkan.hpp>

#include <array>
#include <memory>


class SwapChain;
class View;


class Renderer {
public:
    void render(const std::unique_ptr<View>& view, SwapChain* swapChain) const;

    static constexpr int getMaxFramesInFlight() { return MAX_FRAMES_IN_FLIGHT; }

private:
    Renderer(const vk::CommandPool& graphicsCommandPool, const vk::Device& device);

    vk::CommandPool _graphicsCommandPool;

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

    // Would be better if we have an "internal" access specifier
    friend class Engine;
};
