#pragma once

#include <cstdint>
#include <vector>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


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
    SwapChain() = default;

    vk::SwapchainKHR _nativeObject{};
    std::vector<vk::Image> _images{};
    std::vector<vk::ImageView> _imageViews{};
    vk::SurfaceFormatKHR _imageFormat{};
    vk::Extent2D _imageExtent{};
    vk::SampleCountFlagBits _msaa{ vk::SampleCountFlagBits::e1 };

    GLFWwindow* _window{ nullptr };
    ResourceAllocator* _allocator{ nullptr };
    vk::SurfaceCapabilitiesKHR _capabilities{};
    vk::PresentModeKHR _presentMode{ vk::PresentModeKHR::eFifo };
    uint32_t _graphicsFamily{};
    uint32_t _presentFamily{};
    vk::SurfaceKHR _surface{};

    using SurfaceFormat = vk::SurfaceFormatKHR;
    using PresentMode = vk::PresentModeKHR;
    using SurfaceCapabilities = vk::SurfaceCapabilitiesKHR;
    [[nodiscard]] static SurfaceFormat chooseSwapSurfaceFormat(const std::vector<SurfaceFormat>& availableFormats);
    [[nodiscard]] static PresentMode chooseSwapPresentMode(const std::vector<PresentMode>& availablePresentModes);
    [[nodiscard]] static vk::Extent2D chooseSwapExtent(const SurfaceCapabilities& capabilities, GLFWwindow* window);

    // Color attachment (render target)
    vk::Image _colorImage{};
    vk::ImageView _colorImageView{};
    void* _colorImageAllocation{ nullptr };

    // The framebuffers will only be populated when a renderer is created,
    // by which time the render pass is available
    std::vector<vk::Framebuffer> _framebuffers{};

    void recreate(const vk::Device& device, const vk::RenderPass& renderPass);

    friend class Engine;
    friend class Renderer;
    friend class SwapChainBuilder;
};
