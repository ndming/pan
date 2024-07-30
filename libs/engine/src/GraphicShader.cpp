#include "engine/GraphicShader.h"
#include "engine/Engine.h"
#include "engine/SwapChain.h"

#include <plog/Log.h>

#include <array>


GraphicShader::Builder& GraphicShader::Builder::vertexShader(const std::filesystem::path& path, std::string entryPoint) {
    _vertexShaderCode = readShaderFile(path);
    _vertexShaderEntryPoint = std::move(entryPoint);
    return *this;
}

GraphicShader::Builder& GraphicShader::Builder::fragmentShader(const std::filesystem::path& path, std::string entryPoint) {
    _fragmentShaderCode = readShaderFile(path);
    _fragmentShaderEntryPoint = std::move(entryPoint);
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

std::unique_ptr<Shader> GraphicShader::Builder::build(const Engine& engine, const SwapChain& swapChain) {
    const auto device = engine.getNativeDevice();

    // Descriptor set layout and pipeline layout
    const auto descriptorSetLayout = device.createDescriptorSetLayout(
        { {}, static_cast<uint32_t>(_descriptorBindings.size()), _descriptorBindings.data() });
    const auto pipelineLayout = device.createPipelineLayout(
        { {}, 1, &descriptorSetLayout, static_cast<uint32_t>(_pushConstants.size()), _pushConstants.data() });

    // Shader modules and shader stages
    const auto vertShaderModule = device.createShaderModule(
        { {}, _vertexShaderCode.size(), reinterpret_cast<const uint32_t*>(_vertexShaderCode.data()) });
    const auto fragShaderModule = device.createShaderModule(
        { {}, _fragmentShaderCode.size(), reinterpret_cast<const uint32_t*>(_fragmentShaderCode.data()) });
    const auto shaderStages = std::array{
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, _vertexShaderEntryPoint.data(), nullptr },
        vk::PipelineShaderStageCreateInfo{
            {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, _fragmentShaderEntryPoint.data(), nullptr },
    };

    // These properties by default must be able to change at runtime
    using enum vk::DynamicState;
    auto dynamicStates = std::vector{
        eVertexInputEXT,            // vk::PipelineVertexInputStateCreateInfo
        eViewport,                  // vk::PipelineViewportStateCreateInfo
        eScissor,                   // ...
        ePrimitiveTopology,         // vk::PipelineInputAssemblyStateCreateInfo
        ePolygonModeEXT,            // vk::PipelineRasterizationStateCreateInfo
        eCullMode,                  // ...
        eFrontFace,                 // ...
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
    const auto [depthClamp, msaa, largePoints, sampleShading, wideLines] = engine.getEngineFeature();

    // Rasterization
    auto rasterizer = vk::PipelineRasterizationStateCreateInfo{};
    rasterizer.lineWidth = 1.0f;
    if (depthClamp) {
        dynamicStates.push_back(eDepthClampEnableEXT);
        rasterizer.depthClampEnable = vk::True;
    }
    if (wideLines) {
        dynamicStates.push_back(eLineWidth);
    }

    // Multisampling options
    if (msaa) {
        dynamicStates.push_back(eRasterizationSamplesEXT);
    }
    if (!sampleShading && _minSampleShading > 0.0f) {
        PLOGW << "Using min sample shading without having enabled it: "
                 "enable this feature via EngineFeature during Engine creation";
    }
    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        {}, msaa ? vk::SampleCountFlagBits::e1 : swapChain.getNativeSampleCount(),
        sampleShading, _minSampleShading, nullptr, vk::False, vk::False };

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

    return std::unique_ptr<Shader>(new GraphicShader(descriptorSetLayout, pipelineLayout, pipeline));
}

GraphicShader::GraphicShader(
    const vk::DescriptorSetLayout& descriptorSetLayout,
    const vk::PipelineLayout& pipelineLayout,
    const vk::Pipeline& pipeline
) : Shader{ descriptorSetLayout, pipelineLayout, pipeline } {
}
