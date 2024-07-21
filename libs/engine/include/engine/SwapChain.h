#pragma once

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <optional>
#include <vector>


struct GLFWwindow;
class ResourceAllocator;

class SwapChain {
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

    void initSwapChain(const vk::Device& device, ResourceAllocator* allocator, vk::SampleCountFlagBits msaa);

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

    void recreate(const vk::Device& device);
    void cleanup(const vk::Device& device) const noexcept;

    GLFWwindow* _window{ nullptr };
    vk::SurfaceKHR _surface{};

    vk::PhysicalDevice _physicalDevice{};
    std::optional<uint32_t> _graphicsFamily{};
    std::optional<uint32_t> _presentFamily{};
    std::optional<uint32_t> _computeFamily{};

    vk::SwapchainKHR _swapChain{};
    ResourceAllocator* _allocator{ nullptr };

    std::vector<vk::Image> _images{};
    std::vector<vk::ImageView> _imageViews{};
    vk::Format _imageFormat{ vk::Format::eUndefined };
    vk::Extent2D _imageExtent{};

    // Color attachment (render target)
    vk::Image _colorImage{};
    vk::ImageView _colorImageView{};
    void* _colorImageAllocation{ nullptr };

    vk::RenderPass _renderPass{};
    vk::SampleCountFlagBits _msaaSamples{ vk::SampleCountFlagBits::e1 };

    std::vector<vk::Framebuffer> _framebuffers{};

    friend class Engine;
    friend class Renderer;
};
