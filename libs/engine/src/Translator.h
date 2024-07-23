#pragma once

#include "engine/Engine.h"

#include <vulkan/vulkan.hpp>


class Translator {
public:
    static vk::SampleCountFlagBits toSupportSampleCount(SwapChain::MSAA msaa, const vk::PhysicalDevice& device);

private:
    static vk::SampleCountFlagBits getMaxUsableSampleCount(const vk::PhysicalDevice& device);
    static vk::SampleCountFlagBits toSampleCount(SwapChain::MSAA msaa);
};
