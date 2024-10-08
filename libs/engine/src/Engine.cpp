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
        _swapChain = std::make_shared<SwapChain>(window, _instance, feature, deviceExtensions);
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
    // with VK_QUEUE_GRAPHICS_BIT capabilities already supports VK_QUEUE_TRANSFER_BIT operations implicitly.
    // We specify the eTransient flag because memory transfer operations involve using short-lived buffers.
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
    basicFeatures.largePoints = vk::True;       // for gl_PointSize in vertex shader
    basicFeatures.wideLines = vk::True;         // for custom line width
    basicFeatures.fillModeNonSolid = vk::True;  // for custom polygon mode
    if (feature.sampleShading) {
        basicFeatures.sampleRateShading = vk::True;
    }
    if (feature.samplerAnisotropy) {
        basicFeatures.samplerAnisotropy = vk::True;
    }

    // Vertex input dynamic state features
    auto vertexInputDynamicStateFeatures = new vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT{};
    vertexInputDynamicStateFeatures->vertexInputDynamicState = vk::True;

    // Explicitly required by the application
    auto descriptorIndexingFeatures = new vk::PhysicalDeviceDescriptorIndexingFeatures{};
    descriptorIndexingFeatures->descriptorBindingVariableDescriptorCount = vk::True;

    // Extended dynamic state features: cull mode, front face, primitive topology
    auto extendedDynamicStateFeatures = new vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{};
    extendedDynamicStateFeatures->extendedDynamicState = vk::True;

    // Extended dynamic state 2 features: primitive restart
    auto extendedDynamicState2Features = new vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    extendedDynamicState2Features->extendedDynamicState2 = vk::True;

    // Extended dynamic state 3 features
    auto extendedDynamicState3Features = new vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT{};
    extendedDynamicState3Features->extendedDynamicState3PolygonMode = vk::True;  // explicitly required by the Engine

    // Any update to this feature chain must also get updated in the PhysicalDeviceSelector::checkFeatureSupport method
    auto deviceFeatures = vk::PhysicalDeviceFeatures2{};
    deviceFeatures.features = basicFeatures;
    deviceFeatures.pNext = vertexInputDynamicStateFeatures;
    vertexInputDynamicStateFeatures->pNext = descriptorIndexingFeatures;
    descriptorIndexingFeatures->pNext = extendedDynamicStateFeatures;
    extendedDynamicStateFeatures->pNext = extendedDynamicState2Features;
    extendedDynamicState2Features->pNext = extendedDynamicState3Features;

    return deviceFeatures;
}

void Engine::cleanupPhysicalDeviceFeatures(const vk::PhysicalDeviceFeatures2& deviceFeatures) {
    const auto vertexInputDynamicStateFeatures = static_cast<vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT*>(deviceFeatures.pNext);
    const auto descriptorIndexingFeatures = static_cast<vk::PhysicalDeviceDescriptorIndexingFeatures*>(vertexInputDynamicStateFeatures->pNext);
    const auto extendedDynamicStateFeatures = static_cast<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT*>(descriptorIndexingFeatures->pNext);
    const auto extendedDynamicState2Features = static_cast<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT*>(extendedDynamicStateFeatures->pNext);
    const auto extendedDynamicState3Features = static_cast<vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT*>(extendedDynamicState2Features->pNext);

    delete extendedDynamicState3Features;
    delete extendedDynamicState2Features;
    delete extendedDynamicStateFeatures;
    delete descriptorIndexingFeatures;
    delete vertexInputDynamicStateFeatures;
}

void Engine::destroy() noexcept {
    delete _allocator;
    _allocator = nullptr;

    _device.destroyCommandPool(_transferCommandPool);

    _device.destroy(nullptr);

    _swapChain.reset();
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}


std::shared_ptr<SwapChain> Engine::createSwapChain(const SwapChain::MSAA level) const {
    // Create a native Vulkan swap chain object and populate its resources
    _swapChain->init(_device, _allocator, level);
    return _swapChain;
}

void Engine::destroySwapChain(const std::shared_ptr<SwapChain>& swapChain) const noexcept {
    _device.destroyRenderPass(swapChain->_renderPass);
    swapChain->cleanup(_device);
    _instance.destroySurfaceKHR(swapChain->_surface);
    swapChain->_allocator = nullptr;
}


std::unique_ptr<Renderer> Engine::createRenderer() const {
    // We will be recording a command buffer every frame, so we want to be able to reset and re-record over it
    const auto graphicsCommandPool = _device.createCommandPool(
        { vk::CommandPoolCreateFlagBits::eResetCommandBuffer, _swapChain->getGraphicsQueueFamily() });
    const auto graphicsQueue = _device.getQueue(_swapChain->getGraphicsQueueFamily(), 0);
    const auto func = reinterpret_cast<PFN_vkCmdSetPolygonModeEXT>(vkGetInstanceProcAddr(_instance, "vkCmdSetPolygonModeEXT"));
    return std::unique_ptr<Renderer>(new Renderer{ graphicsCommandPool, graphicsQueue, _device, func });
}

void Engine::destroyRenderer(const std::unique_ptr<Renderer>& renderer) const noexcept {
    using namespace std::ranges;
    for_each(renderer->_drawingFences, [this](const auto& it) { _device.destroyFence(it); });
    for_each(renderer->_renderFinishedSemaphores, [this](const auto& it) { _device.destroySemaphore(it); });
    for_each(renderer->_imageAvailableSemaphores, [this](const auto& it) { _device.destroySemaphore(it); });
    _device.destroyCommandPool(renderer->_graphicsCommandPool);
}


void Engine::destroyBuffer(const Buffer* const buffer) const noexcept {
    _allocator->destroyBuffer(buffer->getNativeBuffer(), static_cast<VmaAllocation>(buffer->getAllocation()));
    delete buffer;
}

void Engine::destroyImage(const std::shared_ptr<Image>& image) const noexcept {
    _device.destroyImageView(image->getNativeImageView());
    _allocator->destroyImage(image->getNativeImage(), static_cast<VmaAllocation>(image->getAllocation()));
}

void Engine::destroySampler(const std::unique_ptr<Sampler>& sampler) const noexcept {
    _device.destroySampler(sampler->getNativeSampler());
}

void Engine::destroyShader(const Shader* const shader) const noexcept {
    _device.destroyPipeline(shader->getNativePipeline());
    _device.destroyPipelineLayout(shader->getNativePipelineLayout());
    _device.destroyDescriptorSetLayout(shader->getNativeDescriptorSetLayout());
    delete shader;
}

void Engine::destroyShaderInstance(const ShaderInstance* const instance) const noexcept {
    _device.destroyDescriptorPool(instance->getNativeDescriptorPool());
    delete instance;
}


void Engine::waitIdle() const {
    _device.waitIdle();
}


const EngineFeature & Engine::getEngineFeature() const {
    return _feature;
}

uint32_t Engine::getLimitPushConstantSize() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxPushConstantsSize;
}

float Engine::getLimitMaxSamplerAnisotropy() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxSamplerAnisotropy;
}

uint32_t Engine::getLimitMinUniformBufferOffsetAlignment() const {
    return _swapChain->_physicalDevice.getProperties().limits.minUniformBufferOffsetAlignment;
}

uint32_t Engine::getLimitMinStorageBufferOffsetAlignment() const {
    return _swapChain->_physicalDevice.getProperties().limits.minStorageBufferOffsetAlignment;
}

uint32_t Engine::getLimitMaxUniformBufferRange() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxUniformBufferRange;
}

uint32_t Engine::getLimitMaxStorageBufferRange() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxStorageBufferRange;
}

uint32_t Engine::getLimitMaxPerStageDescriptorUniformBuffers() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxPerStageDescriptorUniformBuffers;
}

uint32_t Engine::getLimitMaxPerStageDescriptorStorageBuffers() const {
    return _swapChain->_physicalDevice.getProperties().limits.maxPerStageDescriptorStorageBuffers;
}

vk::Instance Engine::getNativeInstance() const {
    return _instance;
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
