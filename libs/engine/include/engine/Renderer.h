#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

#include "engine/SwapChain.h"


class Renderer {
public:
    enum class Pipeline {
        Particle,
    };

    bool beginFrame(SwapChain* swapChain);
    void endFrame();

    void compute(double frameTimeSeconds) const;
    void render() const;

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

private:
    Renderer() = default;

    vk::Device _device{};
    vk::Queue _graphicsQueue{};
    vk::Queue _presentQueue{};
    vk::Queue _computeQueue{};

    bool _framebufferResized = false;
    static void framebufferResizeCallback(GLFWwindow*, int, int);

    vk::RenderPass _renderPass{};

    vk::DescriptorSetLayout _drawingDescriptorSetLayout{};
    vk::DescriptorSetLayout _computeDescriptorSetLayout{};

    vk::PipelineLayout _drawingPipelineLayout{};
    vk::PipelineLayout _computePipelineLayout{};

    vk::Pipeline _drawingPipeline{};
    vk::Pipeline _computePipeline{};

    static constexpr auto MAX_FRAMES_IN_FLIGHT = 2;
    static constexpr auto PARTICLE_COUNT = 8192;

    std::vector<vk::Buffer> _shaderStorageBuffers{};
    std::vector<void*> _shaderStorageBuffersAllocation{};

    vk::DescriptorPool _descriptorPool{};
    std::vector<vk::DescriptorSet> _computeDescriptorSets{};

    std::vector<vk::CommandBuffer> _drawingCommandBuffers{};
    std::vector<vk::CommandBuffer> _computeCommandBuffers{};
    void recordDrawingCommands(const vk::CommandBuffer& buffer, uint32_t imageIndex) const;
    void recordComputeCommands(const vk::CommandBuffer& buffer, uint32_t currentFrame, float deltaTime) const;

    std::vector<vk::Semaphore> _imageAvailableSemaphores{};
    std::vector<vk::Semaphore> _renderFinishedSemaphores{};
    std::vector<vk::Fence> _drawingInFlightFences{};

    std::vector<vk::Semaphore> _computeFinishedSemaphores;
    std::vector<vk::Fence> _computeInFlightFences;

    uint32_t _currentFrame = 0;
    SwapChain* _swapChain{ nullptr };
    vk::ResultValue<uint32_t> _imageIndex{ vk::Result::eErrorUnknown, 0 };

    friend class Engine;
};
