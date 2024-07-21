#include "DeviceBuilder.h"

#include <ranges>


DeviceBuilder& DeviceBuilder::queueFamilies(const std::set<uint32_t>& families) {
    _uniqueFamilies = families;
    return *this;
}

DeviceBuilder& DeviceBuilder::deviceExtensions(const std::vector<const char*>& extensions) {
    _deviceExtensions = extensions;
    return *this;
}

DeviceBuilder& DeviceBuilder::deviceFeatures(const vk::PhysicalDeviceFeatures& features) {
    _deviceFeatures = features;
    return *this;
}

DeviceBuilder& DeviceBuilder::validationLayers(const std::vector<const char*>& layers) {
    _validationLayers = layers;
    return *this;
}


vk::Device DeviceBuilder::build(const vk::PhysicalDevice& physicalDevice) const {
    using namespace std::views;
    constexpr auto priority = 1.0f;
    const auto toQueueCreateInfo = [&priority](const auto& family) { return vk::DeviceQueueCreateInfo{ {}, family, 1, &priority }; };
    const auto queueCreateInfos = _uniqueFamilies | transform(toQueueCreateInfo) | std::ranges::to<std::vector>();

    auto deviceCreateInfo = vk::DeviceCreateInfo{};
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pEnabledFeatures = &_deviceFeatures;
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(_deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = _deviceExtensions.data();

#ifndef NDEBUG
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = _validationLayers.data();
#else
    deviceCreateInfo.enabledLayerCount = 0;
#endif

    return physicalDevice.createDevice(deviceCreateInfo);
}
