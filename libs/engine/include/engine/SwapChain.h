#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>


class SwapChain {
public:

private:
    SwapChain(
        const vk::SurfaceCapabilitiesKHR& capabilities,
        const std::vector<vk::SurfaceFormatKHR>& formats,
        const std::vector<vk::PresentModeKHR>& presentModes,
        void* nativeWindow
    );

    vk::SwapchainKHR _swapChain{};
    std::vector<vk::Image> _swapChainImages{};
    std::vector<vk::ImageView> _swapChainImageViews{};
    vk::Format _swapChainImageFormat{ vk::Format::eUndefined };
    vk::Extent2D _swapChainImageExtent{};

    friend class Engine;
};
