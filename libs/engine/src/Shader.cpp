#include "engine/Shader.h"
#include "engine/Engine.h"

#include <ranges>
#include <unordered_map>


Shader::Shader(
    const vk::DescriptorSetLayout& descriptorSetLayout,
    const vk::PipelineLayout& pipelineLayout,
    const vk::Pipeline& pipeline,
    std::vector<vk::DescriptorSetLayoutBinding>&& descriptorBindings,
    std::vector<vk::PushConstantRange>&& pushConstants
) : _descriptorSetLayout{ descriptorSetLayout },
    _pipelineLayout{ pipelineLayout },
    _pipeline{ pipeline },
    _descriptorBindings{ std::move(descriptorBindings) },
    _pushConstants{ std::move(pushConstants) } {
}

ShaderInstance* Shader::createInstance(const Engine& engine) const {
    const auto device = engine.getNativeDevice();

    // Count how many descriptor of each type in this pipeline
    auto descriptorTypeNumbers = std::unordered_map<vk::DescriptorType, uint32_t>{};
    for (const auto binding : _descriptorBindings) {
        descriptorTypeNumbers[binding.descriptorType] += binding.descriptorCount;
    }

    // Create a descriptor pool
    auto poolSizes = std::vector<vk::DescriptorPoolSize>{};
    for (const auto& [descriptorType, count] : descriptorTypeNumbers) {
        poolSizes.emplace_back(descriptorType, static_cast<uint32_t>(Renderer::getMaxFramesInFlight()) * count);
    }
    // The structure has an optional flag similar to command pools that determines if individual descriptor sets can be
    // freed or not: VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT. We’re not going to touch the descriptor set
    // after creating it, so we don’t need this flag
    const auto poolInfo = vk::DescriptorPoolCreateInfo{
        {}, static_cast<uint32_t>(Renderer::getMaxFramesInFlight()), static_cast<uint32_t>(poolSizes.size()), poolSizes.data() };
    const auto descriptorPool = device.createDescriptorPool(poolInfo);

    // Allocate a descriptor set for each in-flight frame
    const auto layouts = std::vector(Renderer::getMaxFramesInFlight(), _descriptorSetLayout);
    const auto allocInfo = vk::DescriptorSetAllocateInfo{
        descriptorPool, static_cast<uint32_t>(Renderer::getMaxFramesInFlight()), layouts.data() };
    const auto descriptorSetVector = device.allocateDescriptorSets(allocInfo);

    auto descriptorSets = std::array<vk::DescriptorSet, Renderer::getMaxFramesInFlight()>{};
    std::ranges::copy_n(descriptorSetVector.begin(), Renderer::getMaxFramesInFlight(), descriptorSets.begin());

    return new ShaderInstance{ this, descriptorPool, _pushConstants.size(), descriptorSets };
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

const std::vector<vk::PushConstantRange> & Shader::getNativePushConstantRanges() const {
    return _pushConstants;
}
