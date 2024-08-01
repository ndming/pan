#pragma once

#include "engine/Buffer.h"

#include <vulkan/vulkan.hpp>


class Engine;

class UniformBuffer final : public Buffer {
public:
    class Builder {
    public:
        Builder& dataByteSize(std::size_t size);

        [[nodiscard]] UniformBuffer* build(const Engine& engine) const;

    private:
        std::size_t _dataSize{ 0 };
    };

    void setBufferData(uint32_t frameIndex, const void* data) const;
    void setBufferData(const void* data) const;

    [[nodiscard]] std::size_t getBufferSize() const;
    [[nodiscard]] std::size_t getDataSize() const;

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

private:
    UniformBuffer(std::size_t bufferSize, std::size_t dataSize, const vk::Buffer& buffer, void* allocation, std::byte* pMappedData);

    // This holds the size of the uniform buffer for each in-flight frame, not the actual native buffer size.
    // The offset to the buffer for frame i should be calculated as i * _bufferSize.
    std::size_t _bufferSize;

    // This holds the data size requested for each uniform buffer in each in-flight frame, which may not be as large
    // _bufferSize because of alignment requirements.
    std::size_t _dataSize;
};
