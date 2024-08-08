#pragma once

#include "VmaUsage.h"

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

    vk::Buffer allocateDedicatedBuffer(
        std::size_t bufferSize,
        vk::BufferUsageFlags usage,
        VmaAllocation* allocation) const;

    vk::Buffer allocateStagingBuffer(
        std::size_t bufferSize,
        VmaAllocation* allocation) const;

    vk::Buffer allocatePersistentBuffer(
        std::size_t bufferSize,
        vk::BufferUsageFlags usage,
        VmaAllocation* allocation,
        VmaAllocationInfo* allocationInfo) const;

    vk::Image allocateDedicatedImage(
        uint32_t width,
        uint32_t height,
        uint32_t depth,
        uint32_t mipLevels,
        vk::SampleCountFlagBits sampleCount,
        vk::ImageType type,
        vk::Format format,
        vk::ImageTiling tiling,
        vk::ImageUsageFlags usage,
        VmaAllocation* allocation) const;

    void destroyBuffer(const vk::Buffer& buffer, VmaAllocation allocation) const noexcept;

    void destroyImage(const vk::Image& image, VmaAllocation allocation) const noexcept;

    void mapAndCopyData(std::size_t bufferSize, const void* data, VmaAllocation allocation) const;

    ~ResourceAllocator();

    ResourceAllocator(const ResourceAllocator&) = delete;
    ResourceAllocator& operator=(const ResourceAllocator&) = delete;

private:
    explicit ResourceAllocator(VmaAllocator allocator) : _allocator{ allocator } {}

    VmaAllocator _allocator{};
};
