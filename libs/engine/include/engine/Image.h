#pragma once

#include <vulkan/vulkan.hpp>

#include <plog/Log.h>


class Engine;


class Image {
public:
    [[nodiscard]] vk::Image getNativeImage() const;
    [[nodiscard]] vk::ImageView getNativeImageView() const;
    [[nodiscard]] void* getAllocation() const;

    virtual ~Image() = default;

    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

protected:
    Image(const vk::Image& image, const vk::ImageView& imageView, void* allocation);

    void transitionImageLayout(
        vk::ImageLayout oldLayout,
        vk::ImageLayout newLayout,
        vk::PipelineStageFlags shaderReadStages,
        const Engine& engine) const;

    void copyBufferToImage(const vk::Buffer& buffer, const vk::Extent3D& extent, const Engine& engine) const;

private:
    [[nodiscard]] static vk::CommandBuffer beginSingleTimeCommands(const Engine& engine);
    static void endSingleTimeCommands(const vk::CommandBuffer &commandBuffer, const Engine& engine);

    vk::Image _image;
    vk::ImageView _imageView;

    void* _allocation;
};
