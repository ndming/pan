#pragma once

#include <cstdint>
#include <memory>

#include <vulkan/vulkan.hpp>

#include "external/VmaUsage.h"


class ResourceAllocator {
public:
    class Builder {
    public:
        Builder& flags(VmaAllocatorCreateFlags flags);
        Builder& vulkanApiVersion(uint32_t apiVersion);

        [[nodiscard]] std::unique_ptr<ResourceAllocator> build(
            const vk::Instance& instance, const vk::PhysicalDevice& physicalDevice, const vk::Device& device) const;

    private:
        VmaAllocatorCreateFlags _flags{};
        uint32_t _apiVersion{};
    };

    vk::Image createColorAttachmentImage(
        uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount,
        vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaAllocation* allocation) const;

    void destroyImage(const vk::Image& image, VmaAllocation allocation) const noexcept;

    ~ResourceAllocator();
    ResourceAllocator(const ResourceAllocator&) = delete;
    ResourceAllocator& operator=(const ResourceAllocator&) = delete;

private:
    explicit ResourceAllocator(VmaAllocator allocator) : _allocator{ allocator } {}

    VmaAllocator _allocator{};
};
