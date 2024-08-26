#pragma once

#include "engine/Shader.h"


class ComputeShader final : public Shader {
public:
    class Builder final : public Shader::Builder<Builder> {
    public:
        Builder& computeShader(const std::filesystem::path& path, std::string entryPoint = "main");

        [[nodiscard]] Shader* build(const Engine& engine);

    private:
        std::vector<char> _shaderCode{};
        std::string _shaderEntryPoint{};
    };

    ComputeShader() = delete;
};
