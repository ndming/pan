#pragma once

#include "engine/Renderer.h"

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

    void setPushConstantData(uint32_t index, const void* data);

    void setDescriptorData(uint32_t binding, const UniformBuffer* uniformBuffer, const Engine& engine) const;
    void setDescriptorData(uint32_t binding, const std::vector<const UniformBuffer*>& uniformBuffers, const Engine& engine) const;

    [[nodiscard]] const Shader* getShader() const;

    [[nodiscard]] vk::DescriptorPool getNativeDescriptorPool() const;
    [[nodiscard]] vk::DescriptorSet getNativeDescriptorSetAt(uint32_t frame) const;
    [[nodiscard]] const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& getNativeDescriptorSets() const;

private:
    ShaderInstance(
        const Shader* shader, const vk::DescriptorPool& descriptorPool, std::size_t pushConstantCount,
        const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& descriptorSets);

    const Shader* _shader;
    vk::DescriptorPool _descriptorPool;

    std::vector<const void*> _pushConstantValues;
    std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()> _descriptorSets;
};
