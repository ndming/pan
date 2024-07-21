#include <GLFW/glfw3.h>

#ifndef NDEBUG
#include <functional>
#include <unordered_set>
#include <ranges>
#endif

#include "InstanceBuilder.h"


InstanceBuilder& InstanceBuilder::applicationName(std::string name) {
    _applicationName = std::move(name);
    return *this;
}

InstanceBuilder & InstanceBuilder::applicationVersion(const int major, const int minor, const int patch) {
    _applicationVersion = vk::makeApiVersion(0, major, minor, patch);
    return *this;
}

InstanceBuilder& InstanceBuilder::apiVersion(const int major, const int minor, const int patch) {
    _apiVersion = vk::makeApiVersion(0, major, minor, patch);
    return *this;
}

InstanceBuilder & InstanceBuilder::layers(const char* const* layers, const std::size_t count) {
    _layers = std::vector(layers, layers + count);
    return *this;
}

InstanceBuilder & InstanceBuilder::callback(PFN_vkDebugUtilsMessengerCallbackEXT callback) {
    _callback = callback;
    return *this;
}

vk::Instance InstanceBuilder::build() const {
    // Application info
    const auto appInfo = vk::ApplicationInfo{
        _applicationName.data(), _applicationVersion, "None", _apiVersion, _apiVersion };

#ifndef NDEBUG
    // Check if all required validation layers are available, throw if any of them is not supported
    using namespace std::ranges;
    const auto properties = vk::enumerateInstanceLayerProperties();

    const auto toName = [](const auto& property) { return property.layerName.data(); };
    const auto supportedLayers = properties | views::transform(toName) | to<std::unordered_set<std::string>>();

    if (const auto supported = [&supportedLayers](const auto& it) { return supportedLayers.contains(it); };
        any_of(_layers, std::logical_not{}, supported)) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }
#endif

    // Tell the Vulkan driver which global extensions and validation layers we want to use
    // Global here means that they apply to the entire program and not a specific device
    auto createInfo = vk::InstanceCreateInfo{};
    createInfo.pApplicationInfo = &appInfo;

    // Vulkan is a platform-agnostic API, which means that we need an extension to interface with the window system
    uint32_t glfwExtensionCount{ 0 };
    const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    auto requiredExtensions = std::vector(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifndef NDEBUG
    requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

#ifdef __APPLE__
    // Beginning with the 1.3.216 Vulkan SDK, the VK_KHR_PORTABILITY_subset extension is mandatory
    // for macOS with the latest MoltenVK SDK
    requiredExtensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
    createInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

#ifndef NDEBUG
    // Enable validation layers
    createInfo.enabledLayerCount = static_cast<uint32_t>(_layers.size());
    createInfo.ppEnabledLayerNames = _layers.data();

    using MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    const auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{ {},
        MessageSeverity::eVerbose | MessageSeverity::eWarning | MessageSeverity::eError,
        MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        _callback, nullptr };
    // To have the debug messenger account for the creation and destruction of the Vulkan instance, pass the
    // debug messenger to pNext to receive debugging messages for the creation and destruction of VkInstance
    createInfo.pNext = &debugCreateInfo;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
#endif

    return vk::createInstance(createInfo);
}
