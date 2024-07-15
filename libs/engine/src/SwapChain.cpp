#include <ranges>

#include <plog/Log.h>
#include <GLFW/glfw3.h>

#include "bootstrap/SwapChainBuilder.h"

#include "ResourceAllocator.h"

#include "engine/SwapChain.h"


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
