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
    const auto bindingFlagInfo = vk::DescriptorSetLayoutBindingFlagsCreateInfo{
        static_cast<uint32_t>(_descriptorBindingFlags.size()), _descriptorBindingFlags.data() };
    const auto descriptorSetLayout = device.createDescriptorSetLayout(
        { {}, static_cast<uint32_t>(_descriptorBindings.size()), _descriptorBindings.data(), &bindingFlagInfo });
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
        eLineWidth,                 // ...
    };

    // These values will be able to configure at runtime
    constexpr auto vertexInputState = vk::PipelineVertexInputStateCreateInfo{};
    constexpr auto viewportState = vk::PipelineViewportStateCreateInfo{ {}, 1, {}, 1, {} };
    constexpr auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{};

    // While others depend on the requested features
    const auto feature = engine.getEngineFeature();

    // Rasterization
    auto rasterizer = vk::PipelineRasterizationStateCreateInfo{};
    rasterizer.lineWidth = 1.0f;
    rasterizer.depthClampEnable = vk::False;

    // Multisampling options
    if (!feature.sampleShading && _minSampleShading > 0.0f) {
        PLOGW << "Using min sample shading without having enabled it: "
                 "enable this feature via EngineFeature during Engine creation";
    }
    auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        {}, swapChain.getNativeSampleCount(), feature.sampleShading, _minSampleShading, nullptr, vk::False, vk::False };
    multisampling.rasterizationSamples = swapChain.getNativeSampleCount();
    multisampling.sampleShadingEnable = feature.sampleShading;
    multisampling.minSampleShading = _minSampleShading;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = vk::False;
    multisampling.alphaToOneEnable = vk::False;

    // Depth stencil options
    auto depthStencil = vk::PipelineDepthStencilStateCreateInfo{};
    depthStencil.depthTestEnable = vk::True;
    depthStencil.depthWriteEnable = vk::True;
    depthStencil.depthCompareOp = vk::CompareOp::eLess;
    // Optional depth bound test
    depthStencil.depthBoundsTestEnable = vk::False;
    depthStencil.minDepthBounds = 0.0f;
    depthStencil.maxDepthBounds = 1.0f;
    // Optional stencil test
    depthStencil.stencilTestEnable = vk::False;
    depthStencil.front = vk::StencilOp::eZero;
    depthStencil.back = vk::StencilOp::eZero;

    // TODO: add support for color blending
    auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{};
    colorBlendAttachment.blendEnable = vk::False;
    colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR |
                                          vk::ColorComponentFlagBits::eG |
                                          vk::ColorComponentFlagBits::eB |
                                          vk::ColorComponentFlagBits::eA;
    // Configure global color blending
    const auto colorBlending = vk::PipelineColorBlendStateCreateInfo{
        {}, vk::False, {}, 1, &colorBlendAttachment };

    const auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo{
        {}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data() };

    // Construct the pipeline
    auto pipelineInfo = vk::GraphicsPipelineCreateInfo{};
    // The shaders
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    // Fixed and dynamic states
    pipelineInfo.pVertexInputState = &vertexInputState;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicStateInfo;
    // The pipeline layout is a Vulkan handle rather than a struct pointer
    pipelineInfo.layout = pipelineLayout;
    // Reference to the render pass and the index of the sub pass where this graphics pipeline will be used
    // It is also possible to use other render passes with this pipeline instead of this specific instance,
    // but they have to be compatible with this very specific renderPass
    pipelineInfo.renderPass = swapChain.getNativeRenderPass();
    pipelineInfo.subpass = 0;  // right now we only have the one (and only) subpass
    // It's possible to create a new graphics pipeline by deriving from an existing pipeline. The idea of pipeline
    // derivatives is that it is less expensive to set up pipelines when they have much functionality in common with
    // an existing pipeline and switching between pipelines from the same parent can also be done quicker. We can
    // specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about
    // to be created by index with basePipelineIndex. These values are only used if the eDerivative flag is also
    // specified in the flags field.
    pipelineInfo.basePipelineHandle = nullptr;
    pipelineInfo.basePipelineIndex = -1;

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
