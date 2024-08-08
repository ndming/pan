#include "engine/ShaderInstance.h"
#include "engine/Engine.h"
#include "engine/UniformBuffer.h"
#include "engine/Texture.h"
#include "engine/Sampler.h"


ShaderInstance::ShaderInstance(
    const Shader* const shader,
    const vk::DescriptorPool& descriptorPool,
    const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& descriptorSets
) : _shader{ shader },
    _descriptorPool{ descriptorPool },
    _descriptorSets{ descriptorSets } {
}

void ShaderInstance::setDescriptor(
    const uint32_t binding,
    const UniformBuffer* const uniformBuffer,
    const Engine& engine
) const {
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

void ShaderInstance::setDescriptor(
    const uint32_t binding,
    const std::shared_ptr<Texture>& texture,
    const std::unique_ptr<Sampler>& sampler,
    const Engine& engine
) const {
    const auto device = engine.getNativeDevice();

    for (uint32_t i = 0; i < Renderer::getMaxFramesInFlight(); ++i) {
        const auto imageInfo = vk::DescriptorImageInfo{
            sampler->getNativeSampler(), texture->getNativeImageView(), vk::ImageLayout::eShaderReadOnlyOptimal };

        const auto descriptorWrites = std::array{
            vk::WriteDescriptorSet{ _descriptorSets[i], binding, 0, 1, vk::DescriptorType::eCombinedImageSampler, &imageInfo },
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

const vk::DescriptorSet& ShaderInstance::getNativeDescriptorSetAt(const uint32_t frame) const {
    return _descriptorSets[frame];
}

const std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>& ShaderInstance::getNativeDescriptorSets() const {
    return _descriptorSets;
}
