#pragma once

#include <vulkan/vulkan.hpp>

#include <string>
#include <vector>


struct EngineFeature;

class PhysicalDeviceSelector {
public:
    PhysicalDeviceSelector& extensions(const std::vector<const char*>& extensions);

    [[nodiscard]] std::vector<vk::PhysicalDevice> select(
        const std::vector<vk::PhysicalDevice>& candidates, const vk::SurfaceKHR& surface, const EngineFeature& feature) const;

private:
    std::vector<std::string> _requiredExtensions{};
    [[nodiscard]] bool checkExtensionSupport(const vk::PhysicalDevice& device) const;

    [[nodiscard]] static bool checkFeatureSupport(const vk::PhysicalDevice& device, const EngineFeature& feature);
};
