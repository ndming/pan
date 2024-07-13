#pragma once

#include <vector>

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>


class SwapChain {
public:
    SwapChain(const SwapChain&) = delete;
    SwapChain& operator=(const SwapChain&) = delete;

private:
    SwapChain() = default;

    vk::SwapchainKHR _swapChain{};
    std::vector<vk::Image> _swapChainImages{};
    std::vector<vk::ImageView> _swapChainImageViews{};
    vk::Format _swapChainImageFormat{ vk::Format::eUndefined };
    vk::Extent2D _swapChainImageExtent{};

    using SurfaceFormat = vk::SurfaceFormatKHR;
    using PresentMode = vk::PresentModeKHR;
    using SurfaceCapabilities = vk::SurfaceCapabilitiesKHR;
    [[nodiscard]] static SurfaceFormat chooseSwapSurfaceFormat(const std::vector<SurfaceFormat>& availableFormats);
    [[nodiscard]] static PresentMode chooseSwapPresentMode(const std::vector<PresentMode>& availablePresentModes);
    [[nodiscard]] static vk::Extent2D chooseSwapExtent(const SurfaceCapabilities& capabilities, GLFWwindow* window);

    friend class Engine;
};
