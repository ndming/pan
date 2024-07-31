#pragma once

#include "engine/Buffer.h"

#include <cstdint>


class Engine;

enum class IndexType {
    Uint16,
    Uint32,
};

class IndexBuffer final : public Buffer {
public:
    class Builder {
    public:
        Builder& indexCount(int count);
        Builder& indexType(IndexType type);

        [[nodiscard]] IndexBuffer* build(const Engine& engine) const;

    private:
        static std::size_t getSize(IndexType type);
        static vk::IndexType getIndexType(IndexType type);

        int _indexCount{ 0 };
        IndexType _indexType{ IndexType::Uint16 };
    };

    void setData(const void* data, const Engine& engine) const;

    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

private:
    IndexBuffer(
        uint32_t indexCount,
        vk::IndexType indexType,
        std::size_t bufferSize,
        const vk::Buffer& buffer,
        void* allocation);

    uint32_t _indexCount;
    vk::IndexType _indexType;
    std::size_t _bufferSize;
};
