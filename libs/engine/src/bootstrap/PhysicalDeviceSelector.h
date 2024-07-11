#pragma once

#include <string>
#include <vector>

#include <vulkan/vulkan.hpp>


class PhysicalDeviceSelector {
public:
    PhysicalDeviceSelector& extensions(const std::vector<const char*>& extensions);
    PhysicalDeviceSelector& features11(const vk::PhysicalDeviceVulkan11Features& features);
    PhysicalDeviceSelector& features12(const vk::PhysicalDeviceVulkan12Features& features);
    PhysicalDeviceSelector& features13(const vk::PhysicalDeviceVulkan13Features& features);

    [[nodiscard]] std::vector<vk::PhysicalDevice> select(
        const std::vector<vk::PhysicalDevice>& candidates, const vk::SurfaceKHR& surface) const;

private:
    std::vector<std::string> _requiredExtensions{};
    [[nodiscard]] bool checkExtensionSupport(const vk::PhysicalDevice& device) const;

    vk::PhysicalDeviceVulkan11Features _features11{};
    vk::PhysicalDeviceVulkan12Features _features12{};
    vk::PhysicalDeviceVulkan13Features _features13{};
    [[nodiscard]] bool checkFeatureSupport(const vk::PhysicalDevice& device) const;
};
