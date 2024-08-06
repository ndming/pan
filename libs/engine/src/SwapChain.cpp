#include "engine/SwapChain.h"
#include "engine/Engine.h"

#include "bootstrap/PhysicalDeviceSelector.h"
#include "bootstrap/QueueFamilyFinder.h"

#include "allocator/ResourceAllocator.h"

#include <GLFW/glfw3.h>
#include <plog/Log.h>

#include <ranges>


SwapChain::SwapChain(
    GLFWwindow* const window,
    const vk::Instance& instance,
    const EngineFeature& feature,
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
        .select(instance.enumeratePhysicalDevices(), _surface, feature);

    // Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize,
    // it is not guaranteed to happen. That’s why we’ll add some extra code to also handle resizes explicitly
    glfwSetWindowUserPointer(_window, this);
    glfwSetFramebufferSizeCallback(_window, framebufferResizeCallback);

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
        PLOGE << "Could not find a suitable GPU: try requesting less features or updating your driver";
        throw std::runtime_error("Failed to find a suitable GPU!");
    }

#ifndef NDEBUG
    PLOGD << "Graphics queue family index: " << _graphicsFamily.value();
    PLOGD << "Present queue family index:  " << _presentFamily.value();

    if (_computeFamily.has_value()) {
        PLOGD << "Compute queue family index:  " << _computeFamily.value();
    }
#endif

    // Print the device name
    const auto properties = _physicalDevice.getProperties();
    PLOGI << "Found a suitable device: " << properties.deviceName.data();
}

void SwapChain::framebufferResizeCallback(GLFWwindow* window, [[maybe_unused]] const int width, [[maybe_unused]] const int height) {
    const auto swapChain = static_cast<SwapChain*>(glfwGetWindowUserPointer(window));
    swapChain->_framebufferResized = true;
}

void SwapChain::init(const vk::Device& device, ResourceAllocator* const allocator, const MSAA level) {
    _allocator = allocator;
    _presentQueue = device.getQueue(_presentFamily.value(), 0);
    _msaaSamples = getOrFallbackSampleCount(level, getNativeMaxUsableSampleCount());

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
    _minImageCount = capabilities.minImageCount + 1;
    // We should also make sure to not exceed the maximum number of images while doing this, where 0 is a special value
    // that means that there is no maximum
    if (capabilities.maxImageCount > 0 && _minImageCount > capabilities.maxImageCount) {
        _minImageCount = capabilities.maxImageCount;
    }

    auto createInfo = vk::SwapchainCreateInfoKHR{};
    createInfo.surface = _surface;
    createInfo.minImageCount = _minImageCount;
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
    _colorImage = _allocator->allocateColorAttachmentImage(
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

    // We should keep the order we specify these attachments in mind because later on we will have to specify
    // clear values for them in the same order
    const auto attachments = std::array{ colorAttachment, colorAttachmentResolve };

    static constexpr auto colorAttachmentRef = vk::AttachmentReference{ 0, vk::ImageLayout::eColorAttachmentOptimal };
    // Instruct the render pass to resolve the multisampled color image into a presentable attachment
    static constexpr auto colorAttachmentResolveRef = vk::AttachmentReference{ 1, vk::ImageLayout::eColorAttachmentOptimal };

    constexpr auto subpass = vk::SubpassDescription{
        {}, vk::PipelineBindPoint::eGraphics,
        {}, {},
        1, &colorAttachmentRef,
        &colorAttachmentResolveRef    // let the render pass define a multisample resolve operation
    };

    // Subpasses in a render pass automatically take care of image layout transitions. These transitions are controlled
    // by subpass dependencies, which specify memory and execution dependencies between subpasses. We have only a single
    // subpass right now, but the operations right before and right after this subpass also count as implicit "subpasses"
    // There are two built-in dependencies that take care of the transition at the start of the render pass and at the
    // end of the render pass, but the former does not occur at the right time. It assumes that the transition occurs
    // at the start of the pipeline, but we haven’t acquired the image yet at that point because the Renderer uses
    // eColorAttachmentOutput for the drawing wait stage. There are two ways to deal with this problem:
    // - In the Renderer, change the waitStages for the imageAvailableSemaphore to eTopOfPipe to ensure that the render
    // passes don’t begin until the image is available
    // - Make the render pass wait for the eColorAttachmentOutput stage, using subpass dependency
    constexpr auto dependency = vk::SubpassDependency{
        vk::SubpassExternal,  // src subpass
        0,                    // dst subpass
        vk::PipelineStageFlagBits::eColorAttachmentOutput,  // src stage
        vk::PipelineStageFlagBits::eColorAttachmentOutput,  // dst stage
        vk::AccessFlagBits::eColorAttachmentWrite,  // src access
        vk::AccessFlagBits::eColorAttachmentWrite,  // dst access
    };

    const auto renderPassInfo = vk::RenderPassCreateInfo{ {}, attachments, subpass, dependency };
    _renderPass = device.createRenderPass(renderPassInfo);
}

void SwapChain::createFramebuffers(const vk::Device& device) {
    const auto toFramebuffer = [this, &device](const auto& swapChainImageView) {
        // Specify the image views that should be bound to the respective attachment descriptions in the render pass
        const auto attachments = std::array{ _colorImageView, swapChainImageView };
        const auto framebufferInfo = vk::FramebufferCreateInfo{
            {}, _renderPass, attachments, _imageExtent.width, _imageExtent.height,
            1  // Our swap chain images are single images, so the number of layers is 1
        };
        return device.createFramebuffer(framebufferInfo);
    };
    // The swap chain attachment differs for every swap chain image, but the same color and depth image can be used by
    // all of them because only a single subpass is running at the same time due to our semaphores.
    const auto framebuffers = _imageViews | std::ranges::views::transform(toFramebuffer);
    _framebuffers = std::vector(framebuffers.begin(), framebuffers.end());
}

bool SwapChain::acquire(const vk::Device& device, const uint64_t timeout, const vk::Semaphore& semaphore, uint32_t* imageIndex) {
    const auto result = device.acquireNextImageKHR(_swapChain, timeout, semaphore, nullptr);
    if (result.result == vk::Result::eErrorOutOfDateKHR) {
        // Vulkan will usually just tell us that the swap chain is no longer adequate during presentation.
        // ERROR_OUT_OF_DATE means the swap chain has become incompatible with the surface and can no longer be used
        // for rendering, usually happens after a window resize. If that's the case, we recreate the swap chain and
        // return false to signal the call site to terminate the current rendering attempt
        recreate(device);
        return false;
    }
    if (result.result != vk::Result::eSuccess && result.result != vk::Result::eSuboptimalKHR) {
        // VK_SUBOPTIMAL_KHR means the swap chain can still be used to successfully present to the surface,
        // but the surface properties are no longer matched exactly. If this is not the case and the acquisition
        // result come out failed, then there's must be something wrong.
        throw std::runtime_error("Failed to acquire swap chain image!");
    }
    // Sucessfully acquire an image
    *imageIndex = result.value;
    return true;
}

void SwapChain::present(const vk::Device& device, uint32_t imageIndex, const vk::Semaphore& semaphore) {
    const auto presentInfo = vk::PresentInfoKHR{ semaphore, _swapChain, imageIndex };
    if (const auto result = _presentQueue.presentKHR(&presentInfo);
        result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || _framebufferResized) {
        _framebufferResized = false;
        recreate(device);
    } else if (result != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present a swap chain image!");
    }
}

void SwapChain::recreate(const vk::Device& device) {
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

    createSwapChain(device);
    createImageViews(device);
    createColorResources(device);
    createFramebuffers(device);

    _customFramebufferResizeCallback(_imageExtent.width, _imageExtent.height);

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

vk::SampleCountFlagBits SwapChain::getSampleCount(const MSAA msaaLevel) {
    switch (msaaLevel) {
        case MSAA::x2:  return vk::SampleCountFlagBits::e2;
        case MSAA::x4:  return vk::SampleCountFlagBits::e4;
        case MSAA::x8:  return vk::SampleCountFlagBits::e8;
        case MSAA::x16: return vk::SampleCountFlagBits::e16;
        case MSAA::x32: return vk::SampleCountFlagBits::e32;
        case MSAA::x64: return vk::SampleCountFlagBits::e64;
        default: throw std::runtime_error("Unrecognized MSAA level");
    }
}

vk::SampleCountFlagBits SwapChain::getOrFallbackSampleCount(const MSAA level, const vk::SampleCountFlagBits maxSampleCount) {
    if (const auto samples = getSampleCount(level); samples <= maxSampleCount) {
        return samples;
    }
    // Fallback to the max usable sample count
    using enum vk::SampleCountFlagBits;
    switch (maxSampleCount) {
    case e2:  PLOGW << "Falling back MSAA configuration: your device only supports up to 2x MSAA";  return e2;
    case e4:  PLOGW << "Falling back MSAA configuration: your device only supports up to 4x MSAA";  return e4;
    case e8:  PLOGW << "Falling back MSAA configuration: your device only supports up to 8x MSAA";  return e8;
    case e16: PLOGW << "Falling back MSAA configuration: your device only supports up to 16x MSAA"; return e16;
    case e32: PLOGW << "Falling back MSAA configuration: your device only supports up to 32x MSAA"; return e32;
    case e64: PLOGW << "Falling back MSAA configuration: your device only supports up to 64x MSAA"; return e64;
    default:
        PLOGE << "Received unvalid sample count: your device might not support MSAA";
        throw std::runtime_error("The device must support at least 2x MSAA");
    }
}

void SwapChain::setOnFramebufferResize(const std::function<void(uint32_t, uint32_t)>& callback) {
    _customFramebufferResizeCallback = callback;
}

void SwapChain::setOnFramebufferResize(std::function<void(uint32_t, uint32_t)>&& callback) noexcept {
    _customFramebufferResizeCallback = std::move(callback);
}

float SwapChain::getAspectRatio() const {
    return static_cast<float>(_imageExtent.width) /  static_cast<float>(_imageExtent.height);
}

uint32_t SwapChain::getGraphicsQueueFamily() const {
    return _graphicsFamily.value();
}

uint32_t SwapChain::getImageCount() const {
    return _imageViews.size();
}

uint32_t SwapChain::getMinImageCount() const {
    return _minImageCount;
}

vk::PhysicalDevice SwapChain::getNativePhysicalDevice() const {
    return _physicalDevice;
}

vk::Extent2D SwapChain::getNativeSwapImageExtent() const {
    return _imageExtent;
}

vk::RenderPass SwapChain::getNativeRenderPass() const {
    return _renderPass;
}

vk::SampleCountFlagBits SwapChain::getNativeSampleCount() const {
    return _msaaSamples;
}

vk::SampleCountFlagBits SwapChain::getNativeMaxUsableSampleCount() const {
    const auto physicalDeviceProperties = _physicalDevice.getProperties();
    const auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8)  { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4)  { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2)  { return vk::SampleCountFlagBits::e2; }
    return vk::SampleCountFlagBits::e1;
}

const vk::Framebuffer& SwapChain::getNativeFramebufferAt(const uint32_t imageIndex) const {
    return _framebuffers[imageIndex];
}
