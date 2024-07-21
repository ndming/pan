#include "engine/Engine.h"

#include "ResourceAllocator.h"
#include "Translator.h"

#include "bootstrap/DebugMessenger.h"
#include "bootstrap/DeviceBuilder.h"
#include "bootstrap/InstanceBuilder.h"

#include <plog/Log.h>

#include <array>
#include <ranges>
#include <set>


#ifndef NDEBUG
/* Validation layers and messenger callback */
static constexpr std::array mValidationLayers{
    "VK_LAYER_KHRONOS_validation",    // standard validation
};

VKAPI_ATTR vk::Bool32 VKAPI_CALL mCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] const VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    [[maybe_unused]] void* const userData
) {
    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            PLOGV << pCallbackData->pMessage; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            PLOGI << pCallbackData->pMessage; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            PLOGW << pCallbackData->pMessage; break;
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            PLOGE << pCallbackData->pMessage; break;
        default: break;
    }

    return vk::False;
}
#endif


/* Device extensions explicitly required by the engine */
static constexpr std::array mDeviceExtensions{
    vk::EXTMemoryBudgetExtensionName,      // to make our allocator estimate memory budget more accurately
    vk::EXTMemoryPriorityExtensionName,    // incorporate memory priority to the allocator
};


/* We don't make the allocator a memeber of Engine because doing so would require the vma library to be public */
static auto mAllocator = std::unique_ptr<ResourceAllocator>{};


/* Engine initialization and destruction */
std::unique_ptr<Engine> Engine::create() {
    return std::unique_ptr<Engine>(new Engine{});
}

Engine::Engine() {
    // Create a Vulkan instance
    _instance = InstanceBuilder()
        .applicationName("pan")
        .applicationVersion(1, 0, 0)
        .apiVersion(1, 2, 0)
#ifndef NDEBUG
        .layers(mValidationLayers.data(), mValidationLayers.size())
        .callback(mCallback)
#endif
        .build();

#ifndef NDEBUG
    // Plant a debug messenger
    _debugMessenger = DebugMessenger::create(_instance, mCallback);
#endif
}

void Engine::destroy() const noexcept {
    _device.destroy(nullptr);  // should we check for the state of device here?
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}


/* Swap chain */
std::unique_ptr<SwapChain> Engine::createSwapChain(const std::unique_ptr<Context>& context, const std::vector<DeviceFeature>& features) {
    // Have the swap chain create a surface and pick the physical device according to its need
    const auto deviceFeatures = Translator::toPhysicalDeviceFeatures(features);
    auto deviceExtensions = std::vector(mDeviceExtensions.begin(), mDeviceExtensions.end());
    deviceExtensions.push_back(vk::KHRSwapchainExtensionName);   // to present to a surface
    const auto swapChain = new SwapChain{ context->_window, _instance, deviceFeatures, deviceExtensions };

    // Create a logical device
    auto uniqueFamilies = std::set{ swapChain->_graphicsFamily.value(), swapChain->_presentFamily.value() };
    if (swapChain->_computeFamily.has_value()) {
        uniqueFamilies.insert(swapChain->_computeFamily.value());
    }
    _device = DeviceBuilder()
        .queueFamilies(uniqueFamilies)
        .deviceFeatures(deviceFeatures)
        .deviceExtensions(deviceExtensions)
        .validationLayers({ mValidationLayers.begin(), mValidationLayers.end() })
        .build(swapChain->_physicalDevice);

    // We need a transfer queue for resource allocations. Any queue family with VK_QUEUE_GRAPHICS_BIT capabilities
    // already implicitly support VK_QUEUE_TRANSFER_BIT operations.
    _transferQueue = _device.getQueue(swapChain->_graphicsFamily.value(), 0);

    // Create a resource allocator
    mAllocator = ResourceAllocator::Builder()
        .flags(VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT)
        .vulkanApiVersion(VK_API_VERSION_1_2)
        .build(_instance, swapChain->_physicalDevice, _device);

    // Now we can actually create a native Vulkan swap chain object
    swapChain->initSwapChain(_device, mAllocator.get());

    return std::unique_ptr<SwapChain>(swapChain);
}

void Engine::destroySwapChain(const std::unique_ptr<SwapChain>& swapChain) const noexcept {
    _device.destroyRenderPass(swapChain->_renderPass);
    swapChain->cleanup(_device);
    _instance.destroySurfaceKHR(swapChain->_surface);
    mAllocator.reset(nullptr);
}


/* Renderer */
// Renderer* Engine::createRenderer(SwapChain* const swapChain, const Renderer::Pipeline pipeline) const {
//     const auto renderer = new Renderer{};
//     renderer->_device = _device;
//     renderer->_graphicsQueue = _graphicsQueue;
//     renderer->_presentQueue = _presentQueue;
//     renderer->_computeQueue = _graphicsQueue;
//
//     // Register a framebuffer callback
//     glfwSetWindowUserPointer(swapChain->_window, renderer);
//     glfwSetFramebufferSizeCallback(swapChain->_window, Renderer::framebufferResizeCallback);
//
//     // Create a render pass
//     renderer->_renderPass = RenderPassBuider(swapChain->_imageFormat.format)
//         .sampleCount(swapChain->_msaa)
//         .build(_device);
//
//     // Populate the swap chain's framebuffers
//     using namespace std::ranges;
//     const auto toFramebuffer = [this, swapChain, renderer](const auto& swapChainImageView) {
//         const auto attachments = std::array{ swapChain->_colorImageView, swapChainImageView };
//         const auto framebufferInfo = vk::FramebufferCreateInfo{
//             {}, renderer->_renderPass, static_cast<uint32_t>(attachments.size()), attachments.data(),
//             swapChain->_imageExtent.width, swapChain->_imageExtent.height, 1 };
//         return _device.createFramebuffer(framebufferInfo);
//     };
//     swapChain->_framebuffers = swapChain->_imageViews | views::transform(toFramebuffer) | to<std::vector>();
//
//     // Descriptor set layouts and pipeline layouts
//     renderer->_drawingDescriptorSetLayout = Translator::toDrawingDescriptorSetLayout(pipeline, _device);
//     renderer->_computeDescriptorSetLayout = Translator::toComputeDescriptorSetLayout(pipeline, _device);
//     renderer->_drawingPipelineLayout = Translator::toDrawingPipelineLayout(pipeline, renderer->_drawingDescriptorSetLayout, _device);
//     renderer->_computePipelineLayout = Translator::toComputePipelineLayout(pipeline, renderer->_computeDescriptorSetLayout, _device);
//
//     // The pipelines
//     renderer->_drawingPipeline = Translator::toDrawingPipeline(
//         pipeline, renderer->_drawingPipelineLayout, renderer->_renderPass, swapChain->_msaa, _device);
//     renderer->_computePipeline = Translator::toComputePipeline(pipeline, renderer->_computePipelineLayout, _device);
//
//     createShaderStorageBuffers(renderer);
//     createDescriptorPool(renderer);
//     createComputeDescriptorSets(renderer);
//     createCommandBuffers(renderer);
//     createDrawingSyncObjects(renderer);
//     createComputeSyncObjects(renderer);
//
//     return renderer;
// }
//
// void Engine::destroyRenderer(const Renderer* const renderer) const noexcept {
//     using namespace std::ranges;
//     for_each(renderer->_computeInFlightFences, [this](const auto& it) { _device.destroyFence(it); });
//     for_each(renderer->_computeFinishedSemaphores, [this](const auto& it) { _device.destroySemaphore(it); });
//     for_each(renderer->_drawingInFlightFences, [this](const auto& it) { _device.destroyFence(it); });
//     for_each(renderer->_renderFinishedSemaphores, [this](const auto& it) { _device.destroySemaphore(it); });
//     for_each(renderer->_imageAvailableSemaphores, [this](const auto& it) { _device.destroySemaphore(it); });
//     _device.destroyDescriptorPool(renderer->_descriptorPool);
//     for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
//         vmaDestroyBuffer(
//             mAllocator->_allocator, renderer->_shaderStorageBuffers[i],
//             static_cast<VmaAllocation>(renderer->_shaderStorageBuffersAllocation[i]));
//     }
//     _device.destroyPipeline(renderer->_computePipeline);
//     _device.destroyPipeline(renderer->_drawingPipeline);
//     _device.destroyPipelineLayout(renderer->_computePipelineLayout);
//     _device.destroyPipelineLayout(renderer->_drawingPipelineLayout);
//     _device.destroyDescriptorSetLayout(renderer->_computeDescriptorSetLayout);
//     _device.destroyDescriptorSetLayout(renderer->_drawingDescriptorSetLayout);
//     _device.destroyRenderPass(renderer->_renderPass);
//
//     delete renderer;
// }
//
// void Engine::createShaderStorageBuffers(Renderer* const renderer) const {
//     renderer->_shaderStorageBuffers.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//     renderer->_shaderStorageBuffersAllocation.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//
//     auto generator = std::mt19937{ std::random_device{}() };
//     auto dist = std::uniform_real_distribution{ 0.0f, 1.0f };
//
//     const auto generateParticle = [&generator, &dist] {
//         const auto r = 0.25f * std::sqrt(dist(generator));
//         const auto theta = dist(generator) * 2 * std::numbers::pi_v<float>;
//
//         const auto x = r * std::cos(theta) * 768 / 1280;
//         const auto y = r * std::sin(theta);
//         return Translator::Particle{
//             glm::vec2(x, y), normalize(glm::vec2(x,y)) * 0.00025f,
//             glm::vec4(dist(generator), dist(generator), dist(generator), 1.0f) };
//     };
//
//     auto particles = std::vector<Translator::Particle>(Renderer::PARTICLE_COUNT);
//     std::ranges::generate_n(particles.begin(), Renderer::PARTICLE_COUNT, generateParticle);
//
//     // Create a staging buffer to transfer the data
//     constexpr auto bufferSize = sizeof(Translator::Particle) * Renderer::PARTICLE_COUNT;
//     auto staggingBufferCreateInfo = VkBufferCreateInfo{};
//     staggingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//     staggingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//     staggingBufferCreateInfo.size = bufferSize;
//
//     auto allocCreateInfo = VmaAllocationCreateInfo{};
//     allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
//     allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
//
//     auto stagingBuffer = VkBuffer{};
//     auto stagingBufferAllocation = VmaAllocation{};
//     vmaCreateBuffer(mAllocator->_allocator, &staggingBufferCreateInfo, &allocCreateInfo, &stagingBuffer, &stagingBufferAllocation, nullptr);
//
//     void* data;
//     vmaMapMemory(mAllocator->_allocator, stagingBufferAllocation, &data);
//     memcpy(data, particles.data(), bufferSize);
//     vmaUnmapMemory(mAllocator->_allocator, stagingBufferAllocation);
//
//
//     for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
//         auto storageBufferCreateInfo = VkBufferCreateInfo{};
//         storageBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//         storageBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
//         storageBufferCreateInfo.size = bufferSize;
//
//         auto allocInfo = VmaAllocationCreateInfo{};
//         allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
//         allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
//         allocInfo.priority = 1.0f;
//
//         auto storageBuffer = VkBuffer{};
//         auto storageBufferAllocation = VmaAllocation{};
//         vmaCreateBuffer(mAllocator->_allocator, &storageBufferCreateInfo, &allocInfo, &storageBuffer, &storageBufferAllocation, nullptr);
//
//         const auto commandBuffer = beginSingleTimeTransferCommands();
//         commandBuffer.copyBuffer(stagingBuffer, storageBuffer, vk::BufferCopy{ 0, 0, bufferSize });
//         endSingleTimeTransferCommands(commandBuffer);
//
//         renderer->_shaderStorageBuffers[i] = storageBuffer;
//         renderer->_shaderStorageBuffersAllocation[i] = storageBufferAllocation;
//     }
//
//     vmaDestroyBuffer(mAllocator->_allocator, stagingBuffer, stagingBufferAllocation);
// }
//
// void Engine::createDescriptorPool(Renderer* const renderer) const {
//     static constexpr auto poolSizes = std::array{
//         // 2 SSBOs in the compute shader
//         vk::DescriptorPoolSize{ vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(Renderer::MAX_FRAMES_IN_FLIGHT) * 2 },
//     };
//     constexpr auto poolInfo = vk::DescriptorPoolCreateInfo{
//         // 1 descriptor set (compute) for each in-flight frame
//             {}, static_cast<uint32_t>(Renderer::MAX_FRAMES_IN_FLIGHT), static_cast<uint32_t>(poolSizes.size()), poolSizes.data() };
//     renderer->_descriptorPool = _device.createDescriptorPool(poolInfo);
// }
//
// void Engine::createComputeDescriptorSets(Renderer* const renderer) const {
//     const auto layouts = std::vector(Renderer::MAX_FRAMES_IN_FLIGHT, renderer->_computeDescriptorSetLayout);
//     const auto allocInfo = vk::DescriptorSetAllocateInfo{
//         renderer->_descriptorPool, static_cast<uint32_t>(Renderer::MAX_FRAMES_IN_FLIGHT), layouts.data() };
//     renderer->_computeDescriptorSets = _device.allocateDescriptorSets(allocInfo);
//
//     for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
//         const auto storageBufferInfoLastFrame = vk::DescriptorBufferInfo{
//             renderer->_shaderStorageBuffers[(i - 1) % Renderer::MAX_FRAMES_IN_FLIGHT], 0, sizeof(Translator::Particle) * Renderer::PARTICLE_COUNT };
//         const auto storageBufferInfoCurrentFrame = vk::DescriptorBufferInfo{
//             renderer->_shaderStorageBuffers[i], 0, sizeof(Translator::Particle) * Renderer::PARTICLE_COUNT };
//
//         const auto descriptorWrites = std::array{
//             vk::WriteDescriptorSet{ renderer->_computeDescriptorSets[i], 0, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &storageBufferInfoLastFrame },
//             vk::WriteDescriptorSet{ renderer->_computeDescriptorSets[i], 1, 0, 1, vk::DescriptorType::eStorageBuffer, {}, &storageBufferInfoCurrentFrame },
//         };
//
//         _device.updateDescriptorSets(descriptorWrites, {});
//     }
// }
//
// void Engine::createCommandBuffers(Renderer* const renderer) const {
//     const auto allocInfo = vk::CommandBufferAllocateInfo{
//         _graphicsCommandPool, vk::CommandBufferLevel::ePrimary, static_cast<uint32_t>(Renderer::MAX_FRAMES_IN_FLIGHT) };
//
//     renderer->_drawingCommandBuffers = _device.allocateCommandBuffers(allocInfo);
//     renderer->_computeCommandBuffers = _device.allocateCommandBuffers(allocInfo);
// }
//
// void Engine::createDrawingSyncObjects(Renderer *renderer) const {
//     renderer->_imageAvailableSemaphores.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//     renderer->_renderFinishedSemaphores.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//     renderer->_drawingInFlightFences.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//
//     for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
//         renderer->_imageAvailableSemaphores[i] = _device.createSemaphore(vk::SemaphoreCreateInfo{});
//         renderer->_renderFinishedSemaphores[i] = _device.createSemaphore(vk::SemaphoreCreateInfo{});
//         renderer->_drawingInFlightFences[i] = _device.createFence(vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled });
//     }
// }
//
// void Engine::createComputeSyncObjects(Renderer *renderer) const {
//     renderer->_computeFinishedSemaphores.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//     renderer->_computeInFlightFences.resize(Renderer::MAX_FRAMES_IN_FLIGHT);
//
//     for (size_t i = 0; i < Renderer::MAX_FRAMES_IN_FLIGHT; i++) {
//         renderer->_computeFinishedSemaphores[i] = _device.createSemaphore(vk::SemaphoreCreateInfo{});
//         renderer->_computeInFlightFences[i] = _device.createFence(vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled });
//     }
// }

void Engine::flushAndWait() const {
    _device.waitIdle();
}
