#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"

#include "bootstrap/DeviceBuilder.h"
#include "bootstrap/InstanceBuilder.h"

#include <array>
#include <ranges>
#include <set>


#ifndef NDEBUG
#include "bootstrap/DebugMessenger.h"
#include <plog/Log.h>

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
    vk::EXTMemoryBudgetExtensionName,             // to make our allocator estimate memory budget more accurately
    vk::EXTMemoryPriorityExtensionName,           // incorporate memory priority to the allocator
    vk::EXTVertexInputDynamicStateExtensionName,  // allow the vertex binding and attribute to be changed dynamically
    vk::EXTExtendedDynamicState3ExtensionName,    // dynamic polygon mode, msaa, and depth clamp
};


std::unique_ptr<Engine> Engine::create(Surface* const surface, const EngineFeature& feature) {
    return std::unique_ptr<Engine>(new Engine{ surface, feature });
}

Engine::Engine(GLFWwindow* const window, const EngineFeature& feature) {
    // Create a Vulkan instance
    _instance = InstanceBuilder()
        .applicationName("pan")
        .applicationVersion(1, 0, 0)
        .apiVersion(1, 3, 0)
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
    auto deviceExtensions = std::vector(mDeviceExtensions.begin(), mDeviceExtensions.end());
    deviceExtensions.push_back(vk::KHRSwapchainExtensionName);   // to present to a surface
    try {
        _swapChain = new SwapChain{ window, _instance, feature, deviceExtensions };
    } catch (const std::exception&) {
#ifndef NDEBUG
        DebugMessenger::destroy(_instance, _debugMessenger);
#endif
        _instance.destroy(nullptr);
        throw;
    }

    // We need to have multiple VkDeviceQueueCreateInfo structs to create a queue from multiple families.
    // An elegant way to do that is to create a set of all unique queue families that are necessary
    // for the required queues
    auto uniqueFamilies = std::set{ _swapChain->_graphicsFamily.value(), _swapChain->_presentFamily.value() };
    if (_swapChain->_computeFamily.has_value()) {
        uniqueFamilies.insert(_swapChain->_computeFamily.value());
    }

    // Set up a logical device to interface with the selected physical device. We can create multiple logical devices
    // from the same physical device if we have varying requirements
    const auto deviceFeatures = getPhysicalDeviceFeatures(feature);
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
        .vulkanApiVersion(VK_API_VERSION_1_3)
        .build(_instance, _swapChain->_physicalDevice, _device);

    // The physical device features structure were dynamically allocated
    cleanupPhysicalDeviceFeatures(deviceFeatures);
    _feature = feature;
}

vk::PhysicalDeviceFeatures2 Engine::getPhysicalDeviceFeatures(const EngineFeature& feature) {
    // Basic features
    auto basicFeatures = vk::PhysicalDeviceFeatures{};
    if (feature.depthClamp) {
        basicFeatures.depthClamp = vk::True;
    }
    if (feature.largePoints) {
        basicFeatures.largePoints = vk::True;
    }
    if (feature.sampleShading) {
        basicFeatures.sampleRateShading = vk::True;
    }
    if (feature.wideLines) {
        basicFeatures.wideLines = vk::True;
    }

    // Vertex input dynamic state features
    auto vertexInputDynamicStateFeatures = new vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT{};
    vertexInputDynamicStateFeatures->vertexInputDynamicState = vk::True;

    // Extended dynamic state features: cull mode, front face, primitive topology
    auto extendedDynamicStateFeatures = new vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{};
    extendedDynamicStateFeatures->extendedDynamicState = vk::True;

    // Extended dynamic state 2 features: primitive restart
    auto extendedDynamicState2Features = new vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    extendedDynamicState2Features->extendedDynamicState2 = vk::True;

    // Extended dynamic state 3 features
    auto extendedDynamicState3Features = new vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT{};
    extendedDynamicState3Features->extendedDynamicState3PolygonMode = vk::True;  // explicitly required by the Engine
    if (feature.depthClamp) {
        extendedDynamicState3Features->extendedDynamicState3DepthClampEnable = vk::True;
    }
    if (feature.msaa) {
        extendedDynamicState3Features->extendedDynamicState3RasterizationSamples = vk::True;
    }

    // Any update to this feature chain must also get updated in the PhysicalDeviceSelector::checkFeatureSupport method
    auto deviceFeatures = vk::PhysicalDeviceFeatures2{};
    deviceFeatures.features = basicFeatures;
    deviceFeatures.pNext = vertexInputDynamicStateFeatures;
    vertexInputDynamicStateFeatures->pNext = extendedDynamicStateFeatures;
    extendedDynamicStateFeatures->pNext = extendedDynamicState2Features;
    extendedDynamicState2Features->pNext = extendedDynamicState3Features;

    return deviceFeatures;
}

void Engine::cleanupPhysicalDeviceFeatures(const vk::PhysicalDeviceFeatures2& deviceFeatures) {
    const auto vertexInputDynamicStateFeatures = static_cast<vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT*>(deviceFeatures.pNext);
    const auto extendedDynamicStateFeatures = static_cast<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT*>(vertexInputDynamicStateFeatures->pNext);
    const auto extendedDynamicState2Features = static_cast<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT*>(extendedDynamicStateFeatures->pNext);
    const auto extendedDynamicState3Features = static_cast<vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT*>(extendedDynamicState2Features->pNext);

    delete extendedDynamicState3Features;
    delete extendedDynamicState2Features;
    delete extendedDynamicStateFeatures;
    delete vertexInputDynamicStateFeatures;
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

void Engine::destroyShader(std::unique_ptr<Shader>& shader) const noexcept {
    _device.destroyPipeline(shader->getNativePipeline());
    _device.destroyPipelineLayout(shader->getNativePipelineLayout());
    _device.destroyDescriptorSetLayout(shader->getNativeDescriptorSetLayout());
    shader.reset(nullptr);
}


void Engine::waitIdle() const {
    _device.waitIdle();
}


const EngineFeature & Engine::getEngineFeature() const {
    return _feature;
}

vk::Device Engine::getNativeDevice() const {
    return _device;
}

vk::Queue Engine::getNativeTransferQueue() const {
    return _transferQueue;
}

vk::CommandPool Engine::getNativeTransferCommandPool() const {
    return _transferCommandPool;
}

ResourceAllocator* Engine::getResourceAllocator() const {
    return _allocator;
}
