#include "engine/ShaderInstance.h"
#include "engine/Engine.h"
#include "engine/UniformBuffer.h"

#include <plog/Log.h>


ShaderInstance::ShaderInstance(
    const Shader* const shader,
    const vk::DescriptorPool& descriptorPool,
    const std::size_t pushConstantCount,
    const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& descriptorSets
) : _shader{ shader },
    _descriptorPool{ descriptorPool },
    _pushConstantValues(pushConstantCount, nullptr),
    _descriptorSets{ descriptorSets } {
}

void ShaderInstance::setPushConstantData(const uint32_t index, const void* const data) {
    if (index > _pushConstantValues.size()) {
        PLOGE << "Received out-of-bound push constant index: " << index;
        throw std::invalid_argument("Index of push constant data is out of bound");
    }
    _pushConstantValues[index] = data;
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
