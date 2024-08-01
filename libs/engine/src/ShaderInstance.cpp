#include "engine/ShaderInstance.h"
#include "engine/Engine.h"
#include "engine/UniformBuffer.h"


ShaderInstance::ShaderInstance(
    const Shader* const shader,
    const vk::DescriptorPool& descriptorPool,
    const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& descriptorSets
) : _shader{ shader },
    _descriptorPool{ descriptorPool },
    _descriptorSets{ descriptorSets } {
}

void ShaderInstance::setPushConstantData(
    const Shader::Stage stage,
    const uint32_t byteOffset,
    const uint32_t byteSize,
    const void* data
) {
    _pushConstantInfos.emplace_back(
        _shader->getNativePipelineLayout(), Shader::getNativeShaderStage(stage), byteOffset, byteSize, data);
}

void ShaderInstance::setDescriptorData(const uint32_t binding, const UniformBuffer* const uniformBuffer, const Engine& engine) const {
    const auto device = engine.getNativeDevice();

    for (uint32_t i = 0; i < Renderer::getMaxFramesInFlight(); ++i) {
        const auto bufferInfo = vk::DescriptorBufferInfo{
            uniformBuffer->getNativeBuffer(),
            i * uniformBuffer->getBufferSize(),  // byte offset to the buffer for the i-th frame
            uniformBuffer->getDataSize()         // we are actually binding a subset of this buffer portion
        };

        const auto descriptorWrites = std::array{
            vk::WriteDescriptorSet{ _descriptorSets[i], binding, 0, 1, vk::DescriptorType::eUniformBuffer, {}, &bufferInfo },
        };

        device.updateDescriptorSets(descriptorWrites, {});
    }
}

const Shader* ShaderInstance::getShader() const {
    return _shader;
}

vk::DescriptorPool ShaderInstance::getNativeDescriptorPool() const {
    return _descriptorPool;
}

vk::DescriptorSet ShaderInstance::getNativeDescriptorSetAt(const uint32_t frame) const {
    return _descriptorSets[frame];
}

const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()> & ShaderInstance::getNativeDescriptorSets() const {
    return _descriptorSets;
}
