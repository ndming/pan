#pragma once

#include "engine/Buffer.h"


class StorageBuffer final : public Buffer {
public:
    class Builder {
    public:
        Builder& byteSize(std::size_t size);

        [[nodiscard]] StorageBuffer* build(const Engine& engine) const;

    private:
        std::size_t _bufferSize{ 0 };
    };

    void setData(const void* data, const Engine& engine) const;

    [[nodiscard]] uint32_t getBufferSize() const;

private:
    StorageBuffer(std::size_t bufferSize, const vk::Buffer& buffer, void* allocation);

    std::size_t _bufferSize;
};
