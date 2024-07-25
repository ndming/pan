#pragma once

#include <vulkan/vulkan.hpp>


class Engine;

class Buffer {
public:
    [[nodiscard]] vk::Buffer getNativeBuffer() const;
    [[nodiscard]] void* getAllocation() const;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    virtual ~Buffer() = default;

protected:
    Buffer(std::size_t bufferSize, vk::BufferUsageFlags usage, const Engine& engine);

    void transferBufferData(std::size_t bufferSize, const void* data, vk::DeviceSize offset, const Engine& engine) const;

private:
    static [[nodiscard]] vk::CommandBuffer beginSingleTimeTransferCommands(const Engine& engine);
    static void endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer, const Engine& engine);

    vk::Buffer _buffer;
    void* _allocation;
};
