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
        [[nodiscard]] bool checkPushConstantSizeLimit(uint32_t psLimit) const;
        [[nodiscard]] bool checkPushConstantValidity() const;

        std::vector<char> _vertShaderCode{};
        std::string _vertShaderEntryPoint{};

        std::vector<char> _fragShaderCode{};
        std::string _fragShaderEntryPoint{};

        float _minSampleShading{ .0f };

    };

    GraphicShader() = delete;
};
