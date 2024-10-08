#include <plog/Log.h>

#include "ResourceAllocator.h"


ResourceAllocator::Builder& ResourceAllocator::Builder::flags(const VmaAllocatorCreateFlags flags) {
    _flags = flags;
    return *this;
}

ResourceAllocator::Builder& ResourceAllocator::Builder::vulkanApiVersion(const uint32_t apiVersion) {
    _apiVersion = apiVersion;
    return *this;
}

ResourceAllocator* ResourceAllocator::Builder::build(
    const vk::Instance &instance,
    const vk::PhysicalDevice &physicalDevice,
    const vk::Device &device
) const {
    auto vulkanFunctions = VmaVulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};
    allocatorCreateInfo.flags = _flags;
    allocatorCreateInfo.vulkanApiVersion = _apiVersion;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device;
    allocatorCreateInfo.instance = instance;
    allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;

    auto allocator = VmaAllocator{};
    vmaCreateAllocator(&allocatorCreateInfo, &allocator);

    return new ResourceAllocator{ allocator };
}

vk::Buffer ResourceAllocator::allocateDedicatedBuffer(
    const std::size_t bufferSize,
    const vk::BufferUsageFlags usage,
    VmaAllocation* allocation
) const {
    auto bufferCreateInfo = VkBufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = static_cast<VkBufferUsageFlags>(usage);
    bufferCreateInfo.size = bufferSize;

    auto allocInfo = VmaAllocationCreateInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    allocInfo.priority = 1.0f;

    auto buffer = VkBuffer{};
    if (vmaCreateBuffer(_allocator, &bufferCreateInfo, &allocInfo, &buffer, allocation, nullptr) != VK_SUCCESS) {
        PLOGE << "Could not create a dedicated buffer";
        throw std::runtime_error("Failed to create a dedicated buffer");
    }

    return buffer;
}

vk::Buffer ResourceAllocator::allocateStagingBuffer(const std::size_t bufferSize, VmaAllocation* allocation) const {
    auto bufferCreateInfo = VkBufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCreateInfo.size = bufferSize;

    auto allocCreateInfo = VmaAllocationCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;

    auto buffer = VkBuffer{};
    if (vmaCreateBuffer(_allocator, &bufferCreateInfo, &allocCreateInfo, &buffer, allocation, nullptr) != VK_SUCCESS) {
        PLOGE << "Could not create staging buffer";
        throw std::runtime_error("Failed to create staging buffer");
    }

    return buffer;
}

vk::Buffer ResourceAllocator::allocatePersistentBuffer(
    const std::size_t bufferSize,
    const vk::BufferUsageFlags usage,
    VmaAllocation* allocation,
    VmaAllocationInfo* allocationInfo
) const {
    // This approach for creating persistently mapped buffers may not be optimal on systems with unified memory
    // (e.g. AMD APU or Intel integrated graphics, mobile chips)
    auto bufferCreateInfo = VkBufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.usage = static_cast<VkBufferUsageFlags>(usage);
    bufferCreateInfo.size = bufferSize;

    auto allocCreateInfo = VmaAllocationCreateInfo{};
    allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocCreateInfo.flags =  VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

    auto buffer = VkBuffer{};
    if (vmaCreateBuffer(_allocator, &bufferCreateInfo, &allocCreateInfo, &buffer, allocation, allocationInfo) != VK_SUCCESS) {
        PLOGE << "Could not create a persistently mapped buffer";
        throw std::runtime_error("Failed to create a persistently mapped buffer");
    }

    return buffer;
}

vk::Image ResourceAllocator::allocateDedicatedImage(
    const uint32_t width,
    const uint32_t height,
    const uint32_t depth,
    const uint32_t mipLevels,
    const vk::SampleCountFlagBits sampleCount,
    const vk::ImageType type,
    const vk::Format format,
    const vk::ImageTiling tiling,
    const vk::ImageUsageFlags usage,
    VmaAllocation* allocation
) const {
    auto imgCreateInfo = VkImageCreateInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    imgCreateInfo.extent.width = width;
    imgCreateInfo.extent.height = height;
    imgCreateInfo.extent.depth = depth;
    imgCreateInfo.mipLevels = mipLevels;
    imgCreateInfo.samples = static_cast<VkSampleCountFlagBits>(sampleCount);
    imgCreateInfo.imageType = static_cast<VkImageType>(type);
    imgCreateInfo.format = static_cast<VkFormat>(format);
    imgCreateInfo.tiling = static_cast<VkImageTiling>(tiling);
    imgCreateInfo.usage = static_cast<VkImageUsageFlags>(usage);
    imgCreateInfo.arrayLayers = 1;
    imgCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    auto allocInfo = VmaAllocationCreateInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    // This flag is preferable for resources that are large and get destroyed or recreated with different sizes
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    // When VK_EXT_memory_priority extension is enabled, it is also worth setting high priority to such allocation
    // to decrease chances to be evicted to system memory by the operating system
    allocInfo.priority = 1.0f;

    auto image = VkImage{};
    if (vmaCreateImage(_allocator, &imgCreateInfo, &allocInfo, &image, allocation, nullptr) != VK_SUCCESS) {
        PLOGE << "Could not create a dedicated image";
        throw std::runtime_error("Failed to create dedicated image");
    }

    return image;
}

void ResourceAllocator::destroyBuffer(const vk::Buffer& buffer, VmaAllocation allocation) const noexcept {
    vmaDestroyBuffer(_allocator, buffer, allocation);
}

void ResourceAllocator::destroyImage(const vk::Image& image, VmaAllocation allocation) const noexcept {
    vmaDestroyImage(_allocator, image, allocation);
}

void ResourceAllocator::mapAndCopyData(const std::size_t bufferSize, const void* const data, VmaAllocation allocation) const {
    void* mappedData;
    vmaMapMemory(_allocator, allocation, &mappedData);
    memcpy(mappedData, data, bufferSize);
    vmaUnmapMemory(_allocator, allocation);
}

ResourceAllocator::~ResourceAllocator() {
    vmaDestroyAllocator(_allocator);
}
