#include "engine/Shader.h"


Shader::Shader(
    const vk::DescriptorSetLayout& descriptorSetLayout,
    const vk::PipelineLayout& pipelineLayout,
    const vk::Pipeline& pipeline
) : _descriptorSetLayout{ descriptorSetLayout },
    _pipelineLayout{ pipelineLayout },
    _pipeline{ pipeline } {
}

vk::DescriptorSetLayout Shader::getNativeDescriptorSetLayout() const {
    return _descriptorSetLayout;
}

vk::PipelineLayout Shader::getNativePipelineLayout() const {
    return _pipelineLayout;
}

vk::Pipeline Shader::getNativePipeline() const {
    return _pipeline;
}
