#pragma once

#include <vulkan/vulkan.hpp>
#include <plog/Log.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>


class Engine;


enum class DescriptorType {
    UniformBuffer,
    StorageBuffer,
    Sampler,
    CombinedImageSampler,
};


class Shader {
public:
    enum class Stage {
        Vertex,
        Fragment,
        Compute,
    };

    template <typename T>
    class Builder {
    public:
        T& descriptorCount(const int descriptorCount) {
            if (descriptorCount <= 0) {
                PLOGE << "Received non-positive descriptor count: " << descriptorCount;
                throw std::invalid_argument("Descriptor count is non-positive");
            }
            _descriptorBindings.resize(descriptorCount);
            return *static_cast<T*>(this);
        }

        T& descriptor(const uint32_t binding, const DescriptorType type, const uint32_t count, const Stage stage) {
            _descriptorBindings[binding] = { binding, getDescriptorType(type), count, getShaderStage(stage) };
            return *static_cast<T*>(this);
        }

        T& pushConstant(const Stage stage, const uint32_t byteSize) {
            const auto offset = _pushConstants.empty() ? 0 : _pushConstants.back().offset + _pushConstants.back().size;
            _pushConstants.emplace_back(getShaderStage(stage), offset, byteSize);

            return *static_cast<T*>(this);
        }

        virtual ~Builder() = default;

    protected:
        [[nodiscard]] static std::vector<char> readShaderFile(const std::filesystem::path& path) {
            // Reading from the end of the file as binary format
            auto file = std::ifstream{ path.string() + ".spv", std::ios::ate | std::ios::binary };
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file!");
            }

            // The advantage of starting to read at the end of the file is that we can use the read position to
            // determine the size of the file and allocate a buffer
            const auto fileSize = static_cast<size_t>(file.tellg());
            auto buffer = std::vector<char>(fileSize);

            // Seek back to the beginning of the file and read all the bytes at once
            file.seekg(0);
            file.read(buffer.data(), static_cast<std::streamsize>(fileSize));

            file.close();
            return buffer;
        }

        std::vector<vk::DescriptorSetLayoutBinding> _descriptorBindings{};
        std::vector<vk::PushConstantRange> _pushConstants{};

    private:
        static vk::DescriptorType getDescriptorType(const DescriptorType type) {
            using enum DescriptorType;
            switch (type) {
                case UniformBuffer:        return vk::DescriptorType::eUniformBuffer;
                case StorageBuffer:        return vk::DescriptorType::eStorageBuffer;
                case Sampler:              return vk::DescriptorType::eSampler;
                case CombinedImageSampler: return vk::DescriptorType::eCombinedImageSampler;
                default: throw std::invalid_argument("Unsupported descriptor type");
            }
        }

        static vk::ShaderStageFlags getShaderStage(const Stage stage) {
            switch (stage) {
                case Stage::Vertex:   return vk::ShaderStageFlagBits::eVertex;
                case Stage::Fragment: return vk::ShaderStageFlagBits::eFragment;
                case Stage::Compute:  return vk::ShaderStageFlagBits::eCompute;
                default: throw std::invalid_argument("Unsupported shader stage");
            }
        }
    };

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    virtual ~Shader() = default;

    [[nodiscard]] vk::DescriptorSetLayout getNativeDescriptorSetLayout() const;
    [[nodiscard]] vk::PipelineLayout getNativePipelineLayout() const;
    [[nodiscard]] vk::Pipeline getNativePipeline() const;

protected:
    Shader(
        const vk::DescriptorSetLayout& descriptorSetLayout,
        const vk::PipelineLayout& pipelineLayout,
        const vk::Pipeline& pipeline);

private:
    vk::DescriptorSetLayout _descriptorSetLayout{};
    vk::PipelineLayout _pipelineLayout{};
    vk::Pipeline _pipeline{};
};
