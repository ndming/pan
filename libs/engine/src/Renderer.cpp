#include "engine/Renderer.h"
#include "engine/Composable.h"
#include "engine/Scene.h"
#include "engine/SwapChain.h"
#include "engine/View.h"

#include <limits>


Renderer::Renderer(
    const vk::CommandPool& graphicsCommandPool,
    const vk::Queue& graphicsQueue,
    const vk::Device& device,
    PFN_vkCmdSetPolygonModeEXT vkCmdSetPolygonMode
) : _graphicsCommandPool{ graphicsCommandPool },
    _graphicsQueue{ graphicsQueue },
    _device{ device },
    _vkCmdSetPolygonMode{ vkCmdSetPolygonMode } {
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

void Renderer::render(
    const std::unique_ptr<View>& view,
    SwapChain* const swapChain,
    const std::function<void(uint32_t)>& onFrameBegin
) {
    using limits = std::numeric_limits<uint64_t>;

    // At the start of the frame, we want to wait until the command buffer has finished all the rendering work
    // which was recorded for the previous frame
    [[maybe_unused]] const auto result = _device.waitForFences(_drawingFences[_currentFrame], vk::True, limits::max());

    // We acquire an image from the swapchain and provide a semaphore for the swap chain to signal when the image
    // becomes available. That’s the point in time where we can start drawing to it.
    uint32_t imageIndex;
    if (!swapChain->acquire(_device, limits::max(), _imageAvailableSemaphores[_currentFrame], &imageIndex)) {
        // The swap chain tells us to try again in the next rendering iteration
        return;
    }

    // If we indeed acquired a swap chain image, we can start draw stuff to it. The drawing fence for this frame
    // can now be reset so that the command buffer later can signal it
    _device.resetFences(_drawingFences[_currentFrame]);

    // Give the call site a chance to update any frame-specific resource
    onFrameBegin(_currentFrame);

    // We can start record drawing commands to the buffer
    _drawingCommandBuffers[_currentFrame].reset();
    renderView(view, imageIndex, swapChain);

    // With a fully recorded command buffer, we can now submit it. First we need to specify which semaphores to wait on
    // before execution can begin. It should be the semaphore we gave the swap chain which will signal it when the
    // acquired image is available
    const auto waitSemaphores = std::array{ _imageAvailableSemaphores[_currentFrame] };
    // We also need to specify in which stage(s) of the pipeline to wait. We want to wait with writing colors to the
    // image until it’s available. That means that theoretically the implementation can already start executing our
    // vertex shader and such while the image is not yet available. Each entry in the waitStages array corresponds to
    // the semaphore with the same index in the waitSemaphores array.
    // When opting for eColorAttachmentOutput stage, the implicit subpass at the start of the render pass does not
    // occur at the right time. It assumes that the transition occurs at the start of the pipeline, but we haven’t
    // acquired the image yet at that point. We could change the waitStages for the imageAvailableSemaphore to
    // eTopOfPipe, but it't better to synchronize this implicit subpass through subpass dependencies
    constexpr vk::PipelineStageFlags waitStages[]{ vk::PipelineStageFlagBits::eColorAttachmentOutput };
    const auto drawingSubmitInfo = vk::SubmitInfo{
        static_cast<uint32_t>(waitSemaphores.size()), waitSemaphores.data(), waitStages,
        1, &_drawingCommandBuffers[_currentFrame], 1, &_renderFinishedSemaphores[_currentFrame] };
    // Submit drawing commands to the queue and specify which fence to signal when all the operations have finished,
    // allowing us to know when it is safe for the command buffer to be reused
    _graphicsQueue.submit(drawingSubmitInfo, _drawingFences[_currentFrame]);

    // The last step is to instruct the swap chain to present the drawn image. We also need to tell the swap chain
    // which semaphore it should wait on before presenting.
    swapChain->present(_device, imageIndex, _renderFinishedSemaphores[_currentFrame]);

    // Advance to the next frame
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::renderView(const std::unique_ptr<View>& view, const uint32_t imageIndex, const SwapChain* const swapChain) const {
    // If the command buffer was already recorded once, then a call to vkBeginCommandBuffer will implicitly reset it.
    // It’s not possible to append commands to a buffer at a later time.
    _drawingCommandBuffers[_currentFrame].begin(vk::CommandBufferBeginInfo{});

    // Specify the clear values for each attachment. The order should be identical to the order
    // we specified render pass attachments
    constexpr auto clearValues = std::array<vk::ClearValue, 1>{
        vk::ClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f },  // for the multi-sampled color attachment
    };

    // Begine the render pass
    const auto renderPassInfo = vk::RenderPassBeginInfo{
        swapChain->getNativeRenderPass(), swapChain->getNativeFramebufferAt(imageIndex),
        { { 0, 0 }, swapChain->getNativeSwapImageExtent() }, clearValues };
    // Right now we're not using any secondary command buffer, hence vk::SubpassContents::eInline
    _drawingCommandBuffers[_currentFrame].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    const auto scene = view->getScene();
    scene->forEach([this, &view](const std::shared_ptr<Composable>& composable) {
        const auto buffers = composable->recordDrawingCommands(
            _currentFrame, _drawingCommandBuffers[_currentFrame], { 1.0f }, { 1.0f }, [this, &view](const vk::CommandBuffer& buffer) {
                // Set all dynamic states
                buffer.setViewport(0, view->getNativeViewport());
                buffer.setScissor(0, view->getNativeScissor());
                _vkCmdSetPolygonMode(buffer, static_cast<VkPolygonMode>(view->getNativePolygonMode()));
                buffer.setCullMode(view->getNativeCullMode());
                buffer.setFrontFace(view->getNativeFrontFace());
                buffer.setLineWidth(view->getLineWidth());
            });
    });

    // End the render pass and finish recording the command buffer
    _drawingCommandBuffers[_currentFrame].endRenderPass();
    _drawingCommandBuffers[_currentFrame].end();
}
