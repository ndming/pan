#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <plog/Log.h>

#include "Context.h"


static void check_vk_result(const VkResult err) {
    if (err == 0)
        return;
    fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
    if (err < 0)
        abort();
}

std::unique_ptr<Context> Context::create(const std::string_view name, const int width, const int height) {
    return std::unique_ptr<Context>(new Context{ name, width, height });
}

Context::Context(const std::string_view name, const int width, const int height) {
    glfwInit(); glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    _window = glfwCreateWindow(width, height, name.data(), nullptr, nullptr);
}

void Context::destroy() const noexcept {
    glfwDestroyWindow(_window);
    glfwTerminate();
}

GLFWwindow* Context::getNativeWindow() const {
    return _window;
}

void Context::initGUI(const Engine* const engine, const SwapChain* const swapChain, const Renderer* const renderer) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    _io = ImGui::GetIO(); (void)_io;
    _io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    _io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForVulkan(_window, true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = engine->_instance;
    init_info.PhysicalDevice = engine->_physicalDevice;
    init_info.Device = engine->_device;
    init_info.QueueFamily = engine->_graphicsFamily.value();
    init_info.Queue = engine->_graphicsQueue;
    init_info.PipelineCache = nullptr;
    init_info.DescriptorPool = renderer->_descriptorPool;
    init_info.RenderPass = renderer->_renderPass;
    init_info.Subpass = 0;
    init_info.MinImageCount = swapChain->_minImageCount;
    init_info.ImageCount = Renderer::MAX_FRAMES_IN_FLIGHT;
    init_info.MSAASamples = static_cast<VkSampleCountFlagBits>(swapChain->_msaa);
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = check_vk_result;
    ImGui_ImplVulkan_Init(&init_info);
}

void Context::loop(const std::function<void(double)> &onFrame) const {
    while (!glfwWindowShouldClose(_window)) {
        glfwPollEvents();
        onFrame(glfwGetTime());
    }
}
