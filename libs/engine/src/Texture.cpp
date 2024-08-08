#include "engine/Texture.h"
#include "engine/Engine.h"

#include "allocator/ResourceAllocator.h"


Texture::Texture(
    const std::size_t imageSize,
    const uint32_t width,
    const uint32_t height,
    const vk::PipelineStageFlags stages,
    const vk::Image& image,
    const vk::ImageView& imageView,
    void* const allocation
) : Image{ image, imageView, allocation },
    _imageSize{ imageSize },
    _width{ width },
    _height{ height },
    _shaderStages{ stages } {
}

Texture::Builder & Texture::Builder::width(const uint32_t pixels) {
    _width = pixels;
    return *this;
}

Texture::Builder & Texture::Builder::height(const uint32_t pixels) {
    _height = pixels;
    return *this;
}

Texture::Builder & Texture::Builder::mipLevels(const uint32_t levels) {
    _mipLevels = levels;
    return *this;
}

Texture::Builder & Texture::Builder::format(const Format format) {
    _format = getFormat(format);
    _channelCount = getChannelCount(format);
    return *this;
}

Texture::Builder& Texture::Builder::shaderStages(const std::initializer_list<Shader::Stage> stages) {
    vk::PipelineStageFlags shaderStages;
    for (const auto stage : stages) {
        switch (stage) {
            case Shader::Stage::Vertex:   shaderStages |= vk::PipelineStageFlagBits::eVertexShader; break;
            case Shader::Stage::Fragment: shaderStages |= vk::PipelineStageFlagBits::eFragmentShader; break;
            case Shader::Stage::Compute:  shaderStages |= vk::PipelineStageFlagBits::eComputeShader; break;
            default: throw std::runtime_error("Unrecognized shader stage");
        }
    }

    _shaderStages = shaderStages;
    return *this;
}

std::shared_ptr<Texture> Texture::Builder::build(const Engine& engine) const {
    const auto allocator = engine.getResourceAllocator();
    const auto device = engine.getNativeDevice();
    // A texture is a 2D image
    constexpr auto type = vk::ImageType::e2D;
    // The purpose of a texture is most likely for being sampled from the shader. The image is going to be used as
    // destination for the buffer copy, so it should be set up as a transfer destination.
    constexpr auto usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled;
    // We will be using a staging buffer instead of a staging image, so linear tiling won’t be necessary
    constexpr auto tiling = vk::ImageTiling::eOptimal;
    // Multisampling is only applicable for color attachment images
    constexpr auto samples = vk::SampleCountFlagBits::e1;
    // TODO: add support for mipmap generation
    constexpr auto mipLevels = 1;

    // Create a dedicated image
    auto allocation = VmaAllocation{};
    const auto image = allocator->allocateDedicatedImage(
        _width, _height, 1, mipLevels, samples, type, _format, tiling, usage, &allocation);

    // Create an image view
    constexpr auto aspectFlags = vk::ImageAspectFlagBits::eColor;
    const auto viewInfo = vk::ImageViewCreateInfo{
        {}, image, vk::ImageViewType::e2D, _format, {}, { aspectFlags, /* base mip level */ 0, mipLevels, 0, 1 } };
    const auto imageView = device.createImageView(viewInfo);

    const auto imageSize = _width * _height * _channelCount;
    return std::make_shared<Texture>(imageSize, _width, _height, _shaderStages, image, imageView, allocation);
}

vk::Format Texture::Builder::getFormat(const Format format) {
    switch (format) {
        case Format::R8_sRGB:       return vk::Format::eR8Srgb;
        case Format::R8G8_sRGB:     return vk::Format::eR8G8Srgb;
        case Format::R8G8B8_sRGB:   return vk::Format::eR8G8B8Srgb;
        case Format::R8G8B8A8_sRGB: return vk::Format::eR8G8B8A8Srgb;
        default: throw std::runtime_error("Unsupported texture format");
    }
}

uint32_t Texture::Builder::getChannelCount(const Format format) {
    switch (format) {
        case Format::R8_sRGB:       return 1;
        case Format::R8G8_sRGB:     return 2;
        case Format::R8G8B8_sRGB:   return 3;
        case Format::R8G8B8A8_sRGB: return 4;
        default: throw std::runtime_error("Unsupported texture format");
    }
}

void Texture::setData(const void* const data, const Engine& engine) const {
    const auto allocator = engine.getResourceAllocator();

    // Vulkan allows us to copy pixels from a VkBuffer to an image and the API for this is actually faster on some
    // hardware. Thus, we will load the data into a staging buffer and then copy it to the image
    auto stagingAllocation = VmaAllocation{};
    const auto stagingBuffer = allocator->allocateStagingBuffer(_imageSize, &stagingAllocation);
    allocator->mapAndCopyData(_imageSize, data, stagingAllocation);

    // Transition the image layout to a transfer destination, make the transfer, and then transition it
    // to the layout optimal for shader sampling
    using enum vk::ImageLayout;
    transitionImageLayout(eUndefined, eTransferDstOptimal, _shaderStages, engine);
    copyBufferToImage(stagingBuffer, { _width, _height, 1 }, engine);
    transitionImageLayout(eTransferDstOptimal, eShaderReadOnlyOptimal, _shaderStages, engine);

    allocator->destroyBuffer(stagingBuffer, stagingAllocation);
}
