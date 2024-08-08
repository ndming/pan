#include "engine/Sampler.h"
#include "engine/Engine.h"


Sampler::Builder & Sampler::Builder::filter(const Filter magnifiedFilter, const Filter minifiedFilter) {
    _magFilter = getFilter(magnifiedFilter);
    _minFilter = getFilter(minifiedFilter);
    return *this;
}

Sampler::Builder& Sampler::Builder::wrapMode(const WrapMode modeU, const WrapMode modeV, const WrapMode modeW) {
    _addressModeU = getAddressMode(modeU);
    _addressModeV = getAddressMode(modeV);
    _addressModeW = getAddressMode(modeW);
    return *this;
}

Sampler::Builder& Sampler::Builder::anisotropyEnabled(const bool enabled) {
    _anisotropyEnabled = enabled;
    return *this;
}

Sampler::Builder& Sampler::Builder::maxAnisotropy(const float value) {
    _maxAnisotropy = value;
    return *this;
}

Sampler::Builder& Sampler::Builder::borderColor(const BorderColor color) {
    _borderColor = getBorderColor(color);
    return *this;
}

std::unique_ptr<Sampler> Sampler::Builder::build(const Engine& engine) {
    const auto device = engine.getNativeDevice();

    // Validate sampler anisotropy options
    if (const auto feature = engine.getEngineFeature(); !feature.samplerAnisotropy && _anisotropyEnabled) {
        PLOGW << "Using sampler anisotropy without having enabled it: "
                 "enable this feature via EngineFeature during Engine creation";
        _anisotropyEnabled = vk::False;
    }
    if (const auto anisotropyLimit = engine.getLimitMaxSamplerAnisotropy();  _maxAnisotropy > anisotropyLimit) {
        PLOGW << "Using a maximum sampler anisotropy value that exceeds the limit: " << anisotropyLimit;
        _maxAnisotropy = anisotropyLimit;
    }

    auto samplerInfo = vk::SamplerCreateInfo{};
    // How to interpolate texels that are magnified or minified, magnification concerns oversampling and
    // minification concerns undersampling
    samplerInfo.magFilter = _magFilter;
    samplerInfo.minFilter = _minFilter;
    // The axes are called U, V and W instead of X, Y and Z
    samplerInfo.addressModeU = _addressModeU;
    samplerInfo.addressModeV = _addressModeV;
    samplerInfo.addressModeW = _addressModeW;
    // The maxAnisotropy field limits the amount of texel samples that can be used to calculate the final color.
    // A lower value results in better performance, but lower quality results. A value of 1.0f disables this effect.
    samplerInfo.anisotropyEnable = _anisotropyEnabled;
    samplerInfo.maxAnisotropy = _maxAnisotropy;
    // Which color is returned when sampling beyond the image with clamp to border addressing mode
    samplerInfo.borderColor = _borderColor;
    // TODO: add support for mipmap
    samplerInfo.mipmapMode = _mipmapMode;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    // Values that are uncommon or will be suppored in the future
    samplerInfo.unnormalizedCoordinates = vk::False;
    samplerInfo.compareEnable = vk::False;
    samplerInfo.compareOp = vk::CompareOp::eAlways;

    const auto sampler = device.createSampler(samplerInfo);
    return std::unique_ptr<Sampler>(new Sampler{ sampler });
}

vk::Filter Sampler::Builder::getFilter(const Filter filer) {
    switch (filer) {
        case Filter::Linear:  return vk::Filter::eLinear;
        case Filter::Nearest: return vk::Filter::eNearest;
        default: throw std::runtime_error("Unrecognized sampler filter");
    }
}

vk::SamplerAddressMode Sampler::Builder::getAddressMode(const WrapMode mode) {
    switch (mode) {
        case WrapMode::Repeat:              return vk::SamplerAddressMode::eRepeat;
        case WrapMode::MirroredRepeat:      return vk::SamplerAddressMode::eMirroredRepeat;
        case WrapMode::ClampToEdge:         return vk::SamplerAddressMode::eClampToEdge;
        case WrapMode::ClampToBorder:       return vk::SamplerAddressMode::eClampToBorder;
        case WrapMode::MirroredClampToEdge: return vk::SamplerAddressMode::eMirrorClampToEdge;
        default: throw std::runtime_error("Unrecognized sampler wrap mode");
    }
}

vk::BorderColor Sampler::Builder::getBorderColor(const BorderColor color) {
    switch (color) {
        case BorderColor::IntOpaqueBlack:        return vk::BorderColor::eIntOpaqueBlack;
        case BorderColor::FloatOpaqueBlack:      return vk::BorderColor::eFloatOpaqueBlack;
        case BorderColor::IntOpaqueWhite:        return vk::BorderColor::eIntOpaqueWhite;
        case BorderColor::FloatOpaqueWhite:      return vk::BorderColor::eFloatOpaqueWhite;
        case BorderColor::IntTransparentBlack:   return vk::BorderColor::eIntTransparentBlack;
        case BorderColor::FloatTransparentBlack: return vk::BorderColor::eFloatTransparentBlack;
        default: throw std::runtime_error("Unrecognized sampler border color");
    }
}

vk::SamplerMipmapMode Sampler::Builder::getMipmapMode(const MipmapMode mode) {
    switch (mode) {
        case MipmapMode::Linear: return vk::SamplerMipmapMode::eLinear;
        case MipmapMode::Nearest: return vk::SamplerMipmapMode::eNearest;
        default: throw std::runtime_error("Unsupported sampler mipmap mode");
    }
}

vk::Sampler Sampler::getNativeSampler() const {
    return _sampler;
}
