#include <functional>
#include <ranges>
#include <unordered_set>

#include <plog/Log.h>

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

const std::vector<const char*> InstanceBuilder::VALIDATION_LAYERS{
    "VK_LAYER_KHRONOS_validation",
};

vk::Instance InstanceBuilder::build(const vk::InstanceCreateFlags flags) const {
    // Check for layer support
    using namespace std::ranges;
    const auto properties = vk::enumerateInstanceLayerProperties();
    const auto toName = [](const auto& property) { return property.layerName.data(); };
    const auto supportedLayers = properties | views::transform(toName) | to<std::unordered_set>();

    if (const auto supported = [&supportedLayers](const auto& it) { return supportedLayers.contains(it); };
        any_of(VALIDATION_LAYERS, std::logical_not{}, supported)) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }

    // Application info
    const auto appInfo = vk::ApplicationInfo{
        _applicationName.data(), _applicationVersion, "None", _apiVersion, _apiVersion };

    // Instance create info
    auto createInfo = vk::InstanceCreateInfo{};
    createInfo.sType = vk::StructureType::eInstanceCreateInfo;
    createInfo.flags = flags;
    createInfo.pApplicationInfo = &appInfo;

    // Required extensions
    uint32_t glfwExtensionCount{ 0 };
    const auto glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    auto requiredExtensions = std::vector(glfwExtensions, glfwExtensions + glfwExtensionCount);

#ifndef NDEBUG
    requiredExtensions.push_back(vk::EXTDebugUtilsExtensionName);
#endif

#ifdef __APPLE__
    requiredExtensions.push_back(vk::KHRPortabilityEnumerationExtensionName);
    createInfo.flags |= vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#endif

    createInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();

#ifndef NDEBUG
    createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
    createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

    using MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    const auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{ {},
        MessageSeverity::eVerbose | MessageSeverity::eWarning | MessageSeverity::eError,
        MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        debugCallback, nullptr
    };
    createInfo.pNext = &debugCreateInfo;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
#endif

    return vk::createInstance(createInfo);
}

#ifndef NDEBUG
VKAPI_ATTR vk::Bool32 VKAPI_CALL InstanceBuilder::debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] const VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, [[maybe_unused]] void* const userData
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
