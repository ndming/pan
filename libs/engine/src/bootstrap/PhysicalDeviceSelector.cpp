#include "PhysicalDeviceSelector.h"
#include "engine/Engine.h"

#include <algorithm>
#include <ranges>
#include <unordered_set>


PhysicalDeviceSelector& PhysicalDeviceSelector::extensions(const std::vector<const char*>& extensions) {
    _requiredExtensions = std::vector<std::string>(extensions.begin(), extensions.end());
    return *this;
}

std::vector<vk::PhysicalDevice> PhysicalDeviceSelector::select(
    const std::vector<vk::PhysicalDevice>& candidates,
    const vk::SurfaceKHR& surface,
    const EngineFeature& feature
) const {
    const auto suitable = [this, &surface, &feature](const auto& device) {
        // Although the availability of a presentation queue implies that the swap chain extension must be supported,
        // it’s still good to be explicit about things.
        const auto extensionSupported = checkExtensionSupport(device);

        // Just checking if a swap chain is available is not sufficient, because it may not actually be compatible
        // with our window surface.
        auto swapChainAdequate = false;
        if (extensionSupported) {
            const auto capabilities = device.getSurfaceCapabilitiesKHR(surface);
            const auto formats = device.getSurfaceFormatsKHR(surface);
            const auto presentModes = device.getSurfacePresentModesKHR(surface);
            swapChainAdequate = !formats.empty() && !presentModes.empty();
        }

        return extensionSupported && swapChainAdequate && checkFeatureSupport(device, feature);
    };

    return candidates | std::ranges::views::filter(suitable) | std::ranges::to<std::vector>();
}

bool PhysicalDeviceSelector::checkExtensionSupport(const vk::PhysicalDevice& device) const {
    using namespace std::ranges;
    const auto availableExtensions = device.enumerateDeviceExtensionProperties();
    const auto toName = [](const vk::ExtensionProperties& extension) { return extension.extensionName.data(); };
    const auto extensions = availableExtensions | views::transform(toName) | to<std::unordered_set<std::string>>();

    const auto available = [&extensions](const auto& it) { return extensions.contains(it); };
    return all_of(_requiredExtensions, std::identity{}, available);
}

bool PhysicalDeviceSelector::checkFeatureSupport(const vk::PhysicalDevice& device, const EngineFeature& feature) {
    auto basicFeatures = vk::PhysicalDeviceFeatures{};
    device.getFeatures(&basicFeatures);

    auto vertexInputDynamicStateFeatures = vk::PhysicalDeviceVertexInputDynamicStateFeaturesEXT{};
    auto descriptorIndexingFeatures = vk::PhysicalDeviceDescriptorIndexingFeatures{};
    auto extendedDynamicStateFeatures = vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT{};
    auto extendedDynamicState2Features = vk::PhysicalDeviceExtendedDynamicState2FeaturesEXT{};
    auto extendedDynamicState3Features = vk::PhysicalDeviceExtendedDynamicState3FeaturesEXT{};

    auto supportedFeatures = vk::PhysicalDeviceFeatures2{};
    supportedFeatures.features = basicFeatures;
    supportedFeatures.pNext = &vertexInputDynamicStateFeatures;
    vertexInputDynamicStateFeatures.pNext = &descriptorIndexingFeatures;
    descriptorIndexingFeatures.pNext = &extendedDynamicStateFeatures;
    extendedDynamicStateFeatures.pNext = &extendedDynamicState2Features;
    extendedDynamicState2Features.pNext = &extendedDynamicState3Features;

    device.getFeatures2(&supportedFeatures);

    // Explicitly required by the Engine
    if (!basicFeatures.largePoints) {
        return false;
    }
    if (!basicFeatures.wideLines) {
        return false;
    }
    if (!vertexInputDynamicStateFeatures.vertexInputDynamicState || !extendedDynamicStateFeatures.extendedDynamicState ||
        !extendedDynamicState2Features.extendedDynamicState2 || !extendedDynamicState3Features.extendedDynamicState3PolygonMode) {
        return false;
    }

    // Explicitly required by the application
    if (!descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount) {
        return false;
    }

    // Optionally requested
    if (feature.sampleShading && !basicFeatures.sampleRateShading) {
        return false;
    }
    if (feature.samplerAnisotropy && !basicFeatures.samplerAnisotropy) {
        return false;
    }

    return true;
}
