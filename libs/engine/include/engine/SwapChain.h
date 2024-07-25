#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <optional>
#include <vector>


struct GLFWwindow;
class ResourceAllocator;

class SwapChain final {
public:
    enum class MSAA {
        x1,
        x2,
        x4,
        x8,
        x16,
        x32,
        x64,
    };

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

private:
    SwapChain(
        GLFWwindow* window,
        const vk::Instance& instance,
        const vk::PhysicalDeviceFeatures& features,
        const std::vector<const char*>& extensions);

    void initSwapChain(const vk::Device& device, ResourceAllocator* allocator);

    void createSwapChain(const vk::Device& device);
    void createImageViews(const vk::Device& device);
    void createColorResources(const vk::Device& device);
    void createRenderPass(const vk::Device& device);
    void createFramebuffers(const vk::Device& device);

    using SurfaceFormat = vk::SurfaceFormatKHR;
    using PresentMode = vk::PresentModeKHR;
    using SurfaceCapabilities = vk::SurfaceCapabilitiesKHR;
    [[nodiscard]] static SurfaceFormat chooseSwapSurfaceFormat(const std::vector<SurfaceFormat>& availableFormats);
    [[nodiscard]] static PresentMode chooseSwapPresentMode(const std::vector<PresentMode>& availablePresentModes);
    [[nodiscard]] static vk::Extent2D chooseSwapExtent(const SurfaceCapabilities& capabilities, GLFWwindow* window);

    void recreate(const vk::Device& device, vk::SampleCountFlagBits msaa);
    void cleanup(const vk::Device& device) const noexcept;

    GLFWwindow* _window;
    vk::SurfaceKHR _surface;

    vk::PhysicalDevice _physicalDevice;
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _presentFamily;
    std::optional<uint32_t> _computeFamily;

    // Vulkan does not have the concept of a "default framebuffer", hence it requires an infrastructure that will
    // own the buffers we will render to before we visualize them on the screen. The swap chain is essentially a queue
    // of images that are waiting to be presented to the screen. Our application will acquire such an image to draw to
    // it, and then return it to the queue. The general purpose of the swap chain is to synchronize the presentation
    // of images with the refresh rate of the screen
    vk::SwapchainKHR _swapChain{};

    // Store a pointer to our allocator since we will be destroying and recreating resources throughout the swap chain
    // life time. Since the SwapChain (should) never outlives the Engine, this approach is acceptable.
    ResourceAllocator* _allocator{ nullptr };

    std::vector<vk::Image> _images{};
    std::vector<vk::ImageView> _imageViews{};
    vk::Format _imageFormat{ vk::Format::eUndefined };
    vk::Extent2D _imageExtent{};

    // Color attachment (render target)
    vk::Image _colorImage{};
    vk::ImageView _colorImageView{};
    void* _colorImageAllocation{ nullptr };

    // To create framebuffers, we need to specify how many color and depth buffers there will be, how many samples to
    // use for each of them and how their contents should be handled throughout the rendering operations. All of this
    // information is wrapped in a render pass object.
    vk::RenderPass _renderPass{};
    vk::SampleCountFlagBits _msaaSamples{ vk::SampleCountFlagBits::e1 };

    // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object.
    // The image that we have to use for the attachment depends on which image the swap chain returns when we retrieve
    // one for presentation. That means that we have to create a framebuffer for all of the images in the swap chain and
    // use the one that corresponds to the retrieved image at drawing time
    std::vector<vk::Framebuffer> _framebuffers{};

    // The Engine needs access to the constructor and initSwapChain method when creating and populating the SwapChain
    // These are the cases where an 'internal' access specifier like that from the Kotlin language comes in handy
    friend class Engine;
};
