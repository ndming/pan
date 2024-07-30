#pragma once

#include "engine/Shader.h"

#include <memory>


class Engine;
class SwapChain;

class GraphicShader final : public Shader {
public:
    class Builder final : public Shader::Builder<Builder> {
    public:
        Builder& vertexShader(const std::filesystem::path& path, std::string entryPoint = "main");
        Builder& fragmentShader(const std::filesystem::path& path, std::string entryPoint = "main");

        Builder& minSampleShading(float sample);

        [[nodiscard]] std::unique_ptr<Shader> build(const Engine& engine, const SwapChain& swapChain);

    private:
        std::vector<char> _vertexShaderCode{};
        std::string _vertexShaderEntryPoint{};

        std::vector<char> _fragmentShaderCode{};
        std::string _fragmentShaderEntryPoint{};

        float _minSampleShading{ .0f };

    };

private:
    GraphicShader(
        const vk::DescriptorSetLayout& descriptorSetLayout,
        const vk::PipelineLayout& pipelineLayout,
        const vk::Pipeline& pipeline);
};
