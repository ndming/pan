#pragma once

#include <vulkan/vulkan.hpp>

#include "engine/DeviceFeature.h"


class Translator {
public:
    [[nodiscard]] static vk::PhysicalDeviceFeatures toPhysicalDeviceFeatures(const std::vector<DeviceFeature>& features);
};
