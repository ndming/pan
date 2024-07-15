#include <array>
#include <fstream>

#include <plog/Log.h>

#include "Translator.h"


vk::PhysicalDeviceFeatures Translator::toPhysicalDeviceFeatures(const std::vector<DeviceFeature>& features) {
    auto deviceFeatures = vk::PhysicalDeviceFeatures{};

    for (const auto& feature : features) {
        switch (feature) {
            case DeviceFeature::SamplerAnisotropy:
                deviceFeatures.samplerAnisotropy = vk::True;
            case DeviceFeature::SampleRateShading:
                deviceFeatures.sampleRateShading = vk::True;
        }
    }

    return deviceFeatures;
}

vk::SampleCountFlagBits Translator::toSupportSampleCount(const SwapChain::MSAA msaa, const vk::PhysicalDevice& device) {
    const auto maxSampleCount = getMaxUsableSampleCount(device);
    if (static_cast<int>(toSampleCount(msaa)) <= static_cast<int>(maxSampleCount)) {
        return toSampleCount(msaa);
    }
    using enum vk::SampleCountFlagBits;
    switch (maxSampleCount) {
        case e2: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x2 MSAA"; return e2;
        case e4: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x4 MSAA"; return e4;
        case e8: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x8 MSAA"; return e8;
        case e16: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x16 MSAA"; return e16;
        case e32: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x32 MSAA"; return e32;
        case e64: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x64 MSAA"; return e64;
        default: PLOGW << "Falling back MSAA configuration: your hardware only supports up to x1 MSAA"; return e1;
    }

}

vk::DescriptorSetLayout Translator::toDrawingDescriptorSetLayout(const Renderer::Pipeline pipeline, const vk::Device& device) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle:
    default:
        return {};
    }
}

vk::DescriptorSetLayout Translator::toComputeDescriptorSetLayout(const Renderer::Pipeline pipeline, const vk::Device& device) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle: {
        static constexpr auto computeLayoutBindings = std::array{
            vk::DescriptorSetLayoutBinding{ 0, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
            vk::DescriptorSetLayoutBinding{ 1, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute },
        };
        constexpr auto computeLayoutInfo = vk::DescriptorSetLayoutCreateInfo{
                {}, static_cast<uint32_t>(computeLayoutBindings.size()), computeLayoutBindings.data() };
        return device.createDescriptorSetLayout(computeLayoutInfo);
    }
    default:
        return {};
    }
}

vk::PipelineLayout Translator::toDrawingPipelineLayout(
    const Renderer::Pipeline pipeline,
    const vk::DescriptorSetLayout& layout,
    const vk::Device &device
) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle:
        return device.createPipelineLayout({});
    default:
        return {};
    }
}

vk::PipelineLayout Translator::toComputePipelineLayout(
    const Renderer::Pipeline pipeline,
    const vk::DescriptorSetLayout& layout,
    const vk::Device &device
) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle: {
        constexpr auto pushConstantRange = vk::PushConstantRange{ vk::ShaderStageFlagBits::eCompute, 0, sizeof(ParticleParam) };
        return device.createPipelineLayout({ {}, 1, &layout, 1, &pushConstantRange });
    }
    default:
        return {};
    }
}

vk::Pipeline Translator::toDrawingPipeline(
    const Renderer::Pipeline pipeline,
    const vk::PipelineLayout& layout,
    const vk::RenderPass& renderPass,
    const vk::SampleCountFlagBits msaaSamples,
    const vk::Device& device
) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle:
        return createParticleDrawingPipeline(layout, renderPass, msaaSamples, device);
    default:
        return {};
    }
}

vk::Pipeline Translator::toComputePipeline(
    const Renderer::Pipeline pipeline,
    const vk::PipelineLayout& layout,
    const vk::Device& device
) {
    using enum Renderer::Pipeline;
    switch (pipeline) {
    case Particle:
        return createParticleComputePipeline(layout, device);
    default:
        return {};
    }
}

vk::SampleCountFlagBits Translator::getMaxUsableSampleCount(const vk::PhysicalDevice& device) {
    const auto physicalDeviceProperties = device.getProperties();
    const auto counts = physicalDeviceProperties.limits.framebufferColorSampleCounts &
        physicalDeviceProperties.limits.framebufferDepthSampleCounts;
    if (counts & vk::SampleCountFlagBits::e64) { return vk::SampleCountFlagBits::e64; }
    if (counts & vk::SampleCountFlagBits::e32) { return vk::SampleCountFlagBits::e32; }
    if (counts & vk::SampleCountFlagBits::e16) { return vk::SampleCountFlagBits::e16; }
    if (counts & vk::SampleCountFlagBits::e8) { return vk::SampleCountFlagBits::e8; }
    if (counts & vk::SampleCountFlagBits::e4) { return vk::SampleCountFlagBits::e4; }
    if (counts & vk::SampleCountFlagBits::e2) { return vk::SampleCountFlagBits::e2; }
    return vk::SampleCountFlagBits::e1;
}

vk::SampleCountFlagBits Translator::toSampleCount(const SwapChain::MSAA msaa) {
    using enum SwapChain::MSAA;
    switch (msaa) {
        case x2: return vk::SampleCountFlagBits::e2;
        case x4: return vk::SampleCountFlagBits::e4;
        case x8: return vk::SampleCountFlagBits::e8;
        case x16: return vk::SampleCountFlagBits::e16;
        case x32: return vk::SampleCountFlagBits::e32;
        case x64: return vk::SampleCountFlagBits::e64;
        default: return vk::SampleCountFlagBits::e1;
    }
}

vk::Pipeline Translator::createParticleDrawingPipeline(
    const vk::PipelineLayout& layout,
    const vk::RenderPass& renderPass,
    const vk::SampleCountFlagBits msaaSamples,
    const vk::Device& device
) {
    // Load shader files and create shader modules
    const auto vertShaderCode = readFile("shaders/particle.vert.spv");
    const auto fragShaderCode = readFile("shaders/particle.frag.spv");
    const auto vertShaderModule = createShaderModule(vertShaderCode, device);
    const auto fragShaderModule = createShaderModule(fragShaderCode, device);

    // Assign shaders to graphics pipeline stages
    const auto shaderStages = std::array{
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eVertex, vertShaderModule, "main", nullptr },
        vk::PipelineShaderStageCreateInfo{ {}, vk::ShaderStageFlagBits::eFragment, fragShaderModule, "main", nullptr },
    };

    // Describe the format of the vertex data that will be passed to the vertex shader
    static constexpr auto bindingDesriptions = getParticleBindingDescriptions();
    static constexpr auto attributeDescriptions = getParticleAttributeDescriptions();
    constexpr auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo{
        {}, static_cast<uint32_t>(bindingDesriptions.size()), bindingDesriptions.data(),
        static_cast<uint32_t>(attributeDescriptions.size()), attributeDescriptions.data() };

    // Specify which properties will be able to change at runtime
    static constexpr auto dynamicStates = std::array{ vk::DynamicState::eViewport, vk::DynamicState::eScissor };
    constexpr auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo{
        {}, static_cast<uint32_t>(dynamicStates.size()), dynamicStates.data() };

    constexpr auto viewportState = vk::PipelineViewportStateCreateInfo{ {}, 1, {}, 1, {} };

    // Describe what kind of geometry will be drawn from the vertices and if primitive restart should be enabled
    constexpr auto inputAssembly = vk::PipelineInputAssemblyStateCreateInfo{ {}, vk::PrimitiveTopology::ePointList, vk::False };

    // Configure the rasterizer
    constexpr auto rasterizer = vk::PipelineRasterizationStateCreateInfo{
        {}, vk::False, vk::False, vk::PolygonMode::eFill, vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise,
        vk::False, 0.0f, 0.0f, 0.0f, 1.0f };

    // Configure multisampling to perform anti-aliasing
    const auto multisampling = vk::PipelineMultisampleStateCreateInfo{
        {}, msaaSamples, vk::False, .2f, nullptr, vk::False, vk::False };

    // Configure color blending per attached framebuffer
    static constexpr auto colorBlendAttachment = vk::PipelineColorBlendAttachmentState{
        vk::True, vk::BlendFactor::eSrcAlpha, vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendOp::eAdd,
        vk::BlendFactor::eOneMinusSrcAlpha, vk::BlendFactor::eZero,  vk::BlendOp::eAdd,
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA };
    // Configure global color blending
    constexpr auto colorBlending = vk::PipelineColorBlendStateCreateInfo{
        {}, vk::False, vk::LogicOp::eCopy, 1, &colorBlendAttachment, { 0.0f, 0.0f, 0.0f, 0.0f } };

    // Enable depth test
    constexpr auto depthStencil = vk::PipelineDepthStencilStateCreateInfo{ {}, vk::False, vk::False };

    // Construct the pipeline
    const auto pipelineInfo = vk::GraphicsPipelineCreateInfo{
        {}, static_cast<uint32_t>(shaderStages.size()), shaderStages.data(), &vertexInputInfo, &inputAssembly,
        {}, &viewportState, &rasterizer, &multisampling, &depthStencil, &colorBlending, &dynamicStateInfo,
        layout, renderPass, 0, nullptr, -1 };
    const auto pipeline = device.createGraphicsPipeline(nullptr, pipelineInfo).value;

    device.destroyShaderModule(fragShaderModule);
    device.destroyShaderModule(vertShaderModule);

    return pipeline;
}

vk::Pipeline Translator::createParticleComputePipeline(const vk::PipelineLayout& layout, const vk::Device& device) {
    const auto computeShaderCode = readFile("shaders/particle.comp.spv");
    const auto computeShaderModule = createShaderModule(computeShaderCode, device);
    const auto computeShaderStage =  vk::PipelineShaderStageCreateInfo{
                {}, vk::ShaderStageFlagBits::eCompute, computeShaderModule, "main", nullptr };

    const auto pipelineInfo = vk::ComputePipelineCreateInfo{ {}, computeShaderStage, layout };
    const auto pipeline = device.createComputePipelines(nullptr, pipelineInfo).value[0];

    device.destroyShaderModule(computeShaderModule);
    return pipeline;
}

std::vector<char> Translator::readFile(const std::string_view fileName) {
    auto file = std::ifstream{ fileName.data(), std::ios::ate | std::ios::binary };
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file!");
    }

    const auto fileSize = static_cast<size_t>(file.tellg());
    auto buffer = std::vector<char>(fileSize);

    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

    file.close();
    return buffer;
}

vk::ShaderModule Translator::createShaderModule(const std::vector<char>& bytecode, const vk::Device& device) {
    const auto shaderInfo = vk::ShaderModuleCreateInfo{
            {}, bytecode.size(), reinterpret_cast<const uint32_t*>(bytecode.data()) };
    return device.createShaderModule(shaderInfo);
}
