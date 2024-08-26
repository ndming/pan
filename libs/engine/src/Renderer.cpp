#include "engine/Renderer.h"
#include "engine/Composable.h"
#include "engine/Overlay.h"
#include "engine/Scene.h"
#include "engine/SwapChain.h"
#include "engine/View.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
    const std::shared_ptr<SwapChain>& swapChain,
    const std::function<void(uint32_t)>& onFrameBegin
) {
    // Try acquire an image from the swap chain
    uint32_t imageIndex;
    if (!beginFrame(swapChain, onFrameBegin, &imageIndex)) return;

    // We can start record drawing commands to the buffer
    _drawingCommandBuffers[_currentFrame].reset();
    _drawingCommandBuffers[_currentFrame].begin(vk::CommandBufferBeginInfo{});

    // Begine the render pass
    const auto renderPassInfo = vk::RenderPassBeginInfo{
        swapChain->getNativeRenderPass(), swapChain->getNativeFramebufferAt(imageIndex),
        { { 0, 0 }, swapChain->getNativeSwapImageExtent() }, CLEAR_VALUES };
    // Right now we're not using any secondary command buffer, hence vk::SubpassContents::eInline
    _drawingCommandBuffers[_currentFrame].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    renderView(view);

    // End the render pass and finish recording the command buffer
    _drawingCommandBuffers[_currentFrame].endRenderPass();
    _drawingCommandBuffers[_currentFrame].end();

    // Submit the rendered frame to the swap chain for presentation
    endFrame(imageIndex, swapChain);

    // Advance to the next frame
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::render(
    const std::unique_ptr<View>& view,
    const std::shared_ptr<Overlay>& overlay,
    const std::shared_ptr<SwapChain>& swapChain,
    const std::function<void(uint32_t)>& onFrameBegin
) {
    // Try to acquire an image from the swap chain
    uint32_t imageIndex;
    if (!beginFrame(swapChain, onFrameBegin, &imageIndex)) return;

    // We can now start record drawing commands to the buffer
    _drawingCommandBuffers[_currentFrame].reset();
    _drawingCommandBuffers[_currentFrame].begin(vk::CommandBufferBeginInfo{});

    // Begin the render pass
    const auto renderPassInfo = vk::RenderPassBeginInfo{
        swapChain->getNativeRenderPass(), swapChain->getNativeFramebufferAt(imageIndex),
        { { 0, 0 }, swapChain->getNativeSwapImageExtent() }, CLEAR_VALUES };
    // Right now we're not using any secondary command buffer, hence vk::SubpassContents::eInline
    _drawingCommandBuffers[_currentFrame].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    renderView(view);
    renderOverlay(overlay);

    // End the render pass and finish recording the command buffer
    _drawingCommandBuffers[_currentFrame].endRenderPass();
    _drawingCommandBuffers[_currentFrame].end();

    // Submit the rendered frame to the swap chain for presentation
    endFrame(imageIndex, swapChain);

    // Advance to the next frame
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

bool Renderer::beginFrame(
    const std::shared_ptr<SwapChain>& swapChain,
    const std::function<void(uint32_t)>& onFrameBegin,
    uint32_t* imageIndex
) const {
    using limits = std::numeric_limits<uint64_t>;

    // At the start of the frame, we want to wait until the command buffer has finished all the rendering work
    // which was recorded for the previous frame
    [[maybe_unused]] const auto result = _device.waitForFences(_drawingFences[_currentFrame], vk::True, limits::max());

    // We acquire an image from the swapchain and provide a semaphore for the swap chain to signal when the image
    // becomes available. That’s the point in time where we can start drawing to it.
    if (!swapChain->acquire(_device, limits::max(), _imageAvailableSemaphores[_currentFrame], imageIndex)) {
        // The swap chain tells us to try again in the next rendering iteration
        return false;
    }

    // If we indeed acquired a swap chain image, we can start draw stuff to it. The drawing fence for this frame
    // can now be reset so that the command buffer later can signal it
    _device.resetFences(_drawingFences[_currentFrame]);

    // Give the call site a chance to update any frame-specific resource
    onFrameBegin(_currentFrame);

    return true;
}

void Renderer::endFrame(const uint32_t imageIndex, const std::shared_ptr<SwapChain>& swapChain) const {
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
}

void Renderer::renderView(const std::unique_ptr<View>& view) const {
    const auto scene = view->getScene();
    scene->forEach([this, &view](const std::shared_ptr<Composable>& composable) {
        const auto buffers = composable->recordDrawingCommands(
            _currentFrame, _drawingCommandBuffers[_currentFrame], view->getCamera()->getCameraMatrix(),
            /* current transform */ { 1.0f }, [this, &view](const vk::CommandBuffer& buffer) {
                // Set all dynamic states
                buffer.setViewport(0, view->getNativeViewport());
                buffer.setScissor(0, view->getNativeScissor());
                _vkCmdSetPolygonMode(buffer, static_cast<VkPolygonMode>(view->getNativePolygonMode()));
                buffer.setCullMode(view->getNativeCullMode());
                buffer.setFrontFace(view->getNativeFrontFace());
                buffer.setPrimitiveRestartEnable(view->getNativePrimitiveRestartEnabled());
                buffer.setLineWidth(view->getLineWidth());
            });
    });
}

void Renderer::renderOverlay(const std::shared_ptr<Overlay>& overlay) const {
    // Start the Dear ImGui frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Define the layout
    overlay->define();

    // Rendering
    ImGui::Render();
    const auto drawData = ImGui::GetDrawData();

    // Record dear imgui primitives into command buffer
    ImGui_ImplVulkan_RenderDrawData(drawData, _drawingCommandBuffers[_currentFrame]);
}
