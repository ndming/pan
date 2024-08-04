#include "engine/Renderer.h"


Renderer::Renderer(
    const vk::CommandPool& graphicsCommandPool,
    const vk::Device& device
) : _graphicsCommandPool{ graphicsCommandPool }, _device{ device } {
    // Allocate drawing command buffers for each in-flight frame
    const auto allocInfo = vk::CommandBufferAllocateInfo{
        _graphicsCommandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT) };
    auto commandBuffers = _device.allocateCommandBuffers(allocInfo);
    std::ranges::move(commandBuffers, _drawingCommandBuffers.begin());

    // Create sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        _imageAvailableSemaphores[i] = _device.createSemaphore({});
        _renderFinishedSemaphores[i] = _device.createSemaphore({});
        _drawingFences[i] = _device.createFence({ vk::FenceCreateFlagBits::eSignaled });
    }
}

void Renderer::render(const std::unique_ptr<View>& view, SwapChain* const swapChain) const {

}
