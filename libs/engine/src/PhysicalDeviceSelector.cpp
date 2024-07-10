#include <set>

#include <plog/Log.h>

#include "PhysicalDeviceSelector.h"


PhysicalDeviceSelector::PhysicalDeviceSelector(const vk::Instance &instance, const vk::SurfaceKHR &surface) {
    const auto devices = instance.enumeratePhysicalDevices();

    const auto suitable = [this, &surface](const vk::PhysicalDevice& device) {
        const auto indices = findQueueFamilies(device, surface);
        const auto extensionSupported = extensionSupported(device);

        auto swapChainAdequate = false;
        if (extensionSupported) {
            // const auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
            const auto formats = device.getSurfaceFormatsKHR(surface);
            const auto presentModes = device.getSurfacePresentModesKHR(surface);
            swapChainAdequate = !formats.empty() && !presentModes.empty();
        }

        return indices.isComplete() && extensionSupported && swapChainAdequate && featureSupported(device);
    };

    if (const auto found = std::ranges::find_if(devices, suitable); found != devices.end()) {
        const auto device = *found;

        const auto properties = device.getProperties();
        PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
    } else {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

PhysicalDeviceSelector & PhysicalDeviceSelector::queueFlags(const vk::QueueFlags flags) {
    _queueFlags = flags;
    return *this;
}

PhysicalDeviceSelector & PhysicalDeviceSelector::bindOptions(const BindOptions &options) {
    _options = options;
    return *this;
}

QueueFamilyIndices PhysicalDeviceSelector::findQueueFamilies(const vk::PhysicalDevice &device, const vk::SurfaceKHR &surface) const {
    auto indices = QueueFamilyIndices{};

    const auto queueFamilies = device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        if (queueFamilies[i].queueFlags & _queueFlags) {
            indices.graphics = i;
        }
        if (device.getSurfaceSupportKHR(i, surface)) {
            indices.present = i;
        }
        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

bool PhysicalDeviceSelector::extensionSupported(const vk::PhysicalDevice& device) {
    static constexpr auto deviceExtensions = std::array{
        vk::KHRSwapchainExtensionName
    };
    const auto availableExtensions = device.enumerateDeviceExtensionProperties();

    auto requiredExtensions = std::set<std::string>(deviceExtensions.begin(), deviceExtensions.end());
    // TODO: Debug GCC complaints
    const auto tickOff = [&requiredExtensions](const auto& it){ requiredExtensions.erase(it.extensionName); };
    std::ranges::for_each(availableExtensions, tickOff);

    return requiredExtensions.empty();
}

bool PhysicalDeviceSelector::featureSupported(const vk::PhysicalDevice &device) const {
    const auto features = device.getFeatures();

    auto supported = true;
    if (_options.enabledSamplerAnisotropy) {
        supported = features.samplerAnisotropy;
    }

    return supported;
}
