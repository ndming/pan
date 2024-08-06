#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>


struct EngineFeature;
struct GLFWwindow;
class ResourceAllocator;

class SwapChain final {
    // The Engine needs access to the constructor and initSwapChain method when creating and populating the SwapChain
    // These are the cases where an 'internal' access specifier like that from the Kotlin language comes in handy
    friend class Engine;

    // Likewise, the Renderer needs access to the acquire and present methods of SwapChain
    friend class Renderer;

public:
    enum class MSAA {
        x2,
        x4,
        x8,
        x16,
        x32,
        x64,
    };

    void setOnFramebufferResize(const std::function<void(uint32_t, uint32_t)>& callback);
    void setOnFramebufferResize(std::function<void(uint32_t, uint32_t)>&& callback) noexcept;

    [[nodiscard]] float getAspectRatio() const;
    [[nodiscard]] uint32_t getGraphicsQueueFamily() const;
    [[nodiscard]] uint32_t getImageCount() const;
    [[nodiscard]] uint32_t getMinImageCount() const;

    [[nodiscard]] vk::PhysicalDevice getNativePhysicalDevice() const;
    [[nodiscard]] vk::Extent2D getNativeSwapImageExtent() const;
    [[nodiscard]] vk::RenderPass getNativeRenderPass() const;
    [[nodiscard]] vk::SampleCountFlagBits getNativeSampleCount() const;
    [[nodiscard]] const vk::Framebuffer& getNativeFramebufferAt(uint32_t imageIndex) const;

    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

private:
    SwapChain(
        GLFWwindow* window,
        const vk::Instance& instance,
        const EngineFeature& feature,
        const std::vector<const char*>& extensions);

    // "Internal" operations
    void init(const vk::Device& device, ResourceAllocator* allocator, MSAA level);
    bool acquire(const vk::Device& device, uint64_t timeout, const vk::Semaphore& semaphore, uint32_t* imageIndex);
    void present(const vk::Device& device, uint32_t imageIndex, const vk::Semaphore& semaphore);

    // Truly private operations
    void createSwapChain(const vk::Device& device);
    void createImageViews(const vk::Device& device);
    void createColorResources(const vk::Device& device);
    void createRenderPass(const vk::Device& device);
    void createFramebuffers(const vk::Device& device);

    void recreate(const vk::Device& device);
    void cleanup(const vk::Device& device) const noexcept;

    // Utility static methods
    using SurfaceFormat = vk::SurfaceFormatKHR;
    using PresentMode = vk::PresentModeKHR;
    using SurfaceCapabilities = vk::SurfaceCapabilitiesKHR;
    [[nodiscard]] static SurfaceFormat chooseSwapSurfaceFormat(const std::vector<SurfaceFormat>& availableFormats);
    [[nodiscard]] static PresentMode chooseSwapPresentMode(const std::vector<PresentMode>& availablePresentModes);
    [[nodiscard]] static vk::Extent2D chooseSwapExtent(const SurfaceCapabilities& capabilities, GLFWwindow* window);
    [[nodiscard]] static vk::SampleCountFlagBits getSampleCount(MSAA msaaLevel);
    [[nodiscard]] static vk::SampleCountFlagBits getOrFallbackSampleCount(MSAA level, vk::SampleCountFlagBits maxSampleCount);

    GLFWwindow* _window;
    vk::SurfaceKHR _surface;

    bool _framebufferResized{ false };
    std::function<void(uint32_t, uint32_t)> _customFramebufferResizeCallback{ [](uint32_t, uint32_t) {} };
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

    vk::PhysicalDevice _physicalDevice;
    std::optional<uint32_t> _graphicsFamily;
    std::optional<uint32_t> _presentFamily;
    std::optional<uint32_t> _computeFamily;

    vk::Queue _presentQueue{};

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
    uint32_t _minImageCount{ 0 };

    // Color attachment (render target)
    vk::Image _colorImage{};
    vk::ImageView _colorImageView{};
    void* _colorImageAllocation{ nullptr };

    // To create framebuffers, we need to specify how many color and depth buffers there will be, how many samples to
    // use for each of them and how their contents should be handled throughout the rendering operations. All of this
    // information is wrapped in a render pass object.
    vk::RenderPass _renderPass{};

    // Given that most devices now support MSAA, we either go with MSAA or we don't go at all (throw exception)
    vk::SampleCountFlagBits _msaaSamples{ vk::SampleCountFlagBits::e2 };
    [[nodiscard]] vk::SampleCountFlagBits getNativeMaxUsableSampleCount() const;

    // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object.
    // The image that we have to use for the attachment depends on which image the swap chain returns when we retrieve
    // one for presentation. That means that we have to create a framebuffer for all of the images in the swap chain and
    // use the one that corresponds to the retrieved image at drawing time
    std::vector<vk::Framebuffer> _framebuffers{};
};
