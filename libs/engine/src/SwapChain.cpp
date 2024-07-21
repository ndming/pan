#include "engine/SwapChain.h"

#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"

#include "ResourceAllocator.h"

#include <GLFW/glfw3.h>
#include <plog/Log.h>

#include <ranges>
#include <set>


SwapChain::SwapChain(
    GLFWwindow* const window,
    const vk::Instance& instance,
    const vk::PhysicalDeviceFeatures& features,
    const std::vector<const char*>& extensions
) : _window{ window } {
    // Create a surface
    if (glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface)) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a window surface!");
    }

    // Find a list of possible candidate devices based on requested features and extensions
    const auto candidates = PhysicalDeviceSelector()
        .extensions(extensions)
        .features(features)
        .select(instance.enumeratePhysicalDevices(), _surface);

    // Pick a physical device based on supported queue faimlies
    auto finder = QueueFamilyFinder()
        .requestPresentFamily(_surface)
        .requestComputeFamily();

    // Set out a fallback device in case we couldn't find a device supporting async compute
    auto fallbackCandidate = vk::PhysicalDevice{};
    for (const auto& candidate : candidates) {
        if (finder.find(candidate)) {
            _physicalDevice = candidate;
            break;
        }
        if (finder.completed(true)) {
            fallbackCandidate = candidate;
        }
        finder.reset();
    }
    if (_physicalDevice) {
        _graphicsFamily = finder.getGraphicsFamily();
        _presentFamily = finder.getPresentFamily();
        _computeFamily = finder.getComputeFamily();

        PLOG_INFO << "Detected async compute capability";
    } else if (fallbackCandidate) {
        _physicalDevice = fallbackCandidate;

        // Find queue families again since we didn't break out early when we set fallback candidate
        finder.find(fallbackCandidate);
        _graphicsFamily = finder.getGraphicsFamily();
        _presentFamily = finder.getPresentFamily();

    } else {
        PLOG_ERROR << "Could not find a suitable GPU: try requesting less features or updating your driver";
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

#ifndef NDEBUG
    PLOG_DEBUG << "Graphics queue family index: " << _graphicsFamily.value();
    PLOG_DEBUG << "Present queue family index:  " << _presentFamily.value();

    if (_computeFamily.has_value()) {
        PLOG_DEBUG << "Compute queue family index:  " << _computeFamily.value();
    }

#endif
    // Print the device name
    const auto properties = _physicalDevice.getProperties();
    PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
}

void SwapChain::initSwapChain(const vk::Device& device, ResourceAllocator* const allocator, const vk::SampleCountFlagBits msaa) {
    _allocator = allocator;
    _msaaSamples = msaa;

    createSwapChain(device);
    createImageViews(device);
    createColorResources(device);
    createRenderPass(device);
    createFramebuffers(device);
}

void SwapChain::createSwapChain(const vk::Device &device) {
    const auto capabilities = _physicalDevice.getSurfaceCapabilitiesKHR(_surface);
    const auto formats = _physicalDevice.getSurfaceFormatsKHR(_surface);
    const auto presentModes = _physicalDevice.getSurfacePresentModesKHR(_surface);

    const auto surfaceFormat = chooseSwapSurfaceFormat(formats);
    const auto presentMode = chooseSwapPresentMode(presentModes);
    const auto extent = chooseSwapExtent(capabilities, _window);

    auto minImageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && minImageCount > capabilities.maxImageCount) {
        minImageCount = capabilities.maxImageCount;
    }

    auto createInfo = vk::SwapchainCreateInfoKHR{};
    createInfo.surface = _surface;
    createInfo.minImageCount = minImageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.presentMode = presentMode;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    if (_graphicsFamily != _presentFamily) {
        const uint32_t queueFamilyIndices[]{ _graphicsFamily.value(), _presentFamily.value() };
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.clipped = vk::True;
    createInfo.oldSwapchain = nullptr;

    _swapChain = device.createSwapchainKHR(createInfo);
    _images = device.getSwapchainImagesKHR(_swapChain);
    _imageFormat = surfaceFormat.format;
    _imageExtent = extent;
}

void SwapChain::createImageViews(const vk::Device& device) {
    using namespace std::ranges;
    const auto toImageView = [this, &device](const auto& image) {
        constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;
        constexpr auto mipLevels = 1;
        const auto viewInfo = vk::ImageViewCreateInfo{
                    {}, image, vk::ImageViewType::e2D, _imageFormat, {}, { aspectFlags, 0, mipLevels, 0, 1 } };
        return device.createImageView(viewInfo);
    };
    const auto imageViews = _images | views::transform(toImageView);
    _imageViews = std::vector(imageViews.begin(), imageViews.end());
}

void SwapChain::createColorResources(const vk::Device& device) {
    using Usage = vk::ImageUsageFlagBits;
    constexpr auto mipLevels = 1;
    constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;

    auto allocation = VmaAllocation{};
    _colorImage = _allocator->createColorAttachmentImage(
        _imageExtent.width, _imageExtent.height, mipLevels, _msaaSamples, _imageFormat,
        vk::ImageTiling::eOptimal, Usage::eTransientAttachment | Usage::eColorAttachment, &allocation);
    _colorImageAllocation = allocation;

    const auto viewInfo = vk::ImageViewCreateInfo{
        {}, _colorImage, vk::ImageViewType::e2D, _imageFormat, {}, { aspectFlags, 0, mipLevels, 0, 1 } };
    _colorImageView = device.createImageView(viewInfo);
}

void SwapChain::createRenderPass(const vk::Device& device) {
    const auto colorAttachment = vk::AttachmentDescription{
        {}, _imageFormat, _msaaSamples,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal };
    const auto colorAttachmentResolve = vk::AttachmentDescription{
        {}, _imageFormat, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR };

    static constexpr auto colorAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };
    static constexpr auto colorAttachmentResolveRef = vk::AttachmentReference{ 1, vk::ImageLayout::eColorAttachmentOptimal };

    constexpr auto subpass = vk::SubpassDescription{
        {}, vk::PipelineBindPoint::eGraphics, {}, {}, 1, &colorAttachmentRef, &colorAttachmentResolveRef };

    const auto attachments = std::array{ colorAttachment, colorAttachmentResolve };

    auto dependency = vk::SubpassDependency{};
    // In subpass zero...
    dependency.dstSubpass = 0;
    // ... at these pipeline stages ...
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // ... wait before performing these operations ...
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    // ... until all operations of these types ...
    dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
    // ... at these stages ...
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    // ... occuring in submission order prior to vkCmdBeginRenderPass have completed execution.
    dependency.srcSubpass = vk::SubpassExternal;

    const auto renderPassInfo = vk::RenderPassCreateInfo{
        {}, static_cast<uint32_t>(attachments.size()), attachments.data(), 1, &subpass, 1, &dependency };
    _renderPass = device.createRenderPass(renderPassInfo);
}

void SwapChain::createFramebuffers(const vk::Device& device) {
    const auto toFramebuffer = [this, &device](const auto& swapChainImageView) {
        const auto attachments = std::array{ _colorImageView, swapChainImageView };
        const auto framebufferInfo = vk::FramebufferCreateInfo{
            {}, _renderPass, static_cast<uint32_t>(attachments.size()), attachments.data(),
            _imageExtent.width, _imageExtent.height, 1 };
        return device.createFramebuffer(framebufferInfo);
    };
    const auto framebuffers = _imageViews | std::ranges::views::transform(toFramebuffer);
    _framebuffers = std::vector(framebuffers.begin(), framebuffers.end());
}

vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    constexpr auto format = vk::Format::eB8G8R8A8Srgb;
    constexpr auto colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    const auto suitable = [](const auto& it){ return it.format == format && it.colorSpace == colorSpace; };
    if (const auto found = std::ranges::find_if(availableFormats, suitable); found != availableFormats.end()) {
        return *found;
    }
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    constexpr auto preferredMode = vk::PresentModeKHR::eMailbox;
    if (const auto found = std::ranges::find(availablePresentModes, preferredMode); found != availablePresentModes.end()) {
        return preferredMode;
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* const window) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    auto actualExtent = vk::Extent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    actualExtent.width = std::clamp(
        actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

void SwapChain::recreate(const vk::Device& device) {
    // We won't recreate while the window is being minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    cleanup(device);

    createSwapChain(device);
    createImageViews(device);

    createColorResources(device);
    createFramebuffers(device);
}

void SwapChain::cleanup(const vk::Device &device) const noexcept {
    // Destroy all attachments
    device.destroyImageView(_colorImageView);
    _allocator->destroyImage(_colorImage, static_cast<VmaAllocation>(_colorImageAllocation));

    // Destroy the framebuffers and swap chain image views
    using namespace std::ranges;
    for_each(_framebuffers, [&device](const auto& it) { device.destroyFramebuffer(it); });
    for_each(_imageViews, [&device](const auto& it) { device.destroyImageView(it); });

    // Get rid of the old swap chain
    device.destroySwapchainKHR(_swapChain);
}
