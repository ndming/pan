#pragma once

#include "engine/Buffer.h"

#include <vulkan/vulkan.hpp>


class Engine;

class UniformBuffer final : public Buffer {
public:
    class Builder {
    public:
        Builder& bufferByteSize(std::size_t size);

        [[nodiscard]] UniformBuffer* build(const Engine& engine) const;

    private:
        std::size_t _bufferSize{ 0 };
    };

    void setBufferData(uint32_t frameIndex, const void* data) const;
    void setBufferData(const void* data) const;

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

private:
    UniformBuffer(vk::DeviceSize bufferSize, const vk::Buffer& buffer, void* allocation, std::byte* pMappedData);

    // This holds the size the uniform buffer for each in-flight frame, not the actual native buffer size.
    // The offset to the buffer for frame i should be calculated as i * _bufferSize.
    vk::DeviceSize _bufferSize;
};
