#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "engine/SwapChain.h"
#include "ResourceAllocator.h"


class SwapChainBuilder {
public:
    SwapChainBuilder(const uint32_t graphicsFamily, const uint32_t presentFamily)
    : _graphicsFamily{ graphicsFamily }, _presentFamily{ presentFamily } {}

    SwapChainBuilder& surfaceFormat(vk::SurfaceFormatKHR format);
    SwapChainBuilder& presentMode(vk::PresentModeKHR mode);
    SwapChainBuilder& extent(vk::Extent2D extent);

    SwapChainBuilder& minImageCount(uint32_t count);
    SwapChainBuilder& imageUsage(vk::ImageUsageFlags usage);
    SwapChainBuilder& preTransform(vk::SurfaceTransformFlagBitsKHR transform);

    SwapChainBuilder& sampleCount(vk::SampleCountFlagBits count);
    SwapChainBuilder& compositeAlpha(vk::CompositeAlphaFlagBitsKHR compositeAlpha);
    SwapChainBuilder& clipped(vk::Bool32 clipped);

    [[nodiscard]] SwapChain* build(
        const vk::Device& device, const vk::SurfaceKHR& surface, const ResourceAllocator* allocator) const;

private:
    uint32_t _graphicsFamily;
    uint32_t _presentFamily;

    vk::SurfaceFormatKHR _surfaceFormat{};
    vk::PresentModeKHR _presentMode{};
    vk::Extent2D _extent{};

    uint32_t _minImageCount{};
    vk::ImageUsageFlags _imageUsage{};
    vk::SurfaceTransformFlagBitsKHR _preTransform{};

    vk::SampleCountFlagBits _msaaSamples{ vk::SampleCountFlagBits::e1 };
    vk::CompositeAlphaFlagBitsKHR _compositeAlpha{ vk::CompositeAlphaFlagBitsKHR::eOpaque };
    vk::Bool32 _clipped{ vk::True };

    [[nodiscard]] SwapChain* createSwapChain(const vk::Device& device, const vk::SurfaceKHR& surface) const;
    void initializeSwapChainImageViews(SwapChain* swapChain, const vk::Device& device) const;
    void initializeSwapChainColorAttachment(SwapChain* swapChain, const vk::Device& device, const ResourceAllocator* allocator) const;
};
