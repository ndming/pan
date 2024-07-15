#include <limits>

#include <plog/Log.h>

#include "Translator.h"

#include "engine/Renderer.h"


bool Renderer::beginFrame(SwapChain* const swapChain) {
    using limits = std::numeric_limits<uint64_t>;

    // Wait until the previous frame has finished drawing
    if (_device.waitForFences(_drawingInFlightFences[_currentFrame], vk::True, limits::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for drawing fences!");
    }

    // Acquire an image from the swapchain
    _imageIndex = _device.acquireNextImageKHR(
        swapChain->_nativeObject, limits::max(), _imageAvailableSemaphores[_currentFrame], nullptr);
    if (_imageIndex.result == vk::Result::eErrorOutOfDateKHR) {
        PLOGD << "Recreating swap chain in beginFrame...";
        swapChain->recreate(_device, _renderPass);
        return false;
    }
    if (_imageIndex.result != vk::Result::eSuccess && _imageIndex.result != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    return true;
}

void Renderer::endFrame(SwapChain* const swapChain) {
    // Present the drawn images
    const auto presentInfo = vk::PresentInfoKHR{
        1, &_renderFinishedSemaphores[_currentFrame], 1, &swapChain->_nativeObject, &_imageIndex.value };
    if (const auto result = _presentQueue.presentKHR(&presentInfo);
         result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || _framebufferResized) {
        _framebufferResized = false;
        swapChain->recreate(_device, _renderPass);
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swap chain image!");
    }

    // Advance to the next frame
    _currentFrame = (_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::compute(const double frameTimeSeconds) const {
    static auto lastTime = frameTimeSeconds;
    const auto currentTime = frameTimeSeconds;
    const auto deltaTime = (currentTime - lastTime) * 2000.0;
    lastTime = currentTime;

    // Wait until the previous frame has finished computing
    using limits = std::numeric_limits<uint64_t>;
    if (_device.waitForFences(_computeInFlightFences[_currentFrame], vk::True, limits::max()) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for compute fences!");
    }

    _device.resetFences(_computeInFlightFences[_currentFrame]);

    // Record compute work
    _computeCommandBuffers[_currentFrame].reset();
    recordComputeCommands(_computeCommandBuffers[_currentFrame], _currentFrame, static_cast<float>(deltaTime));

    // Submit compute work
    const auto computeSubmitInfo = vk::SubmitInfo{
            {}, {}, {}, 1, &_computeCommandBuffers[_currentFrame], 1, &_computeFinishedSemaphores[_currentFrame] };
    _computeQueue.submit(computeSubmitInfo, _computeInFlightFences[_currentFrame]);
}

void Renderer::render(SwapChain* const swapChain) const {
    // Only reset the fence if we are submitting work
    _device.resetFences(_drawingInFlightFences[_currentFrame]);

    _drawingCommandBuffers[_currentFrame].reset();
    recordDrawingCommands(_drawingCommandBuffers[_currentFrame], _imageIndex.value, swapChain);

    // Submit draw commands
    constexpr vk::PipelineStageFlags waitStages[] {
        vk::PipelineStageFlagBits::eVertexInput, vk::PipelineStageFlagBits::eColorAttachmentOutput };
    const vk::Semaphore waitSemaphores[] {
        _imageAvailableSemaphores[_currentFrame], _computeFinishedSemaphores[_currentFrame] };
    const auto drawingSubmitInfo = vk::SubmitInfo{
        2, waitSemaphores, waitStages, 1, &_drawingCommandBuffers[_currentFrame],
        1, &_renderFinishedSemaphores[_currentFrame] };
    _graphicsQueue.submit(drawingSubmitInfo, _drawingInFlightFences[_currentFrame]);
}

void Renderer::framebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] const int w, [[maybe_unused]] const int h) {
    const auto renderer = static_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->_framebufferResized = true;
}

void Renderer::recordDrawingCommands(const vk::CommandBuffer &buffer, const uint32_t imageIndex, SwapChain* const swapChain) const {
    buffer.begin(vk::CommandBufferBeginInfo{});

    // Start the render pass
    constexpr auto clearValues = std::array<vk::ClearValue, 1>{
        vk::ClearColorValue{ 0.0f, 0.0f, 0.0f, 1.0f },
    };
    const auto renderPassInfo = vk::RenderPassBeginInfo{
        _renderPass, swapChain->_framebuffers[imageIndex], { { 0, 0 }, swapChain->_imageExtent },
        static_cast<uint32_t>(clearValues.size()), clearValues.data() };
    buffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

    // Bind the graphics pipeline
    buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, _drawingPipeline);

    // Set viewport and scissor state
    const auto viewport = vk::Viewport{
        0.0f, 0.0f, static_cast<float>(swapChain->_imageExtent.width), static_cast<float>(swapChain->_imageExtent.height),
        0.0f, 1.0f };
    buffer.setViewport(0, viewport);
    buffer.setScissor(0, vk::Rect2D{ { 0, 0 }, swapChain->_imageExtent });

    // Bind the storage buffer
    buffer.bindVertexBuffers(0, _shaderStorageBuffers[_currentFrame], vk::DeviceSize{ 0 });

    // Issue the draw call
    buffer.draw(PARTICLE_COUNT, 1, 0, 0);

    // End the drawing
    buffer.endRenderPass();
    buffer.end();
}

void Renderer::recordComputeCommands(const vk::CommandBuffer &buffer, const uint32_t currentFrame, const float deltaTime) const {
    buffer.begin(vk::CommandBufferBeginInfo{});

    buffer.bindPipeline(vk::PipelineBindPoint::eCompute, _computePipeline);
    buffer.bindDescriptorSets(
        vk::PipelineBindPoint::eCompute, _computePipelineLayout, 0, _computeDescriptorSets[currentFrame], {});

    const auto param = Translator::ParticleParam{ deltaTime };
    buffer.pushConstants(_computePipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, sizeof(Translator::ParticleParam), &param);

    // The vkCmdDispatch will dispatch PARTICLE_COUNT / 256 local work groups in the x dimension. As our particles
    // array is linear, we leave the other two dimensions at one, resulting in a one-dimensional dispatch. But why do
    // we divide the number of particles (in our array) by 256? That’s because we defined that every compute shader in
    // a work group will do 256 invocations. So if we were to have 4096 particles, we would dispatch 16 work groups,
    // with each work group running 256 compute shader invocations. Getting the two numbers right usually takes some
    // tinkering and profiling, depending on your workload and the hardware you’re running on. If your particle size
    // would be dynamic and can’t always be divided by e.g. 256, we can always use gl_GlobalInvocationID at the start of
    // our compute shader and return from it if the global invocation index is greater than the number of our particles.
    buffer.dispatch(PARTICLE_COUNT / 256, 1, 1);

    buffer.end();
}
