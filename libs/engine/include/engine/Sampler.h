#pragma once

#include <vulkan/vulkan.hpp>

#include <memory>


class Engine;


class Sampler final {
public:
    enum class Filter {
        Linear,
        Nearest,
    };

    enum class WrapMode {
        Repeat,
        MirroredRepeat,
        ClampToBorder,
        ClampToEdge,
        MirroredClampToEdge,
    };

    enum class BorderColor {
        IntOpaqueBlack,
        FloatOpaqueBlack,
        IntOpaqueWhite,
        FloatOpaqueWhite,
        IntTransparentBlack,
        FloatTransparentBlack,
    };

    enum class MipmapMode {
        Linear,
        Nearest,
    };

    class Builder {
    public:
        Builder& filter(Filter magnifiedFilter, Filter minifiedFilter);

        Builder& wrapMode(WrapMode modeU, WrapMode modeV, WrapMode modeW);

        Builder& anisotropyEnabled(bool enabled);
        Builder& maxAnisotropy(float value);

        Builder& borderColor(BorderColor color);

        [[nodiscard]] std::unique_ptr<Sampler> build(const Engine& engine);

    private:
        [[nodiscard]] static vk::Filter getFilter(Filter filer);
        [[nodiscard]] static vk::SamplerAddressMode getAddressMode(WrapMode mode);
        [[nodiscard]] static vk::BorderColor getBorderColor(BorderColor color);
        [[nodiscard]] static vk::SamplerMipmapMode getMipmapMode(MipmapMode mode);

        vk::Filter _magFilter{ vk::Filter::eLinear };
        vk::Filter _minFilter{ vk::Filter::eLinear };

        vk::SamplerAddressMode _addressModeU{ vk::SamplerAddressMode::eRepeat };
        vk::SamplerAddressMode _addressModeV{ vk::SamplerAddressMode::eRepeat };
        vk::SamplerAddressMode _addressModeW{ vk::SamplerAddressMode::eRepeat };

        vk::Bool32 _anisotropyEnabled{ vk::False };
        float _maxAnisotropy{ 1.0f };

        vk::BorderColor _borderColor{ vk::BorderColor::eIntOpaqueBlack };

        vk::SamplerMipmapMode _mipmapMode{ vk::SamplerMipmapMode::eLinear };
        float _mipLoddBias{ 0.0f };
        float _minLod{ 0.0f };
        float _maxLod{ 0.0f };
    };

    [[nodiscard]] vk::Sampler getNativeSampler() const;

    Sampler(const Sampler&) = delete;
    Sampler& operator=(const Sampler&) = delete;

private:
    explicit Sampler(const vk::Sampler& sampler) : _sampler{ sampler } {
    }

    vk::Sampler _sampler;
};
