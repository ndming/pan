#pragma once

#include <vulkan/vulkan.hpp>

#include <cstddef>


class Engine;

class Buffer {
public:
    [[nodiscard]] vk::Buffer getNativeBuffer() const;
    [[nodiscard]] void* getAllocation() const;

    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    virtual ~Buffer() = default;

protected:
    Buffer(const vk::Buffer& buffer, void* allocation, std::byte* pMappedData = nullptr);

    void transferBufferData(std::size_t bufferSize, const void* data, vk::DeviceSize offset, const Engine& engine) const;

    std::byte* _pMappedData;

private:
    [[nodiscard]] static vk::CommandBuffer beginSingleTimeTransferCommands(const Engine& engine);
    static void endSingleTimeTransferCommands(const vk::CommandBuffer &commandBuffer, const Engine& engine);

    vk::Buffer _buffer;
    void* _allocation;
};
