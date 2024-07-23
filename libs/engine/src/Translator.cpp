#include "Translator.h"

#include <plog/Log.h>


vk::PhysicalDeviceFeatures Translator::toPhysicalDeviceFeatures(const std::vector<Engine::Feature>& features) {
    auto deviceFeatures = vk::PhysicalDeviceFeatures{};

    for (const auto& feature : features) {
        switch (feature) {
        case Engine::Feature::SamplerAnisotropy:
            deviceFeatures.samplerAnisotropy = vk::True; break;
        case Engine::Feature::SampleRateShading:
            deviceFeatures.sampleRateShading = vk::True; break;
        }
    }

    return deviceFeatures;
}

vk::Format Translator::toFormat(const AttributeFormat format) {
    using enum AttributeFormat;
    switch (format) {
        case Float:  return vk::Format::eR32Sfloat;
        case Vec2:   return vk::Format::eR32G32Sfloat;
        case Vec3:   return vk::Format::eR32G32B32Sfloat;
        case Vec4:   return vk::Format::eR32G32B32A32Sfloat;
        case Uint:   return vk::Format::eR32Uint;
        case Uvec2:  return vk::Format::eR32G32Uint;
        case Uvec3:  return vk::Format::eR32G32B32Uint;
        case Uvec4:  return vk::Format::eR32G32B32A32Uint;
        case Int:    return vk::Format::eR32Sint;
        case Ivec2:  return vk::Format::eR32G32Sint;
        case Ivec3:  return vk::Format::eR32G32B32Sint;
        case Ivec4:  return vk::Format::eR32G32B32A32Sint;
        case Double: return vk::Format::eR64Sfloat;
        default:     return vk::Format::eUndefined;
    }
}

vk::SampleCountFlagBits Translator::toSupportSampleCount(const SwapChain::MSAA msaa, const vk::PhysicalDevice& device) {
    const auto maxSampleCount = getMaxUsableSampleCount(device);
    if (static_cast<int>(toSampleCount(msaa)) <= static_cast<int>(maxSampleCount)) {
        return toSampleCount(msaa);
    }
    using enum vk::SampleCountFlagBits;
    switch (maxSampleCount) {
        case e2:  PLOGW << "Falling back MSAA configuration: your hardware only supports up to x2 MSAA";  return e2;
        case e4:  PLOGW << "Falling back MSAA configuration: your hardware only supports up to x4 MSAA";  return e4;
        case e8:  PLOGW << "Falling back MSAA configuration: your hardware only supports up to x8 MSAA";  return e8;
        case e16: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x16 MSAA"; return e16;
        case e32: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x32 MSAA"; return e32;
        case e64: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x64 MSAA"; return e64;
        default:  PLOGW << "Falling back MSAA configuration: your hardware only supports up to x1 MSAA";  return e1;
    }

}

vk::SampleCountFlagBits Translator::getMaxUsableSampleCount(const vk::PhysicalDevice& device) {
    const auto physicalDeviceProperties = device.getProperties();
    const auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8)  { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4)  { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2)  { return vk::SampleCountFlagBits::e2; }
    return vk::SampleCountFlagBits::e1;
}

vk::SampleCountFlagBits Translator::toSampleCount(const SwapChain::MSAA msaa) {
    using enum SwapChain::MSAA;
    switch (msaa) {
        case x2:  return vk::SampleCountFlagBits::e2;
        case x4:  return vk::SampleCountFlagBits::e4;
        case x8:  return vk::SampleCountFlagBits::e8;
        case x16: return vk::SampleCountFlagBits::e16;
        case x32: return vk::SampleCountFlagBits::e32;
        case x64: return vk::SampleCountFlagBits::e64;
        default:  return vk::SampleCountFlagBits::e1;
    }
}
