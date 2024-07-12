#include "Translator.h"


vk::PhysicalDeviceFeatures Translator::toPhysicalDeviceFeatures(const std::vector<DeviceFeature> &features) {
    auto deviceFeatures = vk::PhysicalDeviceFeatures{};

    for (const auto& feature : features) {
        switch (feature) {
            case DeviceFeature::SamplerAnisotropy:
                deviceFeatures.samplerAnisotropy = vk::True;
            case DeviceFeature::SampleRateSahding:
                deviceFeatures.sampleRateShading = vk::True;
        }
    }

    return deviceFeatures;
}
