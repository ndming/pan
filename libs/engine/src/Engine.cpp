#include <array>
#include <ranges>
#include <set>

#include <plog/Log.h>

#include "bootstrap/DebugMessenger.h"
#include "bootstrap/InstanceBuilder.h"
#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"
#include "bootstrap/Translator.h"

#include "external/VmaUsage.h"

#include "engine/Engine.h"


#ifdef NDEBUG
static constexpr std::array pValidationLayers{
    "VK_LAYER_KHRONOS_validation",
};
#endif

static constexpr std::array pDeviceExtensions{
    vk::KHRSwapchainExtensionName,         // to present to a surface
    vk::EXTMemoryBudgetExtensionName,      // to make our allocator estimate memory budget more accurately
    vk::EXTMemoryPriorityExtensionName,    // incorporate memory priority to the allocator
};

// We don't make the allocator a memeber of Engine because doing so would require the vma library to be public
static VmaAllocator pAllocator{};

std::unique_ptr<Engine> Engine::create() {
    return std::unique_ptr<Engine>(new Engine{});
}

Engine::Engine() {
    _instance = InstanceBuilder()
        .applicationName("pan")
        .applicationVersion(1, 0, 0)
        .apiVersion(1, 2, 0)
#ifdef NDEBUG
        .layers(pValidationLayers.data(), pValidationLayers.size())
#endif
        .build();

#ifdef NDEBUG
    _debugMessenger = DebugMessenger::create(_instance);
#endif
}

void Engine::destroy() const noexcept {
#ifdef NDEBUG
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

    // Find a suitable GPU and create a logical device and an allocator
    const auto deviceFeatures = Translator::toPhysicalDeviceFeatures(features);
    pickPhysicalDevice(deviceFeatures);
    createLogicalDevice(deviceFeatures);
    createAllocator();
}

void Engine::framebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] const int w, [[maybe_unused]] const int h) {
    const auto engine = static_cast<Engine*>(glfwGetWindowUserPointer(window));
    engine->_framebufferResized = true;
}

void Engine::pickPhysicalDevice(const vk::PhysicalDeviceFeatures& features) {
    // Find a list of possible candidate devices based on requested features
    const auto candidates = PhysicalDeviceSelector()
        .extensions(std::vector(pDeviceExtensions.begin(), pDeviceExtensions.end()))
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

        const auto properties = _physicalDevice.getProperties();
        PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
        PLOG_INFO << "Detected async compute capability";
    } else if (fallbackCandidate) {
        _physicalDevice = fallbackCandidate;
        // Find queue families again since we didn't break out early when we set fallback candidate
        finder.find(fallbackCandidate);
        _graphicsFamily = finder.getGraphicsFamily();
        _presentFamily = finder.getPresentFamily();

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
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(pDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = pDeviceExtensions.data();

#ifdef NDEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(pValidationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = pValidationLayers.data();
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

void Engine::createAllocator() const {
    VmaVulkanFunctions vulkanFunctions = {};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT | VMA_ALLOCATOR_CREATE_EXT_MEMORY_PRIORITY_BIT;
    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
    allocatorCreateInfo.physicalDevice = _physicalDevice;
    allocatorCreateInfo.device = _device;
    allocatorCreateInfo.instance = _instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    vmaCreateAllocator(&allocatorCreateInfo, &pAllocator);
}

void Engine::detachSurface() const noexcept {
    if (!_surface) {
        PLOG_WARNING << "Cannot detach a surface without having attached any, call Engine::attachSurface first";
        return;
    }
    vmaDestroyAllocator(pAllocator);
    _device.destroy(nullptr);
    _instance.destroySurfaceKHR(_surface);
}

std::unique_ptr<SwapChain> Engine::createSwapChain(GLFWwindow* const window) const {
    if (!_surface) {
        PLOG_ERROR << "Cannot create a swap chain without having attached a surface, use Engine::attachSurface first";
        throw std::runtime_error("Failed to create a swap chain: surface is not initialized");
    }

    const auto swapChain = initializeSwapChain(window);
    createSwapChainImageViews(swapChain);

    return std::unique_ptr<SwapChain>(swapChain);
}

SwapChain * Engine::initializeSwapChain(GLFWwindow* const window) const {
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

    auto createInfo = vk::SwapchainCreateInfoKHR{};
    createInfo.surface = _surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.presentMode = presentMode;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    if (_graphicsFamily.value() != _presentFamily.value()) {
        const uint32_t queueFamilyIndices[]{ _graphicsFamily.value(), _presentFamily.value() };
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.clipped = vk::True;
    createInfo.oldSwapchain = nullptr;

    const auto swapChain = new SwapChain{};
    swapChain->_swapChain = _device.createSwapchainKHR(createInfo);
    swapChain->_swapChainImages = _device.getSwapchainImagesKHR(swapChain->_swapChain);
    swapChain->_swapChainImageFormat = surfaceFormat.format;
    swapChain->_swapChainImageExtent = extent;

    return swapChain;
}

void Engine::createSwapChainImageViews(SwapChain* const swapChain) const {
    using namespace std::ranges;
    const auto toImageView = [this, swapChain](const auto& image) {
        constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;
        constexpr auto mipLevels = 1;
        const auto viewInfo = vk::ImageViewCreateInfo{
            {}, image, vk::ImageViewType::e2D, swapChain->_swapChainImageFormat, {}, { aspectFlags, 0, mipLevels, 0, 1 } };
        return _device.createImageView(viewInfo);
    };
    const auto imageViews = swapChain->_swapChainImages | views::transform(toImageView);
    swapChain->_swapChainImageViews = std::vector(imageViews.begin(), imageViews.end());
}

void Engine::destroySwapChain(std::unique_ptr<SwapChain>&& swapChain) const noexcept {
    using namespace std::ranges;
    for_each(swapChain->_swapChainImageViews, [this](const auto& it) { _device.destroyImageView(it); });

    _device.destroySwapchainKHR(swapChain->_swapChain);
}
