#include <array>
#include <functional>
#include <ranges>
#include <unordered_set>

#include <GLFW/glfw3.h>

#include "DebugMessenger.h"
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

vk::Instance InstanceBuilder::build(const vk::InstanceCreateFlags flags) const {
    // Application info
    const auto appInfo = vk::ApplicationInfo{
        _applicationName.data(), _applicationVersion, "None", _apiVersion, _apiVersion };

#ifndef NDEBUG
    // Check for layer support
    using namespace std::ranges;
    const auto properties = vk::enumerateInstanceLayerProperties();

    const auto toName = [](const auto& property) { return property.layerName.data(); };
    const auto supportedLayers = properties | views::transform(toName) | to<std::unordered_set<std::string>>();

    constexpr auto validationLayers = std::array{
        "VK_LAYER_KHRONOS_validation",
    };
    if (const auto supported = [&supportedLayers](const auto& it) { return supportedLayers.contains(it); };
        any_of(validationLayers, std::logical_not{}, supported)) {
        throw std::runtime_error("Validation layers requested, but not available!");
    }
#endif

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
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();

    using MessageSeverity = vk::DebugUtilsMessageSeverityFlagBitsEXT;
    using MessageType = vk::DebugUtilsMessageTypeFlagBitsEXT;
    const auto debugCreateInfo = vk::DebugUtilsMessengerCreateInfoEXT{ {},
        MessageSeverity::eVerbose | MessageSeverity::eWarning | MessageSeverity::eError,
        MessageType::eGeneral | MessageType::eValidation | MessageType::ePerformance,
        DebugMessenger::callback, nullptr
    };
    createInfo.pNext = &debugCreateInfo;
#else
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
#endif

    return vk::createInstance(createInfo);
}
