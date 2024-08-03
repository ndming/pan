#include "engine/GraphicShader.h"
#include "engine/Engine.h"
#include "engine/SwapChain.h"

#include <glm/glm.hpp>
#include <plog/Log.h>

#include <ranges>
#include <unordered_set>


GraphicShader::Builder& GraphicShader::Builder::vertexShader(const std::filesystem::path& path, std::string entryPoint) {
    _vertShaderCode = readShaderFile(path);
    _vertShaderEntryPoint = std::move(entryPoint);
    return *this;
}

GraphicShader::Builder& GraphicShader::Builder::fragmentShader(const std::filesystem::path& path, std::string entryPoint) {
    _fragShaderCode = readShaderFile(path);
    _fragShaderEntryPoint = std::move(entryPoint);
    return *this;
}

GraphicShader::Builder & GraphicShader::Builder::minSampleShading(const float sample) {
    if (sample < 0.0f || sample > 1.0f) {
        PLOGE << "Received " << sample << ": min sample shading must be in the range [0.0, 1.0]";
        throw std::invalid_argument("Min sample shading must be in the range [0.0, 1.0]");
    }
    _minSampleShading = sample;
    return *this;
}

Shader* GraphicShader::Builder::build(const Engine& engine, const SwapChain& swapChain) {
    const auto device = engine.getNativeDevice();

    // Add the pre-defined push constant range for our camera and transform components to the vertex shader:
    // layout(push_constant, std430) uniform MVP {
    //     mat4 cameraMat;
    //     mat4 transform;
    // } mvp;
    pushConstantRange(vk::ShaderStageFlagBits::eVertex, 0, sizeof(glm::mat4) * 2);

    // Ensure that the specified push constants are within the device's limit
    if (const auto psLimit = engine.getLimitPushConstantSize(); !checkPushConstantSizeLimit(psLimit)) {
        PLOGE << "Detected a push constant range whose size exceeds " << psLimit << " bytes";
        throw std::runtime_error("Push constant range (offset + size) must be less than the allowed limit");
    }

    // Each shader stage should only have one push constant block
    if (!checkPushConstantValidity()) {
        PLOGE << "Detected multiple push constant ranges in a shader stage";
        throw std::runtime_error("Each shader stage is allowed to only have one push constant range");
    }

    // Descriptor set layout and pipeline layout
    const auto descriptorSetLayout = device.createDescriptorSetLayout(
        { {}, static_cast<uint32_t>(_descriptorBindings.size()), _descriptorBindings.data() });
    const auto pipelineLayout = device.createPipelineLayout(
        { {}, 1, &descriptorSetLayout, static_cast<uint32_t>(_pushConstantRanges.size()), _pushConstantRanges.data() });

    // A graphics pipeline must have a vertex shader and a fragment shader
    if (_vertShaderCode.empty() || _fragShaderCode.empty()) {
        PLOGE << "Creating a graphics pipeline with empty vertex/fragment shader";
        throw std::runtime_error("A graphics pipeline must have a vertex shader and a fragment shader");
    }

    // Shader modules and shader stages
    const auto vertShaderModule = device.createShaderModule(
        { {}, _vertShaderCode.size(), reinterpret_cast<const uint32_t*>(_vertShaderCode.data()) });
    const auto fragShaderModule = device.createShaderModule(
        { {}, _fragShaderCode.size(), reinterpret_cast<const uint32_t*>(_fragShaderCode.data()) });
    const auto shaderStages = std::array{
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, _vertShaderEntryPoint.data(), nullptr },
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, _fragShaderEntryPoint.data(), nullptr },
    };

    // These properties by default must be able to change at runtime
    using enum vk::DynamicState;
    auto dynamicStates = std::vector{
        eVertexInputEXT,            // vk::PipelineVertexInputStateCreateInfo
        eViewport,                  // vk::PipelineViewportStateCreateInfo
        eScissor,                   // ...
        ePrimitiveTopology,         // vk::PipelineInputAssemblyStateCreateInfo
        ePrimitiveRestartEnable,    // ...
        ePolygonModeEXT,            // vk::PipelineRasterizationStateCreateInfo
        eCullMode,                  // ...
        eFrontFace,                 // ...
        eLineWidth,
        // TODO: add support for depth test
        // eDepthTestEnable,           // vk::PipelineDepthStencilStateCreateInfo
        // eDepthWriteEnable,          // ...
        // eDepthCompareOp,            // ...
        // eDepthBoundsTestEnable,     // ...
        // eStencilTestEnable,         // ...
        // eStencilOp,                 // ...
        // eDepthBounds,               // ...
    };

    // These values will be able to configure at runtime
    constexpr auto vetexInputState = vk::PipelineVertexInputStateCreateInfo{};
    constexpr auto viewportState = vk::PipelineViewportStateCreateInfo{ {}, 1, {}, 1, {} };
    constexpr auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};

    // While others depend on the requested features
    const auto [sampleShading] = engine.getEngineFeature();

    // Rasterization
    auto rasterizer = vk::PipelineRasterizationStateCreateInfo{};
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthClampEnable = vk::False;

    // Multisampling options
    if (!sampleShading && _minSampleShading > 0.0f) {
        PLOGW << "Using min sample shading without having enabled it: "
                 "enable this feature via EngineFeature during Engine creation";
    }
    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        {}, swapChain.getNativeSampleCount(), sampleShading, _minSampleShading, nullptr, vk::False, vk::False };

    // TODO: enable depth test
    constexpr auto depthStencil = vk::PipelineDepthStencilStateCreateInfo{  {}, vk::False, vk::False };

    // TODO: add suport for color blending
    static constexpr auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{ vk::False };
    // Configure global color blending
    constexpr auto colorBlending = vk::PipelineColorBlendStateCreateInfo{ {}, vk::False, {}, 1, &colorBlendAttachment };

    // Construct the pipeline
    const auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo{
        {}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data() };
    const auto pipelineInfo = vk::GraphicsPipelineCreateInfo{
        {}, static_cast<uint32_t>(shaderStages.size()), shaderStages.data(), &vetexInputState, &inputAssembly,
        {}, &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicStateInfo,
        pipelineLayout, swapChain.getNativeRenderPass(), 0, nullptr, -1 };
    const auto pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;

    // We no longer need the shader modules once the pipeline is created
    device.destroyShaderModule(fragShaderModule);
    device.destroyShaderModule(vertShaderModule);

    return buildShader(descriptorSetLayout, pipelineLayout, pipeline);
}

bool GraphicShader::Builder::checkPushConstantSizeLimit(const uint32_t psLimit) const {
    const auto withinLimit = [&psLimit](const vk::PushConstantRange& range) {
        return range.offset + range.size <= psLimit;
    };
    return std::ranges::all_of(_pushConstantRanges, withinLimit);
}

template<> struct std::hash<vk::ShaderStageFlags> {
    size_t operator()(const vk::ShaderStageFlags& stageFlags) const noexcept {
        return std::hash<VkFlags>()(static_cast<VkFlags>(stageFlags));
    }
};

bool operator==(const vk::ShaderStageFlags& lhs, const vk::ShaderStageFlags& rhs) {
    return static_cast<VkFlags>(lhs) == static_cast<VkFlags>(rhs);
}

bool GraphicShader::Builder::checkPushConstantValidity() const {
    auto seenFlags = std::unordered_set<vk::ShaderStageFlags>{};
    for (const auto& range : _pushConstantRanges) {
        if (seenFlags.contains(range.stageFlags)) {
            // Duplicate found
            return false;
        }
        seenFlags.insert(range.stageFlags);
    }
    return true;
}
