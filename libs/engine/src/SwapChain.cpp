#include <GLFW/glfw3.h>

#include "engine/SwapChain.h"


SwapChain::SwapChain(
    const vk::SurfaceCapabilitiesKHR& capabilities,
    const std::vector<vk::SurfaceFormatKHR>& formats,
    const std::vector<vk::PresentModeKHR>& presentModes,
    void* const nativeWindow
) {
    const auto window = static_cast<GLFWwindow*>(nativeWindow);
}
