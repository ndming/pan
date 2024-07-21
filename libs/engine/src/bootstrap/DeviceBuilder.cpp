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
    // The currently available drivers will only allow creating a small number of queues for each queue family and
    // we don’t really need more than one. All the command buffers shall be created on multiple threads and then
    // submitted at once on the main thread with a single low-overhead call
    using namespace std::views;
    // Vulkan lets us assign priorities to queues to influence the scheduling of command buffer execution
    // using floating point numbers between 0.0 and 1.0. This is required even if there is only a single queue
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
    // Previous implementations of Vulkan made a distinction between instance and device specific validation layers,
    // but this is no longer the case. That means that the enabledLayerCount and ppEnabledLayerNames fields of
    // VkDeviceCreateInfo are ignored by up-to-date implementations However, it is still a good idea to set them anyway
    // to be compatible with older implementations
    deviceCreateInfo.enabledLayerCount = static_cast<uint32_t>(_validationLayers.size());
    deviceCreateInfo.ppEnabledLayerNames = _validationLayers.data();
#else
    deviceCreateInfo.enabledLayerCount = 0;
#endif

    return physicalDevice.createDevice(deviceCreateInfo);
}
