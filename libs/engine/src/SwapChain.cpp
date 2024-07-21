#include "engine/SwapChain.h"

#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"

#include "ResourceAllocator.h"

#include <GLFW/glfw3.h>
#include <plog/Log.h>

#include <ranges>


SwapChain::SwapChain(
    GLFWwindow* const window,
    const vk::Instance& instance,
    const vk::PhysicalDeviceFeatures& features,
    const std::vector<const char*>& extensions
) : _window{ window } {
    // Since Vulkan is a platform agnostic API, it can not interface directly with the window system on its own.
    // To establish the connection between Vulkan and the window system to present results to the screen, we need
    // to use the WSI (Window System Integration) extensions.
    if (glfwCreateWindowSurface(instance, window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&_surface)) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create a window surface!");
    }

    // The window surface needs to be created right after the instance creation, because it can actually influence
    // the physical device selection. Find a list of possible candidate devices:
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

void SwapChain::initSwapChain(const vk::Device& device, ResourceAllocator* const allocator) {
    _allocator = allocator;

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

    // We have to decide how many images we would like to have in the swap chain. Simply sticking to the minimum
    // means that we may sometimes have to wait on the driver to complete internal operations before we can acquire
    // another image to render to. Therefore, it is recommended to request at least one more image than the minimum
    auto minImageCount = capabilities.minImageCount + 1;
    // We should also make sure to not exceed the maximum number of images while doing this, where 0 is a special value
    // that means that there is no maximum
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
    createInfo.imageArrayLayers = 1;  // will always be 1 unless for a stereoscopic 3D application
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
    if (_graphicsFamily != _presentFamily) {
        // Concurrent mode requires us to specify in advance between which queue families ownership will be shared
        // using the queueFamilyIndexCount and pQueueFamilyIndices parameters
        const uint32_t queueFamilyIndices[]{ _graphicsFamily.value(), _presentFamily.value() };
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        // If the graphics queue family and presentation queue family are the same, which will be the case on most
        // hardware, then we should stick to exclusive mode
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }
    // Specify that a certain transform should be applied to images in the swap chain if it is supported
    // (supportedTransforms in capabilities), like a 90 degree clockwise rotation or horizontal flip
    // Use currentTransform to omit this
    createInfo.preTransform = capabilities.currentTransform;
    // Specify if the alpha channel should be used for blending with other windows in the window system
    // We’ll almost always want to simply ignore the alpha channel, hence VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR.
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    // We don’t care about the color of pixels that are obscured, for example because another window is in front of them
    createInfo.clipped = vk::True;
    // With Vulkan it’s possible that our swap chain becomes invalid or unoptimized while our application is running,
    // for example because the window was resized. In that case the swap chain actually needs to be recreated from
    // scratch and a reference to the old one must be specified in this field.
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
    if (_msaaSamples == vk::SampleCountFlagBits::e1) {
        // When MSAA is disabled, we are not allowed to create a resolving attachment for the color attachment,
        // therefore, we will treat this case separately
        const auto colorAttachment = vk::AttachmentDescription{
            {}, _imageFormat, _msaaSamples,
            vk::AttachmentLoadOp::eClear,
            vk::AttachmentStoreOp::eStore,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePresentSrcKHR,
        };

        static constexpr auto colorAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };

        constexpr auto subpass = vk::SubpassDescription{
            {}, vk::PipelineBindPoint::eGraphics,
            {}, {},
            1, &colorAttachmentRef,
        };

        constexpr auto dependency = vk::SubpassDependency{
            vk::SubpassExternal,  // src subpass
            0,                    // dst subpass
            vk::PipelineStageFlagBits::eColorAttachmentOutput,  // src stage
            vk::PipelineStageFlagBits::eColorAttachmentOutput,  // dst stage
            {},                                         // src access
            vk::AccessFlagBits::eColorAttachmentWrite,  // dst access
        };

        const auto renderPassInfo = vk::RenderPassCreateInfo{ {}, 1, &colorAttachment, 1, &subpass, 1, &dependency };
        _renderPass = device.createRenderPass(renderPassInfo);
    } else {
        const auto colorAttachment = vk::AttachmentDescription{
            {}, _imageFormat, _msaaSamples,
            vk::AttachmentLoadOp::eClear,   // clear the framebuffer to black before drawing a new frame
            vk::AttachmentStoreOp::eStore,  // rendered contents will be stored in memory to be resolved later
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::eColorAttachmentOptimal
        };
        const auto colorAttachmentResolve = vk::AttachmentDescription{
            {}, _imageFormat, vk::SampleCountFlagBits::e1,
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eStore,    // resolved contents will be stored in memory to be presented later
            vk::AttachmentLoadOp::eDontCare,
            vk::AttachmentStoreOp::eDontCare,
            vk::ImageLayout::eUndefined,
            vk::ImageLayout::ePresentSrcKHR   // we want the image to be ready for presentation after rendering
        };

        static constexpr auto colorAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };
        // Instruct the render pass to resolve multisampled color image into regular attachment
        static constexpr auto colorAttachmentResolveRef = vk::AttachmentReference{ 1, vk::ImageLayout::eColorAttachmentOptimal };

        constexpr auto subpass = vk::SubpassDescription{
            {}, vk::PipelineBindPoint::eGraphics,
            {}, {},
            1, &colorAttachmentRef,
            &colorAttachmentResolveRef    // let the render pass define a multisample resolve operation
        };

        const auto attachments = std::array{ colorAttachment, colorAttachmentResolve };

        auto dependency = vk::SubpassDependency{};
        // In subpass zero...
        dependency.dstSubpass = 0;
        // ... at these pipeline stages ...
        dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        // ... wait before performing these operations ...
        dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        // ... until all operations of these types ...
        // (ensures that any write operations to the attachments are completed before subsequent ones begin)
        dependency.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        // ... at these stages ...
        dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
        // ... occuring in submission order prior to vkCmdBeginRenderPass have completed execution.
        dependency.srcSubpass = vk::SubpassExternal;

        const auto renderPassInfo = vk::RenderPassCreateInfo{
            {}, static_cast<uint32_t>(attachments.size()), attachments.data(), 1, &subpass, 1, &dependency };
        _renderPass = device.createRenderPass(renderPassInfo);
    }
}

void SwapChain::createFramebuffers(const vk::Device& device) {
    const auto toFramebuffer = [this, &device](const auto& swapChainImageView) {
        // If MSAA is disabled, we use swap chain's images as color targets directly
        const auto attachments = _msaaSamples == vk::SampleCountFlagBits::e1 ? std::vector{ swapChainImageView } :
            std::vector{ _colorImageView, swapChainImageView };
        const auto framebufferInfo = vk::FramebufferCreateInfo{
            {}, _renderPass,
            static_cast<uint32_t>(attachments.size()), attachments.data(),
            _imageExtent.width, _imageExtent.height,
            1  // Our swap chain images are single images, so the number of layers is 1
        };
        return device.createFramebuffer(framebufferInfo);
    };
    // The swap chain attachment differs for every swap chain image, but the same color and depth image can be used by
    // all of them because only a single subpass is running at the same time due to our semaphores.
    const auto framebuffers = _imageViews | std::ranges::views::transform(toFramebuffer);
    _framebuffers = std::vector(framebuffers.begin(), framebuffers.end());
}

vk::SurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats) {
    // Each VkSurfaceFormatKHR entry contains a format and a colorSpace member. The format member specifies the
    // color channels and types. For the color space we’ll use sRGB, which is pretty much the standard color space for
    // viewing and printing purposes, like the textures we’ll use later on
    constexpr auto format = vk::Format::eB8G8R8A8Srgb;
    // VK_FORMAT_B8G8R8A8_SRGB means that we store the B, G, R and alpha channels in that order with
    // an 8 bit unsigned integer for a total of 32 bits per pixel. Because we're using sRGB, we should also use
    // an sRGB color format, of which one of the most common ones is VK_FORMAT_B8G8R8A8_SRGB.
    constexpr auto colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
    const auto suitable = [](const auto& it){ return it.format == format && it.colorSpace == colorSpace; };
    if (const auto found = std::ranges::find_if(availableFormats, suitable); found != availableFormats.end()) {
        return *found;
    }
    // If our checkingfails then we could start ranking the available formats based on how "good" they are, but in
    // most cases it’s okay to just settle with the first format that is specified
    return availableFormats[0];
}

vk::PresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes) {
    // MAILBOX is a very nice trade-off if energy usage is not a concern. It allows us to avoid tearing while still
    // maintaining a fairly low latency by rendering new images that are as up-to-date as possible right until
    // the vertical blank. On mobile devices, where energy usage is more important, FIFO is more preferable
    constexpr auto preferredMode = vk::PresentModeKHR::eMailbox;
    if (const auto found = std::ranges::find(availablePresentModes, preferredMode); found != availablePresentModes.end()) {
        return preferredMode;
    }
    return vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities, GLFWwindow* const window) {
    // Vulkan tells us to match the resolution of the window set in the currentExtent member. However, some
    // window managers do allow us to differ here and this is indicated by setting the width and height in
    // currentExtent to a special value: the maximum value of uint32_t. In that case we’ll pick the resolution that
    // best matches the window within the minImageExtent and maxImageExtent bounds
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    // GLFW uses two units when measuring sizes: pixels and screen coordinates. For example, the resolution
    // {WIDTH, HEIGHT} that we specified when creating the window is measured in screen coordinates.
    // But Vulkan works with pixels, so the swap chain extent must be specified in pixels as well. Unfortunately,
    // if we are using a high DPI display (like Apple’s Retina display), screen coordinates don’t correspond to pixels.
    // Instead, due to the higher pixel density, the resolution of the window in pixel will be larger than
    // the resolution in screen coordinates. So if Vulkan doesn’t fix the swap extent for us, we can’t just use
    // the original {WIDTH, HEIGHT}. Instead, we must use glfwGetFramebufferSize to query the resolution of the window
    // in pixel before matching it against the minimum and maximum image extent.
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    auto actualExtent = vk::Extent2D{ static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
    actualExtent.width = std::clamp(
        actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

void SwapChain::recreate(const vk::Device& device, const vk::SampleCountFlagBits msaa) {
    // We won't recreate while the window is being minimized
    int width = 0, height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(_window, &width, &height);
        glfwWaitEvents();
    }

    // Before cleaning, we shouldn’t touch resources that may still be in use
    device.waitIdle();
    cleanup(device);

    // Changing MSAA samples affect the render pass, color resources, and potentially the framebuffers
    _msaaSamples = msaa;

    createSwapChain(device);
    createImageViews(device);
    createColorResources(device);
    createRenderPass(device);
    createFramebuffers(device);

    /* TODO: Make use of the oldSwapChain field
     * The disadvantage of this approach is that we need to stop all rendering before creating the new swap chain.
     * It is possible to create a new swap chain while drawing commands on an image from the old swap chain are still
     * in-flight. We need to pass the previous swap chain to the oldSwapchain field in the VkSwapchainCreateInfoKHR
     * struct and destroy the old swap chain as soon as we’ve finished using it */
}

void SwapChain::cleanup(const vk::Device& device) const noexcept {
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
