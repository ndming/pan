#pragma once

#include <vulkan/vulkan.hpp>

#include "engine/DeviceFeature.h"
#include "engine/SwapChain.h"


class Translator {
public:
    static vk::PhysicalDeviceFeatures toPhysicalDeviceFeatures(const std::vector<DeviceFeature>& features);

    static vk::SampleCountFlagBits toSupportSampleCount(SwapChain::MSAA msaa, const vk::PhysicalDevice& device);

private:
    static vk::SampleCountFlagBits getMaxUsableSampleCount(const vk::PhysicalDevice& device);

    static vk::SampleCountFlagBits toSampleCount(SwapChain::MSAA msaa);
};
