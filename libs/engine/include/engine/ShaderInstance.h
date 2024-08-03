#pragma once

#include "engine/Renderer.h"
#include "engine/Shader.h"

#include <vulkan/vulkan.hpp>

#include <array>
#include <cstdint>
#include <vector>


class Engine;
class UniformBuffer;

class ShaderInstance {
    friend class Shader;

public:
    ShaderInstance(const ShaderInstance&) = delete;
    ShaderInstance& operator=(const ShaderInstance&) = delete;

    void setDescriptorData(uint32_t binding, const UniformBuffer* uniformBuffer, const Engine& engine) const;

    [[nodiscard]] const Shader* getShader() const;

    [[nodiscard]] vk::DescriptorPool getNativeDescriptorPool() const;
    [[nodiscard]] const vk::DescriptorSet& getNativeDescriptorSetAt(uint32_t frame) const;
    [[nodiscard]] const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& getNativeDescriptorSets() const;

private:
    ShaderInstance(
        const Shader* shader,
        const vk::DescriptorPool& descriptorPool,
        const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& descriptorSets);

    const Shader* _shader;
    vk::DescriptorPool _descriptorPool;

    std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()> _descriptorSets;
};
