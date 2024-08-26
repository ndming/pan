#include "engine/Overlay.h"
#include "engine/Engine.h"
#include "engine/SwapChain.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <plog/Log.h>


static void checkVkResult(const VkResult err) {
    if (err == 0) return;

    PLOGE << "Received ImGUI error: " << err;
    if (err < 0) abort();
}

static vk::DescriptorPool m_pool{};

void Overlay::init(Surface* const surface, const Engine& engine, const SwapChain& swapChain) {
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForVulkan(surface, true);

    const auto device = engine.getNativeDevice();

    // Create a descriptor pool tailored for Dear ImGUI
    constexpr auto poolSizes = std::array{
        vk::DescriptorPoolSize{ vk::DescriptorType::eCombinedImageSampler, 1 },
    };
    const auto poolInfo = vk::DescriptorPoolCreateInfo{
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
        static_cast<uint32_t>(Renderer::getMaxFramesInFlight()), poolSizes };
    m_pool = device.createDescriptorPool(poolInfo);

    auto initInfo = ImGui_ImplVulkan_InitInfo{};
    initInfo.Instance = engine.getNativeInstance();
    initInfo.PhysicalDevice = swapChain.getNativePhysicalDevice();
    initInfo.MinImageCount = swapChain.getMinImageCount();
    initInfo.ImageCount = swapChain.getImageCount();
    initInfo.QueueFamily = swapChain.getGraphicsQueueFamily();
    initInfo.Device = device;
    initInfo.Queue = device.getQueue(swapChain.getGraphicsQueueFamily(), 0);
    initInfo.RenderPass = swapChain.getNativeRenderPass();
    initInfo.Subpass = 0;
    initInfo.MSAASamples = static_cast<VkSampleCountFlagBits>(swapChain.getNativeSampleCount());
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = m_pool;
    initInfo.Allocator = nullptr;              // optional
    initInfo.CheckVkResultFn = checkVkResult;  // optional
    ImGui_ImplVulkan_Init(&initInfo);
}

void Overlay::setMinImageCount(const uint32_t minImageCount) {
    ImGui_ImplVulkan_SetMinImageCount(minImageCount);
}

void Overlay::teardown(const Engine& engine) {
    // Have ImGUI clean up its internal resources
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // Clean up resource created explicitly for ImGUI
    const auto device = engine.getNativeDevice();
    device.destroyDescriptorPool(m_pool);
}
