#pragma once

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>


class PhysicalDeviceSelector {
public:
    PhysicalDeviceSelector& extensions(const std::vector<const char*>& extensions);
    PhysicalDeviceSelector& features(const vk::PhysicalDeviceFeatures& features);

    [[nodiscard]] std::vector<vk::PhysicalDevice> select(
        const std::vector<vk::PhysicalDevice>& candidates, const vk::SurfaceKHR& surface) const;

private:
    std::vector<std::string> _requiredExtensions{};
    [[nodiscard]] bool checkExtensionSupport(const vk::PhysicalDevice& device) const;

    vk::PhysicalDeviceFeatures _features{};
    [[nodiscard]] bool checkFeatureSupport(const vk::PhysicalDevice& device) const;
};
