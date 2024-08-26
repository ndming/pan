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

    /**
     * Updates the portion of buffer content associated with the frame index, guranteed to complete prior to the next
     * draw call.
     *
     * This operation shall be used to safely update the buffer content inside the render loop. The Renderer::render
     * function accepts an optional lambda as its last parameter. This lambda provides the a frame index value which
     * can be fed directly to this function.
     *
     * @param frameIndex The frame index provided by the callback of Renderer::render.
     * @param data The new data to update this buffer, whose size conforms to the data byte size specified during
     * buffer creation.
     */
    void setData(uint32_t frameIndex, const void* data) const;

    /**
     * Updates the buffer content. The transfer is guaranteed to complete prior to the next draw call.
     *
     * Note that this operation must NOT be called inside the main render loop. Uniform buffers are frame-dependent
     * resources, thus under the hood, a UniformBuffer conceptually holds more than a single native buffer object, each
     * dedicated to a certain in-flight frame. Because this function updates all the internal buffers at once, the
     * content of some native buffer will change while its associated frame might be consuming it, causing artifacts.
     *
     * To safely update the buffer content in the main render loop, use the overload of this function that accepts
     * a frame index parameter. Such an overload only updates the native buffer associated with the specified frame,
     * avoiding race conditions.
     *
     * @param data The new data to update this buffer, whose size conforms to the data byte size specified during
     * buffer creation.
     */
    void setData(const void* data) const;

    [[nodiscard]] std::size_t getBufferSize() const;
    [[nodiscard]] std::size_t getDataSize() const;

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;

private:
    UniformBuffer(std::size_t bufferSize, std::size_t dataSize, const vk::Buffer& buffer, void* allocation, std::byte* pMappedData);

    // This holds the size of the uniform buffer for each in-flight frame, not the actual native buffer size.
    // The offset to the buffer for frame ith should be calculated as i * _bufferSize.
    std::size_t _bufferSize;

    // This holds the data size requested for each uniform buffer in each in-flight frame, which may not be as large
    // _bufferSize because of alignment requirements.
    std::size_t _dataSize;
};
