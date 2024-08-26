#include "engine/ComputeShader.h"


ComputeShader::Builder& ComputeShader::Builder::computeShader(const std::filesystem::path& path, std::string entryPoint) {
    _shaderCode = readShaderFile(path);
    _shaderEntryPoint = std::move(entryPoint);
    return *this;
}

Shader* ComputeShader::Builder::build(const Engine& engine) {
    return nullptr;
}
