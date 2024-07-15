#include <array>
#include <ranges>
#include <set>

#include <plog/Log.h>

#include "bootstrap/DebugMessenger.h"
#include "bootstrap/InstanceBuilder.h"
#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"
#include "bootstrap/RenderPassBuilder.h"
#include "bootstrap/SwapChainBuilder.h"

#include "ResourceAllocator.h"
#include "Translator.h"

#include "engine/Engine.h"


#ifndef NDEBUG
static constexpr std::array mValidationLayers{
    "VK_LAYER_KHRONOS_validation",
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

static constexpr std::array mDeviceExtensions{
    vk::KHRSwapchainExtensionName,         // to present to a surface
    vk::EXTMemoryBudgetExtensionName,      // to make our allocator estimate memory budget more accurately
    vk::EXTMemoryPriorityExtensionName,    // incorporate memory priority to the allocator
};

// We don't make the allocator a memeber of Engine because doing so would require the vma library to be public
static auto mAllocator = std::unique_ptr<ResourceAllocator>{};

std::unique_ptr<Engine> Engine::create() {
    return std::unique_ptr<Engine>(new Engine{});
}

Engine::Engine() {
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
    _debugMessenger = DebugMessenger::create(_instance, mCallback);
#endif
}

void Engine::destroy() const noexcept {
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}

void Engine::attachSurface(GLFWwindow* const window, const std::vector<DeviceFeature>& features) {
    if (_surface) {
        PLOG_ERROR << "Cannot attach a new surface onto an existing one, call Engine::detachSurface first";
        throw std::runtime_error("Failed to attach a surface: a surface is already existed");
    }

    // Register a framebuffer callback
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // Create a surface
    if (static_cast<vk::Result>(glfwCreateWindowSurface(
        _instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface))) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create a window surface!");
    }

    // Find a suitable GPU and create a logical device
    const auto deviceFeatures = Translator::toPhysicalDeviceFeatures(features);
    pickPhysicalDevice(deviceFeatures);
    createLogicalDevice(deviceFeatures);

    // Create a resource allocator
    mAllocator = ResourceAllocator::Builder()
        .flags(VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT)
        .vulkanApiVersion(VK_API_VERSION_1_2)
        .build(_instance, _physicalDevice, _device);
}

void Engine::framebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] const int w, [[maybe_unused]] const int h) {
    const auto engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine->_framebufferResized = true;
}

void Engine::pickPhysicalDevice(const vk::PhysicalDeviceFeatures& features) {
    // Find a list of possible candidate devices based on requested features
    const auto candidates = PhysicalDeviceSelector()
        .extensions(std::vector(mDeviceExtensions.begin(), mDeviceExtensions.end()))
        .features(features)
        .select(_instance.enumeratePhysicalDevices(), _surface);

    auto finder = QueueFamilyFinder()
        .requestPresentFamily(_surface)
        .requestComputeFamily(true);

    // Pick a physical device based on supported queue faimlies
    // Set out a fallback device in case we couldn't find a device supporting async compute
    auto fallbackCandidate = vk::PhysicalDevice{};
    for (const auto& candidate : candidates) {
        if (finder.find(candidate)) {
            _physicalDevice = candidate;
            break;
        }
        if (finder.completed(true)) {
            fallbackCandidate = candidate;
        }
        finder.reset();
    }
    if (_physicalDevice) {
        _graphicsFamily = finder.getGraphicsFamily();
        _presentFamily = finder.getPresentFamily();
        _computeFamily = finder.getComputeFamily();

#ifndef NDEBUG
        PLOG_DEBUG << "Graphics queue family index: " << _graphicsFamily.value();
        PLOG_DEBUG << "Present queue family index: " << _presentFamily.value();
        PLOG_DEBUG << "Compute queue family index: " << _computeFamily.value();
#endif

        const auto properties = _physicalDevice.getProperties();
        PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
        PLOG_INFO << "Detected async compute capability";
    } else if (fallbackCandidate) {
        _physicalDevice = fallbackCandidate;
        // Find queue families again since we didn't break out early when we set fallback candidate
        finder.find(fallbackCandidate);
        _graphicsFamily = finder.getGraphicsFamily();
        _presentFamily = finder.getPresentFamily();

#ifndef NDEBUG
        PLOG_DEBUG << "Graphics queue family index: " << _graphicsFamily.value();
        PLOG_DEBUG << "Present queue family index: " << _presentFamily.value();
#endif

        const auto properties = _physicalDevice.getProperties();
        PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
    } else {
        PLOG_ERROR << "Could not find a suitable GPU: try requesting less features or updating your driver";
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void Engine::createLogicalDevice(const vk::PhysicalDeviceFeatures& features) {
    auto uniqueFamilies = std::set{ _graphicsFamily.value(), _presentFamily.value() };
    if (_computeFamily.has_value()) {
        uniqueFamilies.insert(_computeFamily.value());
    }
    constexpr auto queuePriority = 1.0f;

    using namespace std::ranges::views;
    const auto infoViews = uniqueFamilies | transform([&queuePriority](const auto& family) {
        return vk::DeviceQueueCreateInfo{ {}, family, 1, &queuePriority };
    });
    const auto queueCreateInfos = std::vector(infoViews.begin(), infoViews.end());

    auto deviceCreateInfo = vk::DeviceCreateInfo{};
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pEnabledFeatures = &features;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(mDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = mDeviceExtensions.data();

#ifndef NDEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(mValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = mValidationLayers.data();
#else
    deviceCreateInfo.enabledLayerCount = 0;
#endif

    _device = _physicalDevice.createDevice(deviceCreateInfo);
    _graphicsQueue = _device.getQueue(_graphicsFamily.value(), 0);
    _presentQueue = _device.getQueue(_presentFamily.value(), 0);
    if (_computeFamily.has_value()) {
        _computeQueue = _device.getQueue(_computeFamily.value(), 0);
    }
}

void Engine::detachSurface() const noexcept {
    if (!_surface) {
        PLOG_WARNING << "Cannot detach a surface without having attached any, call Engine::attachSurface first";
        return;
    }
    mAllocator.reset(nullptr);
    _device.destroy(nullptr);
    _instance.destroySurfaceKHR(_surface);
}

SwapChain* Engine::createSwapChain(GLFWwindow* const window, const SwapChain::MSAA msaa) const {
    if (!_surface) {
        PLOG_ERROR << "Cannot create a swap chain without having attached a surface, use Engine::attachSurface first";
        throw std::runtime_error("Failed to create a swap chain: surface is not initialized");
    }

    const auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
    const auto formats = _physicalDevice.getSurfaceFormatsKHR(_surface);
    const auto presentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

    const auto surfaceFormat = SwapChain::chooseSwapSurfaceFormat(formats);
    const auto presentMode = SwapChain::chooseSwapPresentMode(presentModes);
    const auto extent = SwapChain::chooseSwapExtent(capabilities, window);

    auto minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount) {
        minImageCount = capabilities.maxImageCount;
    }

    return SwapChainBuilder(_graphicsFamily.value(), _presentFamily.value())
        .surfaceFormat(surfaceFormat)
        .presentMode(presentMode)
        .extent(extent)
        .minImageCount(minImageCount)
        .imageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .preTransform(capabilities.currentTransform)
        .sampleCount(Translator::toSupportSampleCount(msaa, _physicalDevice))
        .compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .clipped(vk::True)
        .build(_device, _surface, mAllocator.get());
}

void Engine::destroySwapChain(SwapChain* const swapChain) const noexcept {
    _device.destroyImageView(swapChain->_colorImageView);
    mAllocator->destroyImage(swapChain->_colorImage, static_cast<VmaAllocation>(swapChain->_colorImageAllocation));

    using namespace std::ranges;
    for_each(swapChain->_framebuffers, [this](const auto& it) { _device.destroyFramebuffer(it); });
    for_each(swapChain->_imageViews, [this](const auto& it) { _device.destroyImageView(it); });

    _device.destroySwapchainKHR(swapChain->_nativeObject);
    delete swapChain;
}

Renderer* Engine::createRenderer(const SwapChain* const swapChain, const Renderer::Pipeline pipeline) const {
    const auto renderer = new Renderer{};

    renderer->_renderPass = RenderPassBuider(swapChain->_imageFormat.format)
        .sampleCount(swapChain->_msaa)
        .build(_device);

    return renderer;
}

void Engine::destroyRenderer(Renderer* const renderer) const noexcept {
    _device.destroyRenderPass(renderer->_renderPass);

    delete renderer;
}

void Engine::flushAndWait() const {
    _device.waitIdle();
}
