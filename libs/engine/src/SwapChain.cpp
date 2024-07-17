#include "engine/SwapChain.h"

#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"

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
    PLOG_DEBUG << "Compute queue family index:  " << _computeFamily.value_or("none");
#endif
    // Print the device name
    const auto properties = _physicalDevice.getProperties();
    PLOG_INFO << "Found a suitable device: " << properties.deviceName.data();
}

void SwapChain::createSwapChain(const vk::Device& device, ResourceAllocator* const allocator) {
    _allocator = allocator;

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

vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats) {
    using namespace std::ranges;
    constexpr auto format = vk::Format::eB8G8R8A8Srgb;
    constexpr auto colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    const auto suitable = [](const auto& it){ return it.format == format && it.colorSpace == colorSpace; };
    if (const auto found = find_if(availableFormats, suitable); found != availableFormats.end()) {
        return *found;
    }
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    using namespace std::ranges;
    constexpr auto preferredMode = vk::PresentModeKHR::eMailbox;
    if (const auto found = find(availablePresentModes, preferredMode); found != availablePresentModes.end()) {
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

void SwapChain::recreate(const vk::Device& device, const vk::RenderPass& renderPass) {
    // We won't recreate while the window is being minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    device.destroyImageView(_colorImageView);
    _allocator->destroyImage(_colorImage, static_cast<VmaAllocation>(_colorImageAllocation));

    using namespace std::ranges;
    for_each(_framebuffers, [&device](const auto& it) { device.destroyFramebuffer(it); });
    for_each(_imageViews, [&device](const auto& it) { device.destroyImageView(it); });

    device.destroySwapchainKHR(_nativeObject);

    const auto newSwapChain = SwapChainBuilder(_graphicsFamily, _presentFamily)
        .surfaceFormat(chooseSwapSurfaceFormat(_formats))
        .presentMode(chooseSwapPresentMode(_presentModes))
        .extent(chooseSwapExtent(_capabilities, _window))
        .minImageCount(_minImageCount)
        .imageUsage(vk::ImageUsageFlagBits::eColorAttachment)
        .preTransform(_capabilities.currentTransform)
        .sampleCount(_msaa)
        .compositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .clipped(vk::True)
        .build(device, _surface, _allocator);

    const auto extent = chooseSwapExtent(_capabilities, _window);
    PLOGD << "In recreate swap chain - new swap extent: " << extent.width << " | " << extent.height;

    _nativeObject = newSwapChain->_nativeObject;
    _images = std::move(newSwapChain->_images);
    _imageViews = std::move(newSwapChain->_imageViews);
    _imageExtent = newSwapChain->_imageExtent;
    _colorImage = newSwapChain->_colorImage;
    _colorImageView = newSwapChain->_colorImageView;
    _colorImageAllocation = newSwapChain->_colorImageAllocation;

    using namespace std::ranges;
    const auto toFramebuffer = [this, &renderPass, &device](const auto& swapChainImageView) {
        const auto attachments = std::array{ _colorImageView, swapChainImageView };
        const auto framebufferInfo = vk::FramebufferCreateInfo{
            {}, renderPass, static_cast<uint32_t>(attachments.size()), attachments.data(),
            _imageExtent.width, _imageExtent.height, 1 };
        return device.createFramebuffer(framebufferInfo);
    };
    _framebuffers = _imageViews | views::transform(toFramebuffer) | to<std::vector>();

    delete newSwapChain;
}

void SwapChain::recreateSwapChain(const vk::Device& device) {
    // We won't recreate while the window is being minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }
    device.waitIdle();

    cleanupSwapChain(device);


}

void SwapChain::cleanupSwapChain(const vk::Device& device) const noexcept {
    // Destroy all attachments
    device.destroyImageView(_colorImageView);
    _allocator->destroyImage(_colorImage, static_cast<VmaAllocation>(_colorImageAllocation));

    // Destroy the framebuffers and swap chain image views
    using namespace std::ranges;
    for_each(_framebuffers, [&device](const auto& it) { device.destroyFramebuffer(it); });
    for_each(_imageViews, [&device](const auto& it) { device.destroyImageView(it); });

    // Get rid of the old swap chain
    device.destroySwapchainKHR(_nativeObject);
}


