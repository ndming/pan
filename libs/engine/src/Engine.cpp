#include "engine/Engine.h"

#include "engine/Context.h"
#include "engine/SwapChain.h"

#include "ResourceAllocator.h"
#include "Translator.h"

#ifndef NDEBUG
#include "bootstrap/DebugMessenger.h"
#endif

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


/* Engine initialization and destruction */
std::unique_ptr<Engine> Engine::create(const std::unique_ptr<Context>& context, const std::vector<Feature>& features) {
    return std::unique_ptr<Engine>(new Engine{ context, features });
}

Engine::Engine(const std::unique_ptr<Context>& context, const std::vector<Feature>& features) {
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

    // Have the swap chain create a surface and pick the physical device according to its need
    const auto deviceFeatures = Translator::toPhysicalDeviceFeatures(features);
    auto deviceExtensions = std::vector(mDeviceExtensions.begin(), mDeviceExtensions.end());
    deviceExtensions.push_back(vk::KHRSwapchainExtensionName);   // to present to a surface
    _swapChain = std::unique_ptr<SwapChain>(new SwapChain{ context->_window, _instance, deviceFeatures, deviceExtensions });

    // We need to have multiple VkDeviceQueueCreateInfo structs to create a queue from multiple families.
    // An elegant way to do that is to create a set of all unique queue families that are necessary
    // for the required queues
    auto uniqueFamilies = std::set{ _swapChain->_graphicsFamily.value(), _swapChain->_presentFamily.value() };
    if (_swapChain->_computeFamily.has_value()) {
        uniqueFamilies.insert(_swapChain->_computeFamily.value());
    }

    // Set up a logical device to interface with the selected physical device. We can create multiple logical devices
    // from the same physical device if we have varying requirements
    _device = DeviceBuilder()
        .queueFamilies(uniqueFamilies)
        .deviceFeatures(deviceFeatures)
        .deviceExtensions(deviceExtensions)
#ifndef NDEBUG
        .validationLayers({ mValidationLayers.begin(), mValidationLayers.end() })
#endif
        .build(_swapChain->_physicalDevice);

    // We need a transfer queue and a transfer command pool to transfer image and buffer data. Any queue family
    // with VK_QUEUE_GRAPHICS_BIT capabilities already implicitly support VK_QUEUE_TRANSFER_BIT operations.
    _transferQueue = _device.getQueue(_swapChain->_graphicsFamily.value(), 0);
    const auto transferPoolInfo = vk::CommandPoolCreateInfo{
        vk::CommandPoolCreateFlagBits::eTransient, _swapChain->_graphicsFamily.value() };
    _transferCommandPool = _device.createCommandPool(transferPoolInfo);

    // Create a resource allocator
    _allocator = ResourceAllocator::Builder()
        .flags(VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT)
        .vulkanApiVersion(VK_API_VERSION_1_2)
        .build(_instance, _swapChain->_physicalDevice, _device);
}

void Engine::destroy() noexcept {
    delete _allocator;
    _allocator = nullptr;
    _device.destroyCommandPool(_transferCommandPool);
    _device.destroy(nullptr);
    _swapChain.reset(nullptr);
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}


/* Swap chain */
SwapChain* Engine::createSwapChain() const {
    // Create a native Vulkan swap chain object and populate its resources
    _swapChain->initSwapChain(_device, _allocator);
    return _swapChain.get();
}

void Engine::destroySwapChain(SwapChain* const swapChain) const noexcept {
    _device.destroyRenderPass(swapChain->_renderPass);
    swapChain->cleanup(_device);
    _instance.destroySurfaceKHR(swapChain->_surface);
    swapChain->_allocator = nullptr;
}

vk::CommandBuffer Engine::beginSingleTimeTransferCommands() const {
    const auto allocInfo = vk::CommandBufferAllocateInfo{ _transferCommandPool, vk::CommandBufferLevel::ePrimary, 1 };
    const auto commandBuffer = _device.allocateCommandBuffers(allocInfo)[0];

    // Use the command buffer once and wait with returning from the function until the copy operation has finished
    constexpr auto beginInfo = vk::CommandBufferBeginInfo{ vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
    commandBuffer.begin(beginInfo);

    return commandBuffer;
}

void Engine::endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer) const {
    commandBuffer.end();
    _transferQueue.submit(vk::SubmitInfo{ {}, {}, {}, 1, &commandBuffer });
    _transferQueue.waitIdle();
    _device.freeCommandBuffers(_transferCommandPool, 1, &commandBuffer);
}

void Engine::createDeviceBuffer(const std::size_t bufferSize, const vk::BufferUsageFlags usage, vk::Buffer& buffer, void** allocation) const {
    auto vmaAllocation = VmaAllocation{};
    buffer = _allocator->allocateDeviceBuffer(bufferSize, static_cast<VkBufferUsageFlags>(usage), &vmaAllocation);
    *allocation = vmaAllocation;
}

void Engine::transferBufferData(const std::size_t bufferSize, const void* const data, const vk::Buffer& buffer, const vk::DeviceSize offset) const {
    auto stagingAllocation = VmaAllocation{};
    const auto stagingBuffer = _allocator->allocateStagingBuffer(bufferSize, &stagingAllocation);

    _allocator->mapData(bufferSize, data, stagingAllocation);
    const auto commandBuffer = beginSingleTimeTransferCommands();
    commandBuffer.copyBuffer(stagingBuffer, buffer, vk::BufferCopy{ 0, offset, bufferSize });
    endSingleTimeTransferCommands(commandBuffer);

    _allocator->destroyBuffer(stagingBuffer, stagingAllocation);
}

void Engine::destroyVertexBuffer(VertexBuffer* const buffer) const noexcept {
    _allocator->destroyBuffer(buffer->_buffer, static_cast<VmaAllocation>(buffer->allocation));
    buffer->allocation = nullptr;  // a bit redundant, but this prevents the IDE from suggesting making pointer to const
    delete buffer;
}

void Engine::destroyIndexBuffer(IndexBuffer *buffer) const noexcept {
    _allocator->destroyBuffer(buffer->_buffer, static_cast<VmaAllocation>(buffer->allocation));
    buffer->allocation = nullptr;  // a bit redundant, but this prevents the IDE from suggesting making pointer to const
    delete buffer;
}

void Engine::waitIdle() const {
    _device.waitIdle();
}
