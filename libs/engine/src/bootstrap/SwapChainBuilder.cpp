#include <ranges>

#include "external/VmaUsage.h"

#include "SwapChainBuilder.h"


SwapChainBuilder & SwapChainBuilder::surfaceFormat(const vk::SurfaceFormatKHR format) {
    _surfaceFormat = format;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::presentMode(const vk::PresentModeKHR mode) {
    _presentMode = mode;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::extent(const vk::Extent2D extent) {
    _extent = extent;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::minImageCount(const uint32_t count) {
    _minImageCount = count;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::imageUsage(const vk::ImageUsageFlags usage) {
    _imageUsage = usage;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::preTransform(const vk::SurfaceTransformFlagBitsKHR transform) {
    _preTransform = transform;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::sampleCount(const vk::SampleCountFlagBits count) {
    _msaaSamples = count;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::compositeAlpha(const vk::CompositeAlphaFlagBitsKHR compositeAlpha) {
    _compositeAlpha = compositeAlpha;
    return *this;
}

SwapChainBuilder & SwapChainBuilder::clipped(const vk::Bool32 clipped) {
    _clipped = clipped;
    return *this;
}

SwapChain* SwapChainBuilder::build(
    const vk::Device &device,
    const vk::SurfaceKHR &surface,
    const ResourceAllocator* const allocator
) const {
    const auto swapChain = createSwapChain(device, surface);
    initializeSwapChainImageViews(swapChain, device);
    initializeSwapChainColorAttachment(swapChain, device, allocator);

    return swapChain;
}

SwapChain* SwapChainBuilder::createSwapChain(const vk::Device& device, const vk::SurfaceKHR& surface) const {
    auto createInfo = vk::SwapchainCreateInfoKHR{};
    createInfo.surface = surface;
    createInfo.minImageCount = _minImageCount;
    createInfo.imageFormat = _surfaceFormat.format;
    createInfo.imageColorSpace = _surfaceFormat.colorSpace;
    createInfo.imageExtent = _extent;
    createInfo.presentMode = _presentMode;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = _imageUsage;
    if (_graphicsFamily != _presentFamily) {
        const uint32_t queueFamilyIndices[]{ _graphicsFamily, _presentFamily };
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = _preTransform;
    createInfo.compositeAlpha = _compositeAlpha;
    createInfo.clipped = _clipped;
    createInfo.oldSwapchain = nullptr;

    const auto swapChain = new SwapChain{};
    swapChain->_nativeObject = device.createSwapchainKHR(createInfo);
    swapChain->_images = device.getSwapchainImagesKHR(swapChain->_nativeObject);
    swapChain->_imageFormat = _surfaceFormat;
    swapChain->_imageExtent = _extent;
    swapChain->_msaa = _msaaSamples;

    return swapChain;
}

void SwapChainBuilder::initializeSwapChainImageViews(SwapChain* const swapChain, const vk::Device& device) const {
    using namespace std::ranges;
    const auto toImageView = [this, &device](const auto& image) {
        constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;
        constexpr auto mipLevels = 1;
        const auto viewInfo = vk::ImageViewCreateInfo{
            {}, image, vk::ImageViewType::e2D, _surfaceFormat.format, {}, { aspectFlags, 0, mipLevels, 0, 1 } };
        return device.createImageView(viewInfo);
    };
    const auto imageViews = swapChain->_images | views::transform(toImageView);
    swapChain->_imageViews = std::vector(imageViews.begin(), imageViews.end());
}

void SwapChainBuilder::initializeSwapChainColorAttachment(
    SwapChain* const swapChain,
    const vk::Device& device,
    const ResourceAllocator* const allocator
) const {
    using Usage = vk::ImageUsageFlagBits;
    constexpr auto mipLevels = 1;
    constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;

    auto allocation = VmaAllocation{};
    swapChain->_colorImage = allocator->createColorAttachmentImage(
        _extent.width, _extent.height, mipLevels, _msaaSamples, _surfaceFormat.format,
        vk::ImageTiling::eOptimal, Usage::eTransientAttachment | Usage::eColorAttachment, &allocation);
    swapChain->_colorImageAllocation = allocation;

    const auto viewInfo = vk::ImageViewCreateInfo{
            {}, swapChain->_colorImage, vk::ImageViewType::e2D, _surfaceFormat.format, {}, { aspectFlags, 0, mipLevels, 0, 1 } };
    swapChain->_colorImageView = device.createImageView(viewInfo);
}
