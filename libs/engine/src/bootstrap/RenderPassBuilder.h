#pragma once

#include <vulkan/vulkan.hpp>


class RenderPassBuider {
public:
    explicit RenderPassBuider(const vk::Format format) : _surfaceFormat{ format } {}

    RenderPassBuider& sampleCount(vk::SampleCountFlagBits msaa);

    [[nodiscard]] vk::RenderPass build(const vk::Device& device) const;

private:
    vk::Format _surfaceFormat;

    vk::SampleCountFlagBits _msaaSamples{ vk::SampleCountFlagBits::e1 };
};
