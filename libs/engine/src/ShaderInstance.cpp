#include "engine/ShaderInstance.h"

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

void ShaderInstance::setPushConstantData(const uint32_t index, void* const data) {
    if (index > _pushConstantValues.size()) {
        PLOGE << "Received out-of-bound push constant index: " << index;
        throw std::invalid_argument("Index of push constant data is out of bound");
    }
    _pushConstantValues[index] = data;
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
