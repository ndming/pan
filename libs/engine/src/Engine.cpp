#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"

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
std::unique_ptr<Engine> Engine::create(Surface* const surface, const std::vector<Feature>& features) {
    return std::unique_ptr<Engine>(new Engine{ surface, features });
}

Engine::Engine(GLFWwindow* const window, const std::vector<Feature>& features) {
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
    const auto deviceFeatures = getPhysicalDeviceFeatures(features);
    auto deviceExtensions = std::vector(mDeviceExtensions.begin(), mDeviceExtensions.end());
    deviceExtensions.push_back(vk::KHRSwapchainExtensionName);   // to present to a surface
    _swapChain = new SwapChain{ window, _instance, deviceFeatures, deviceExtensions };

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

vk::PhysicalDeviceFeatures Engine::getPhysicalDeviceFeatures(const std::vector<Feature> &features) {
    auto deviceFeatures = vk::PhysicalDeviceFeatures{};

    for (const auto& feature : features) {
        switch (feature) {
            case Feature::SamplerAnisotropy:
                deviceFeatures.samplerAnisotropy = vk::True; break;
            case Feature::SampleRateShading:
                deviceFeatures.sampleRateShading = vk::True; break;
        }
    }

    return deviceFeatures;
}

void Engine::destroy() noexcept {
    delete _allocator;
    _allocator = nullptr;

    _device.destroyCommandPool(_transferCommandPool);

    _device.destroy(nullptr);

    delete _swapChain;
    _swapChain = nullptr;
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}


/* Swap chain */
SwapChain* Engine::createSwapChain() const {
    // Create a native Vulkan swap chain object and populate its resources
    _swapChain->init(_device, _allocator);
    return _swapChain;
}

void Engine::destroySwapChain(SwapChain* const swapChain) const noexcept {
    _device.destroyRenderPass(swapChain->_renderPass);
    swapChain->cleanup(_device);
    _instance.destroySurfaceKHR(swapChain->_surface);
    swapChain->_allocator = nullptr;
}

void Engine::destroyBuffer(const Buffer* const buffer) const noexcept {
    _allocator->destroyBuffer(buffer->getNativeBuffer(), static_cast<VmaAllocation>(buffer->getAllocation()));
    delete buffer;
}

void Engine::waitIdle() const {
    _device.waitIdle();
}

vk::Device Engine::getDevice() const {
    return _device;
}

vk::Queue Engine::getTransferQueue() const {
    return _transferQueue;
}

vk::CommandPool Engine::getTransferCommandPool() const {
    return _transferCommandPool;
}

ResourceAllocator* Engine::getResourceAllocator() const {
    return _allocator;
}
