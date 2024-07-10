#pragma once

#include <vulkan/vulkan.hpp>

#include "Utility.h"


class PhysicalDeviceSelector {
public:
    PhysicalDeviceSelector(const vk::Instance& instance, const vk::SurfaceKHR& surface);

    PhysicalDeviceSelector& queueFlags(vk::QueueFlags flags);
    PhysicalDeviceSelector& bindOptions(const BindOptions& options);

private:
    vk::QueueFlags _queueFlags{ vk::QueueFlagBits::eGraphics };
    [[nodiscard]] QueueFamilyIndices findQueueFamilies(const vk::PhysicalDevice& device, const vk::SurfaceKHR& surface) const;

    [[nodiscard]] static bool extensionSupported(const vk::PhysicalDevice& device);

    BindOptions _options{};
    [[nodiscard]] bool featureSupported(const vk::PhysicalDevice& device) const;
};
