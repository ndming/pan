#include <algorithm>
#include <ranges>
#include <unordered_set>

#include "PhysicalDeviceSelector.h"


PhysicalDeviceSelector& PhysicalDeviceSelector::extensions(const std::vector<const char*> &extensions) {
    _requiredExtensions = std::vector<std::string>(extensions.begin(), extensions.end());
    return *this;
}

PhysicalDeviceSelector & PhysicalDeviceSelector::features(const vk::PhysicalDeviceFeatures &features) {
    _features = features;
    return *this;
}

std::vector<vk::PhysicalDevice> PhysicalDeviceSelector::select(
    const std::vector<vk::PhysicalDevice>& candidates,
    const vk::SurfaceKHR& surface
) const {
    const auto suitable = [this, &surface](const auto& device) {
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

        return extensionSupported && swapChainAdequate && checkFeatureSupport(device);
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

bool PhysicalDeviceSelector::checkFeatureSupport(const vk::PhysicalDevice& device) const {
    const auto supportedFeatures = device.getFeatures();

    const auto requestedFeaturesArray = reinterpret_cast<const vk::Bool32*>(&_features);
    const auto supportedFeaturesArray = reinterpret_cast<const vk::Bool32*>(&supportedFeatures);
    constexpr auto featureCount = sizeof(vk::PhysicalDeviceFeatures) / sizeof(vk::Bool32);

    for (size_t i = 0; i < featureCount; ++i) {
        if (requestedFeaturesArray[i] && !supportedFeaturesArray[i]) {
            return false;
        }
    }
    return true;
}
