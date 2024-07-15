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

std::unique_ptr<ResourceAllocator> ResourceAllocator::Builder::build(
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

    return std::unique_ptr<ResourceAllocator>(new ResourceAllocator{ allocator });
}

vk::Image ResourceAllocator::createColorAttachmentImage(
    const uint32_t width,
    const uint32_t height,
    const uint32_t mipLevels,
    const vk::SampleCountFlagBits sampleCount,
    const vk::Format format,
    const vk::ImageTiling tiling,
    const vk::ImageUsageFlags usage,
    VmaAllocation* allocation
) const {
    const auto imageInfo = vk::ImageCreateInfo{
        {}, vk::ImageType::e2D, format, { width, height, 1 }, mipLevels, 1,
        sampleCount, tiling, usage, vk::SharingMode::eExclusive };
    const auto imageCreateInfo = static_cast<VkImageCreateInfo>(imageInfo);

    auto allocInfo = VmaAllocationCreateInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    // This flag is preferable for resources that are large and get destroyed or recreated with different sizes
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    // When VK_EXT_memory_priority extension is enabled, it is also worth setting high priority to such allocation
    // to decrease chances to be evicted to system memory by the operating system
    allocInfo.priority = 1.0f;

    auto image = VkImage{};
    if (vmaCreateImage(_allocator, &imageCreateInfo, &allocInfo, &image, allocation, nullptr) != VK_SUCCESS) {
        PLOG_ERROR << "Could not create color attachment image";
        throw std::runtime_error("Failed to create color attachment image");
    }

    return image;
}

void ResourceAllocator::destroyImage(const vk::Image& image, VmaAllocation allocation) const noexcept {
    vmaDestroyImage(_allocator, image, allocation);
}

ResourceAllocator::~ResourceAllocator() {
    vmaDestroyAllocator(_allocator);
}
