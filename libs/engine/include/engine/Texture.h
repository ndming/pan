#pragma once

#include "engine/Image.h"
#include "engine/Shader.h"

#include <cstdint>
#include <memory>


class Engine;


class Texture final : public Image {
public:
    enum class Sampler {
        Sampler2D,
    };

    enum class Format {
        R8_sRGB,
        R8G8_sRGB,
        R8G8B8_sRGB,
        R8G8B8A8_sRGB,
    };

    class Builder {
    public:
        Builder& width(uint32_t pixels);
        Builder& height(uint32_t pixels);

        Builder& mipLevels(uint32_t levels);
        Builder& format(Format format);

        Builder& shaderStages(std::initializer_list<Shader::Stage> stages);

        [[nodiscard]] std::shared_ptr<Texture> build(const Engine& engine) const;

    private:
        uint32_t _width{ 0 };
        uint32_t _height{ 0 };

        uint32_t _mipLevels{ 1 };

        vk::Format _format{ vk::Format::eUndefined };
        uint32_t _channelCount{ 0 };

        vk::PipelineStageFlags _shaderStages{ vk::PipelineStageFlagBits::eNone };

        [[nodiscard]] static vk::Format getFormat(Format format);
        [[nodiscard]] static uint32_t getChannelCount(Format format);
    };

    void setData(const void* data, const Engine& engine) const;

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    Texture(
        std::size_t imageSize, uint32_t width, uint32_t height, vk::PipelineStageFlags stages,
        const vk::Image& image, const vk::ImageView& imageView, void* allocation);

private:
    std::size_t _imageSize;

    uint32_t _width;
    uint32_t _height;

    vk::PipelineStageFlags _shaderStages;
};
