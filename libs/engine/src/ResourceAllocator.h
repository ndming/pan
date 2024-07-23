#pragma once

#include "external/VmaUsage.h"

#include <vulkan/vulkan.hpp>

#include <cstdint>


class ResourceAllocator {
public:
    class Builder {
    public:
        Builder& flags(VmaAllocatorCreateFlags flags);
        Builder& vulkanApiVersion(uint32_t apiVersion);

        [[nodiscard]] ResourceAllocator* build(
            const vk::Instance& instance, const vk::PhysicalDevice& physicalDevice, const vk::Device& device) const;

    private:
        VmaAllocatorCreateFlags _flags{};
        uint32_t _apiVersion{};
    };

    vk::Image allocateColorAttachmentImage(
        uint32_t width, uint32_t height, uint32_t mipLevels, vk::SampleCountFlagBits sampleCount,
        vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, VmaAllocation* allocation) const;

    void destroyImage(const vk::Image& image, VmaAllocation allocation) const noexcept;

    vk::Buffer allocateDeviceBuffer(std::size_t bufferSize, VkBufferUsageFlags usage, VmaAllocation* allocation) const;
    vk::Buffer allocateStagingBuffer(std::size_t bufferSize, VmaAllocation* allocation) const;

    void destroyBuffer(const vk::Buffer& buffer, VmaAllocation allocation) const noexcept;

    void mapData(std::size_t bufferSize, const void* data, VmaAllocation allocation) const;

    ~ResourceAllocator();

    ResourceAllocator(const ResourceAllocator&) = delete;
    ResourceAllocator& operator=(const ResourceAllocator&) = delete;

private:
    explicit ResourceAllocator(VmaAllocator allocator) : _allocator{ allocator } {}

    VmaAllocator _allocator{};
};
