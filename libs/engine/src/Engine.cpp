#include <array>
#include <ranges>
#include <set>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <plog/Log.h>

#include "bootstrap/DebugMessenger.h"
#include "bootstrap/InstanceBuilder.h"
#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"
#include "bootstrap/Translator.h"

#include "external/VmaUsage.h"

#include "engine/Engine.h"


#ifndef NDEBUG
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
#ifndef NDEBUG
        .layers(pValidationLayers.data(), pValidationLayers.size())
#endif
        .build();

#ifndef NDEBUG
    _debugMessenger = DebugMessenger::create(_instance);
#endif
}

void Engine::destroy() const {
#ifndef NDEBUG
    DebugMessenger::destroy(_instance, _debugMessenger);
#endif
    _instance.destroy(nullptr);
}

void Engine::attachSurface(void* const nativeWindow, const std::vector<DeviceFeature>& features) {
    // Register a framebuffer callback
    const auto window = static_cast<GLFWwindow*>(nativeWindow);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    // Create a surface
    if (static_cast<vk::Result>(glfwCreateWindowSurface(
        _instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface))) != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to create window surface!");
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
        throw std::runtime_error("Could not find a suitable GPU!");
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

#ifndef NDEBUG
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

void Engine::detachSurface() const {
    vmaDestroyAllocator(pAllocator);
    _device.destroy(nullptr);
    _instance.destroySurfaceKHR(_surface);
}

std::unique_ptr<SwapChain> Engine::createSwapChain(void* const nativeWindow) const {
    const auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
    const auto formats = _physicalDevice.getSurfaceFormatsKHR(_surface);
    const auto presentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

    const auto swapChain = new SwapChain{ capabilities, formats, presentModes, nativeWindow };
    return std::unique_ptr<SwapChain>(swapChain);
}

void Engine::destroySwapChain(std::unique_ptr<SwapChain> swapChain) {

}
