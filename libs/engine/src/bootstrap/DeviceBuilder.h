#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <set>


class DeviceBuilder {
public:
    DeviceBuilder& queueFamilies(const std::set<uint32_t>& families);
    DeviceBuilder& deviceFeatures(const vk::PhysicalDeviceFeatures2& features);
    DeviceBuilder& deviceExtensions(const std::vector<const char*>& extensions);
    DeviceBuilder& validationLayers(const std::vector<const char*>& layers);

    [[nodiscard]] vk::Device build(const vk::PhysicalDevice& physicalDevice) const;

private:
    std::set<uint32_t> _uniqueFamilies{};
    vk::PhysicalDeviceFeatures2 _deviceFeatures{};
    std::vector<const char*> _deviceExtensions{};
    std::vector<const char*> _validationLayers{};
};
