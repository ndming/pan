#pragma once

#include <string_view>

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include "engine/DeviceFeature.h"
#include "engine/Renderer.h"
#include "engine/SwapChain.h"


class Translator {
public:
    static vk::PhysicalDeviceFeatures toPhysicalDeviceFeatures(const std::vector<DeviceFeature>& features);
    static vk::SampleCountFlagBits toSupportSampleCount(SwapChain::MSAA msaa, const vk::PhysicalDevice& device);

    static vk::DescriptorSetLayout toDrawingDescriptorSetLayout(Renderer::Pipeline pipeline, const vk::Device& device);
    static vk::DescriptorSetLayout toComputeDescriptorSetLayout(Renderer::Pipeline pipeline, const vk::Device& device);

    static vk::PipelineLayout toDrawingPipelineLayout(
        Renderer::Pipeline pipeline, const vk::DescriptorSetLayout& layout, const vk::Device& device);
    static vk::PipelineLayout toComputePipelineLayout(
        Renderer::Pipeline pipeline, const vk::DescriptorSetLayout& layout, const vk::Device& device);

    static vk::Pipeline toDrawingPipeline(
        Renderer::Pipeline pipeline, const vk::PipelineLayout& layout, const vk::RenderPass& renderPass,
        vk::SampleCountFlagBits msaaSamples, const vk::Device& device);
    static vk::Pipeline toComputePipeline(
        Renderer::Pipeline pipeline, const vk::PipelineLayout& layout, const vk::Device& device);

private:
    static vk::SampleCountFlagBits getMaxUsableSampleCount(const vk::PhysicalDevice& device);
    static vk::SampleCountFlagBits toSampleCount(SwapChain::MSAA msaa);

    static vk::Pipeline createParticleDrawingPipeline(
        const vk::PipelineLayout& layout, const vk::RenderPass& renderPass,
        vk::SampleCountFlagBits msaaSamples, const vk::Device& device);
    static vk::Pipeline createParticleComputePipeline(const vk::PipelineLayout& layout, const vk::Device& device);

    static std::vector<char> readFile(std::string_view fileName);
    static vk::ShaderModule createShaderModule(const std::vector<char>& bytecode, const vk::Device& device);

    // Particle pipeline
    struct ParticleParam {
        float deltaTime{ 1.0f };
    };
    struct Particle {
        alignas(8)  glm::vec2 position;
        alignas(8)  glm::vec2 velocity;
        alignas(16) glm::vec4 color;
    };
    [[nodiscard]] constexpr static std::array<vk::VertexInputBindingDescription, 1> getParticleBindingDescriptions() {
        return std::array{
            vk::VertexInputBindingDescription{ 0, sizeof(Particle), vk::VertexInputRate::eVertex },
        };
    }
    [[nodiscard]] constexpr static std::array<vk::VertexInputAttributeDescription, 2> getParticleAttributeDescriptions() {
        return std::array{
            vk::VertexInputAttributeDescription{ 0, 0, vk::Format::eR32G32Sfloat, offsetof(Particle, position) },
            vk::VertexInputAttributeDescription{ 1, 0, vk::Format::eR32G32B32A32Sfloat, offsetof(Particle, color) },
        };
    }

    friend class Engine;
    friend class Renderer;
};
